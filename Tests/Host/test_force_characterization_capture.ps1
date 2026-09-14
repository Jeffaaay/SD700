param([string]$OutputDirectory='output/StaticForceRuntimeCharacterization1/capture-tests')
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
. "$root/tools/capture_force_servo.ps1" -LibraryOnly -Characterization
function Check([bool]$Ok,[string]$Message) { if (-not $Ok) { throw $Message } }
function Float-Words($Values,$Fields) {
    foreach ($p in $Fields) {
        [uint32]$u=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$Values.($p.name)),0)
        ($u -shr 16); ($u -band 65535)
    }
}
function Defaults($Fields) {
    $v=[ordered]@{}; foreach ($p in $Fields) { $v[$p.name]=[single]$p.default }; return [pscustomobject]$v
}
function Capture-Case([string]$Name,[double]$Target,[double]$Assist,[double]$Continuous,[string]$Failure='',[string]$Mode='SingleStart') {
    $baseDirectory=if ([IO.Path]::IsPathRooted($OutputDirectory)) {$OutputDirectory} else {Join-Path $root $OutputDirectory}
    $directory=Join-Path $baseDirectory $Name
    Check (-not (Test-Path -LiteralPath $directory)) 'Existing synthetic evidence must not be overwritten'
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $clock=[pscustomobject]@{ElapsedMilliseconds=0L}
    $sim=@{Config=(Defaults $schema.parameters);Profile=(Defaults $schema.profile);Plan=(New-CharacterizationPlan 1 0 0);
        Version=0;Digest=0;Stages=@{};Ack=@{};PlanReads=0;Armed=$false;Starts=0;Stops=0;Framed=0;Committed=$false;
        Running=$false;Frozen=@();Requests=(New-Object Collections.ArrayList);Confirmations=0}
    if ($Failure -eq 'GainChanged') { $sim.Config.kp=11 }
    $transport={param([byte[]]$Request,[int]$TimeoutMs)
        Check (Test-ModbusCrc $Request) 'Bad outbound CRC'
        $f=[int]$Request[1];$a=256*[int]$Request[2]+$Request[3];$n=256*[int]$Request[4]+$Request[5]
        [void]$sim.Requests.Add([pscustomobject]@{function=$f;address=$a;value=$n;at=$clock.ElapsedMilliseconds})
        $clock.ElapsedMilliseconds+=10
        $configDigest=Get-ForceDigest $sim.Config $schema.parameters
        $profileDigest=Get-ForceDigest $sim.Profile $schema.profile
        if ($f -eq 5) {
            if ($a -eq 1 -and $n -eq 0) { $sim.Stops++;$sim.Running=$false;$sim.Armed=$false }
            else {
                Check ($a -eq 0x10 -and $n -eq 0xFF00 -and $sim.Armed -and $sim.Starts -eq 0 -and $sim.Confirmations -eq 1) 'START bypassed complete plan/readback/confirmation or retried'
                $sim.Starts++;$sim.Running=$true;$sim.Armed=$false
            }
            $response=$Request
        } elseif ($f -eq 6) {
            if ($a -eq 0x500) { Check ($n -eq 0xB501) 'Plan BEGIN';$sim.Stages=@{};$sim.Armed=$false }
            elseif ($a -ge 0x510 -and $a -lt 0x51A) { $sim.Stages[$a-0x510]=$n }
            elseif ($a -eq 0x501) {
                Check ($n -eq 0xC501 -and $sim.Stages.Count -eq 10) 'Partial plan was committed'
                $vals=@(); for ($i=0;$i -lt 6;$i+=2) {
                    [uint32]$u=[uint64]$sim.Stages[$i]*65536+$sim.Stages[$i+1]
                    $vals+= [BitConverter]::ToSingle([BitConverter]::GetBytes($u),0)
                }
                $candidate=New-CharacterizationPlan $vals[0] $vals[1] $vals[2]
                $digest=Get-ForceDigest $candidate $characterizationFields
                Check (([uint64]$sim.Stages[6]*65536+$sim.Stages[7]) -eq $sim.Version) 'Stale staged version'
                Check (([uint64]$sim.Stages[8]*65536+$sim.Stages[9]) -eq $digest) 'Wrong staged digest'
                $sim.Plan=$candidate;$sim.Version++;$sim.Digest=$digest;$sim.Committed=$true;$sim.PlanReads=0
                $sim.Profile.peak_press=Convert-CharacterizationPercent $candidate.assist_percent
                $sim.Profile.continuous_press=Convert-CharacterizationPercent $candidate.continuous_percent
                $sim.Config.press_cap=$sim.Profile.continuous_press
            } elseif ($a -ge 0x520 -and $a -lt 0x524) { $sim.Ack[$a-0x520]=$n }
            elseif ($a -eq 0x524) {
                Check ($n -eq 0xA501 -and $sim.PlanReads -eq 0xFFF -and $sim.Ack.Count -eq 4) 'Missing full plan readback before arm'
                Check (([uint64]$sim.Ack[0]*65536+$sim.Ack[1]) -eq $sim.Version -and
                    ([uint64]$sim.Ack[2]*65536+$sim.Ack[3]) -eq $sim.Digest) 'Readback ACK mismatch'
                $sim.Armed=$true
            } elseif ($a -eq 0x102) {
                Check ($n -eq 0xD101) 'Frozen latch magic'
                $values=@{schema=$schema.schema;build_id=$schema.build_id;config_version=($sim.Version+1);config_digest=$configDigest;
                    profile_id=5;profile_digest=$profileDigest;unit=2;measured_valid=1;state=1;output_off=1;
                    now_ms=$clock.ElapsedMilliseconds;latest_received_ms=$clock.ElapsedMilliseconds;received_ms=$clock.ElapsedMilliseconds;
                    latest_raw=30;raw=30;measured=30;force_N=30;target=$sim.Plan.target_N;plan_version=$sim.Version;plan_digest=$sim.Digest;
                    session=$sim.Starts;session_peak_raw=32;session_peak_measured=32;control_sequence=$sim.Starts;
                    cooling_active=[int]($Failure -eq 'Lockout');target_reached=0;run_reason=0}
                if ($sim.Running) { $values.state=13;$values.lease_active=1;$values.output_off=0;$values.current_committed=10;$values.tim3=2 }
                if ($Failure -eq 'RunningFault' -and $sim.Starts) { $values.state=9;$values.fault=2;$values.detail=2;$values.run_reason=2;$values.output_off=1;$values.lease_active=0;$values.current_committed=0;$values.tim3=0 }
                if ($Failure -eq 'Boundary' -and $sim.Starts) {
                    $values.state=1;$values.run_reason=5;$values.target_reached=1;$values.target_reached_ms=1000
                    $values.measured=3000;$values.force_N=3000;$values.latest_raw=3000;$values.raw=3000;$values.session_peak_raw=3000;$values.session_peak_measured=3000
                    $values.output_off=1;$values.lease_active=0;$values.current_committed=0;$values.tim3=0
                }
                $sim.Frozen=@()
                foreach ($name in $schema.u32) { [uint32]$u=if ($values.ContainsKey($name)) {$values[$name]} else {0};$sim.Frozen+=@(($u -shr 16),($u -band 65535)) }
                foreach ($name in $schema.floats) {
                    [single]$v=if ($values.ContainsKey($name)) {$values[$name]} else {0}
                    [uint32]$u=[BitConverter]::ToUInt32([BitConverter]::GetBytes($v),0);$sim.Frozen+=@(($u -shr 16),($u -band 65535))
                }
            } else { throw 'Field workflow attempted generic parameter/profile/target write' }
            $response=$Request
        } else {
            Check ($f -in @(3,4) -and $n -ge 1 -and $n -le 11) 'Read function/chunk bound'
            if ($f -eq 3) {
                if ($a -ge 0x540) {
                    $all=@(Float-Words $sim.Plan $characterizationFields)+@(
                        (Convert-CharacterizationPercent $sim.Plan.assist_percent),(Convert-CharacterizationPercent $sim.Plan.continuous_percent),
                        ($sim.Version -shr 16),($sim.Version -band 65535),($sim.Digest -shr 16),($sim.Digest -band 65535))
                    if ($sim.Committed) {
                        for ($i=$a-0x540;$i -lt $a-0x540+$n;$i++) { $sim.PlanReads=$sim.PlanReads -bor (1 -shl $i) }
                        if ($Failure -eq 'PlanMismatch') { $all[1]=$all[1] -bxor 1 }
                        if ($Failure -eq 'DigestMismatch') { $all[11]=$all[11] -bxor 1 }
                        if ($Failure -eq 'StaleVersion') { $all[9]=0 }
                    }
                    $base=0x540
                } elseif ($a -ge 0x400) { $all=@(foreach ($c in $schema.candidates) { Float-Words $c $schema.profile });$base=0x400 }
                elseif ($a -ge 0x180) { $all=@(Float-Words $sim.Profile $schema.profile);$base=0x180 }
                else { $all=@(Float-Words $sim.Config $schema.parameters);$base=0x110 }
                $words=@($all[($a-$base)..($a-$base+$n-1)])
            } elseif ($a -eq 0x100) { $words=@($schema.schema,0,50,226,0,1,($configDigest -shr 16),($configDigest -band 65535)) }
            else { $words=@($sim.Frozen[($a-0x200)..($a-0x200+$n-1)]) }
            [byte[]]$payload=@(1,$f,(2*$n));foreach ($w in $words) { $payload+=@([byte]($w -shr 8),[byte]($w -band 255)) };$response=Add-ModbusCrc $payload
        }
        if (($Failure -eq 'StartEchoTimeout' -and $f -eq 5 -and $a -eq 0x10) -or
            ($Failure -eq 'PlanTimeout' -and $f -eq 3 -and $a -eq 0x540 -and $sim.Committed)) { $response=[byte[]]$response[0..1] }
        $sim.Framed++
        Read-TestLengthAwareResponse -Response $response -ChunkSizes @(1,1,1,2,3,4) -TimeoutMs $TimeoutMs
    }.GetNewClosure()
    $sleep={param($ms) $clock.ElapsedMilliseconds+=$ms}.GetNewClosure()
    $confirm={param($p) $sim.Confirmations++; return $Failure -ne 'Cancel'}.GetNewClosure()
    $csv=Join-Path $directory 'SYNTHETIC.csv';$errorText=''
    try {
        Invoke-ForceCapture -TransportExchange $transport -Watch $clock -SleepMilliseconds $sleep -Mode $Mode `
            -OutputCsv $csv -ActualHash 'SYNTHETIC_NO_HARDWARE' -MaximumSeconds 5 -Target ([int]$Target) -TargetN -ProfileId 5 `
            -CharacterizationPlan (New-CharacterizationPlan $Target $Assist $Continuous) -ConfirmStart $confirm `
            -CurrentLimitSetting 'SYNTHETIC 0.5 A PSU label ONLY' -InitialGap 'SYNTHETIC' -RepositoryCommit 'SYNTHETIC' | Out-Null
    } catch { $errorText=$_.Exception.Message }
    $zero=$Mode -eq 'Observe' -or $Failure -in @('GainChanged','PlanMismatch','DigestMismatch','StaleVersion','Lockout','Cancel','PlanTimeout')
    $expected=[int](-not $zero)
    Check ($sim.Starts -eq $expected -and $sim.Stops -eq 1) "$Name START/STOP count, error=$errorText"
    Check ($sim.Framed -eq $sim.Requests.Count) "$Name bypassed production framing"
    $meta=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.metadata.json')) | ConvertFrom-Json
    $report=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.report.txt')) | ConvertFrom-Json
    Check ($report.StopVerified -and $meta.start_attempts -eq $expected) "$Name report STOP proof"
    if ($Failure -in @('','Boundary','RunningFault')) { Check (-not $errorText) "$Name unexpected error: $errorText" }
    else { Check ([bool]$errorText) "$Name failed to reject" }
    if ($expected) {
        Check ($meta.runtime_plan.target_N -eq $Target -and $meta.runtime_plan.assist_command -eq (Convert-CharacterizationPercent $Assist) -and
            $meta.runtime_plan.continuous_cap -eq (Convert-CharacterizationPercent $Continuous)) "$Name runtime plan metadata"
        Check ($meta.config.kp -eq 10 -and $meta.config.ki -eq 0 -and $meta.config.kd -eq 0 -and $meta.config.measurement_filter_s -eq 0) 'Field gains/filter changed'
    }
    if ($Failure -eq 'StartEchoTimeout') { Check ($null -eq $meta.start_accepted) 'Lost echo was falsely labeled rejection' }
    $sim.Requests | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'wire_requests.json') -Encoding UTF8
    Write-Output "CHARACTERIZATION_CAPTURE=$Name PASS STARTS=$expected; SYNTHETIC_NO_SERIAL"
}
foreach ($bad in @(@(3001,20,5),@(0,0,0),@(1.5,20,5),@(250,40.1,5),@(250,20,10.1),@(250,[double]::NaN,5))) {
    $rejected=$false; try { New-CharacterizationPlan $bad[0] $bad[1] $bad[2] | Out-Null } catch { $rejected=$true }; Check $rejected 'Bad wrapper plan accepted'
}
Capture-Case Observe 500 30 7.5 '' Observe
Capture-Case Target250 250 20 5
Capture-Case Target500 500 30 7.5
Capture-Case Target1000 1000 30 10
Capture-Case Target650 650 25 8.5
Capture-Case Target2999 2999 35 6
Capture-Case Boundary 3000 40 10 Boundary
Capture-Case Zeros 250 0 0
foreach ($failure in @('GainChanged','PlanMismatch','DigestMismatch','StaleVersion','Lockout','Cancel','PlanTimeout','StartEchoTimeout','RunningFault')) {
    Capture-Case $failure 500 30 7.5 $failure
}
# Parse README literal field examples and run them through the production capture core;
# never invoke the real CLI/SerialPort or bypass the human confirmation in the wrapper.
$readme=Get-Content -Raw -LiteralPath "$root/README.md"
$examples=[regex]::Matches($readme,'(?m)^\.\\tools\\run_force_characterization\.ps1[^\r\n]+')
Check ($examples.Count -eq 4) 'Four documented single-command examples required'
foreach ($example in $examples) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($example.Value,[ref]$tokens,[ref]$errors)
    Check ($errors.Count -eq 0) 'Invalid documented PowerShell'
    $cmd=$ast.Find({param($n) $n -is [Management.Automation.Language.CommandAst]},$true)
    $values=@{}
    for ($i=1;$i -lt $cmd.CommandElements.Count;$i+=2) {
        $name=$cmd.CommandElements[$i].ParameterName
        Check ($name -in @('Port','TargetForceN','AssistPercent','ContinuousPercent')) 'Field example exposes an extra control'
        $values[$name]=$cmd.CommandElements[$i+1].SafeGetValue()
    }
    Check ($values.Port -eq 'COM6' -and $values.Count -eq 4) 'Documented three-input contract'
    Capture-Case ('Documented'+$values.TargetForceN) $values.TargetForceN $values.AssistPercent $values.ContinuousPercent
}
$wrapperTokens=$null;$wrapperErrors=$null
[void][Management.Automation.Language.Parser]::ParseFile("$root/tools/run_force_characterization.ps1",[ref]$wrapperTokens,[ref]$wrapperErrors)
Check ($wrapperErrors.Count -eq 0) 'Invalid field wrapper syntax'
Write-Output 'CHARACTERIZATION_CAPTURE_CASES=21 + INVALID_INPUTS=6 PASS; PHYSICAL_NOT_RUN'
