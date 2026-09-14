param([string]$OutputDirectory='output/BuildToTarget1/capture-tests')
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
. "$root/tools/capture_force_servo.ps1" -LibraryOnly -BuildToTarget
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
        Build=(Defaults @($schema.build_profile.PSObject.Properties | ForEach-Object { [pscustomobject]@{name=$_.Name;default=$_.Value} }));Version=0;Digest=0;Stages=@{};Ack=@{};PlanReads=0;Armed=$false;Starts=0;Stops=0;Framed=0;Committed=$false;
        StartedAt=0L;Running=$false;Frozen=@();Requests=(New-Object Collections.ArrayList);Confirmations=0}
    if ($Failure -eq 'BuildLimit') { $sim.Build.build_ceiling=7001 }
    if ($Failure -eq 'ContinuousHidden') { $sim.Config.press_cap=1800; $sim.Profile.continuous_press=1800 }
    if ($Failure -eq 'GainChanged') { $sim.Config.kp=11 }
    if ($Failure -eq 'SessionChanged') { $sim.Config.session_ms=5000 }
    if ($Failure -eq 'BuildChanged') { $sim.Profile.build_ms=5000 }
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
                $sim.Starts++;$sim.Running=$true;$sim.Armed=$false;$sim.StartedAt=$clock.ElapsedMilliseconds
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
                    profile_id=6;profile_digest=$profileDigest;unit=2;measured_valid=1;state=1;output_off=1;
                    now_ms=$clock.ElapsedMilliseconds;latest_received_ms=$clock.ElapsedMilliseconds;received_ms=$clock.ElapsedMilliseconds;
                    latest_raw=0;raw=0;measured=0;force_N=0;target=$sim.Plan.target_N;plan_version=$sim.Version;plan_digest=$sim.Digest;
                    session=$sim.Starts;session_peak_raw=32;session_peak_measured=32;control_sequence=$sim.Starts;
                    build_mode=1;build_config_digest=$(if ($Failure -eq 'BuildDigest') {0} else {$schema.build_digest});
                    exposure_inhibited=[int]($Failure -eq 'Exposure');build_no_response_ms=$(if ($Failure -eq 'NoResponseBudget') {5000} else {0});
                    segment_request=$sim.Starts;segment_phase=2;segment_command=3000;segment_hard_ms=10;energized_reserved_ms=100;energized_upper_ms=100;
                    cooling_active=[int]($Failure -eq 'Lockout');target_reached=0;run_reason=0}
                if ($sim.Running) { $values.state=13;$values.lease_active=1;$values.output_off=0;$values.current_committed=3000;$values.tim3=600 }
                if ($sim.Starts -and $clock.ElapsedMilliseconds-$sim.StartedAt -ge 6000) {
                    $values.state=$(if ($sim.Running) {16} else {1});$values.target_reached=1;$values.target_reached_ms=$sim.StartedAt+6000
                    $values.run_reason=$(if ($Target -eq 3000) {5} else {6}); $values.build_phase=4
                    $values.measured=[Math]::Max(0,$sim.Plan.target_N-$(if ($clock.ElapsedMilliseconds-$sim.StartedAt -ge 9000) {1} else {0}))
                    $values.force_N=$values.measured;$values.latest_raw=$values.measured;$values.raw=$values.measured
                    $values.output_off=1;$values.lease_active=0;$values.current_committed=0;$values.tim3=0
                }
                if ($Failure -eq 'RunningFault' -and $sim.Starts) { $values.state=9;$values.fault=2;$values.detail=2;$values.run_reason=2;$values.output_off=1;$values.lease_active=0;$values.current_committed=0;$values.tim3=0 }
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
                if ($a -ge 0x600) {
                    $all=@(foreach ($p in $schema.build_profile.PSObject.Properties) {
                        [uint32]$u=$sim.Build.($p.Name); ($u -shr 16); ($u -band 65535)
                    }); $base=0x600
                } elseif ($a -ge 0x540) {
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
            } elseif ($a -eq 0x100) { $words=@($schema.schema,0,50,(2*($schema.u32.Count+$schema.floats.Count)),0,1,($configDigest -shr 16),($configDigest -band 65535)) }
            else { $words=@($sim.Frozen[($a-0x200)..($a-0x200+$n-1)]) }
            [byte[]]$payload=@(1,$f,(2*$n));foreach ($w in $words) { $payload+=@([byte]($w -shr 8),[byte]($w -band 255)) };$response=Add-ModbusCrc $payload
        }
        if (($Failure -eq 'StartEchoTimeout' -and $f -eq 5 -and $a -eq 0x10) -or
            ($Failure -eq 'PlanTimeout' -and $f -eq 3 -and $a -eq 0x540 -and $sim.Committed) -or
            ($Failure -eq 'RunningTimeout' -and $sim.Running -and $clock.ElapsedMilliseconds-$sim.StartedAt -ge 6500)) { $response=[byte[]]$response[0..1] }
        $sim.Framed++
        Read-TestLengthAwareResponse -Response $response -ChunkSizes @(1,1,1,2,3,4) -TimeoutMs $TimeoutMs
    }.GetNewClosure()
    $sleep={param($ms) $clock.ElapsedMilliseconds+=$ms}.GetNewClosure()
    $confirm={param($p) $sim.Confirmations++; return $Failure -ne 'Cancel'}.GetNewClosure()
    $csv=Join-Path $directory 'SYNTHETIC.csv';$errorText=''
    $manualStop={
        if ($sim.Starts -and $clock.ElapsedMilliseconds-$sim.StartedAt -ge 12000) {
            Check ((Get-Item -LiteralPath $csv).Length -gt 0) 'Capture did not preserve CSV during the run'
            $saved=@(Import-Csv -LiteralPath $csv)
            Check (@($saved | Where-Object { $_.phase -eq 'RUN' -and $_.state -eq 16 }).Count -gt 0) 'Capture stopped before target/OFF monitor after5s'
            return $true
        }
        return $false
    }.GetNewClosure()
    try {
        Invoke-ForceCapture -TransportExchange $transport -Watch $clock -SleepMilliseconds $sleep -Mode $Mode `
            -OutputCsv $csv -ActualHash 'SYNTHETIC_NO_HARDWARE' -MaximumSeconds $(if ($Mode -eq 'SingleStart') {0} else {5}) -StopRequested $manualStop -Target ([int]$Target) -TargetN -ProfileId 6 `
            -CharacterizationPlan (New-CharacterizationPlan $Target $Assist $Continuous) -ConfirmStart $confirm `
            -CurrentLimitSetting 'SYNTHETIC 0.5 A PSU label ONLY' -InitialGap 'SYNTHETIC' -RepositoryCommit 'SYNTHETIC' | Out-Null
    } catch { $errorText=$_.Exception.Message }
    $zero=$Mode -eq 'Observe' -or $Failure -in @('BuildLimit','BuildDigest','Exposure','NoResponseBudget','ContinuousHidden','GainChanged','SessionChanged','BuildChanged','PlanMismatch','DigestMismatch','StaleVersion','Lockout','Cancel','PlanTimeout')
    $expected=[int](-not $zero)
    Check ($sim.Starts -eq $expected -and $sim.Stops -eq 1) "$Name START/STOP count, error=$errorText"
    Check ($sim.Framed -eq $sim.Requests.Count) "$Name bypassed production framing"
    $meta=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.metadata.json')) | ConvertFrom-Json
    $report=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.report.txt')) | ConvertFrom-Json
    Check ($report.StopVerified -eq ($Failure -ne 'BuildDigest') -and $meta.start_attempts -eq $expected) "$Name report STOP proof"
    if ($Failure -in @('','Boundary','RunningFault')) { Check (-not $errorText) "$Name unexpected error: $errorText" }
    else { Check ([bool]$errorText) "$Name failed to reject" }
    if ($expected) {
        Check ($meta.runtime_plan.target_N -eq $Target -and $meta.runtime_plan.assist_command -eq (Convert-CharacterizationPercent $Assist) -and
            $meta.runtime_plan.continuous_cap -eq (Convert-CharacterizationPercent $Continuous)) "$Name runtime plan metadata"
        Check ($meta.config.kp -eq 10 -and $meta.config.ki -eq 0 -and $meta.config.kd -eq 0 -and $meta.config.measurement_filter_s -eq 0) 'Field gains/filter changed'
    }
    if ($expected -and -not $Failure) {
        Check ($meta.stop_reason -eq 'OPERATOR_STOP' -and $null -eq $meta.maximum_observation_seconds) 'Unexpected automatic capture stop'
        Check ($meta.stop_sent_pc_ms-$sim.StartedAt -ge 12000) 'Capture still has an overall deadline'
        Check ($meta.capture_end_policy -eq 'OPERATOR_STOP_OR_DEVICE_TERMINAL_OR_FAULT') 'Missing unlimited capture policy'
        Check ($meta.config.session_ms -eq 0 -and $meta.profile.build_ms -eq 0 -and $meta.profile.capture_ms -eq 0) 'Wrong time contract'
        $saved=@(Import-Csv -LiteralPath $csv)
        Check ($saved[0].measured -eq 0 -and $saved[-1].phase -eq 'STOP_READBACK') 'Zero-start or complete streamed capture lost'
    }
    if ($expected -and -not $Failure) {
        Check ($meta.build_profile.build_ceiling -eq 7000 -and $meta.build_config_digest -eq $schema.build_digest) 'Build contract metadata missing'
        Check ($report.target_reached -and $report.off_monitor_samples -gt 1 -and $report.off_monitor_observed_ms -gt 3000) 'OFF monitor event/coverage missing'
        Check ($report.off_monitor_drop_N -eq 1 -and $report.stable_hold -like 'NOT_ESTABLISHED*') 'Target reach falsely labeled stable HOLD'
        Check (@(Import-Csv -LiteralPath $csv | Where-Object { $_.state -eq 14 }).Count -eq 0) 'BuildToTarget entered HOLD'
        Check ($report.mcu_stop_reason -eq $(if ($Target -eq 3000) {'BOUNDARY_TARGET_REACHED'} else {'TARGET_REACHED_OFF'})) 'Persistent target reason lost'
    }
    if ($Failure -eq 'RunningTimeout') {
        Check ($meta.stop_reason -eq 'CAPTURE_ERROR' -and $meta.communication_errors.Count -eq 1) 'Real timeout disguised as observation complete'
        Check ($meta.stop_sent_pc_ms-$sim.StartedAt -ge 6500) 'Late timeout was not exercised'
    }
    if ($Failure -eq 'StartEchoTimeout') { Check ($null -eq $meta.start_accepted) 'Lost echo was falsely labeled rejection' }
    $sim.Requests | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'wire_requests.json') -Encoding UTF8
    Write-Output "BUILD_TO_TARGET_CAPTURE=$Name PASS STARTS=$expected; SYNTHETIC_NO_SERIAL"
}
foreach ($bad in @(@(3001,0,0),@(0,0,0),@(1.5,0,0),@(250,1,0),@(250,0,1),@(250,[double]::NaN,0))) {
    $rejected=$false; try { New-CharacterizationPlan $bad[0] $bad[1] $bad[2] | Out-Null } catch { $rejected=$true }; Check $rejected 'Bad wrapper plan accepted'
}
Capture-Case Observe 250 0 0 '' Observe
foreach ($target in @(1,5,250,500,1000,2000,2999,3000)) { Capture-Case ('Target'+$target) $target 0 0 }
foreach ($failure in @('BuildLimit','BuildDigest','Exposure','NoResponseBudget','ContinuousHidden','GainChanged','SessionChanged','BuildChanged','PlanMismatch','DigestMismatch','StaleVersion','Lockout','Cancel','PlanTimeout','StartEchoTimeout','RunningFault','RunningTimeout')) {
    Capture-Case $failure 500 0 0 $failure
}
# Only literal examples are parsed; no real CLI, port, or powered START.
$readme=Get-Content -Raw -LiteralPath "$root/README.md"
$examples=[regex]::Matches($readme,'(?m)^\.\\tools\\run_force_characterization\.ps1[^\r\n]+')
Check ($examples.Count -eq 1) 'One current Target-only example required'
foreach ($example in $examples) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($example.Value,[ref]$tokens,[ref]$errors)
    Check ($errors.Count -eq 0) 'Invalid documented PowerShell'
    $cmd=$ast.Find({param($n) $n -is [Management.Automation.Language.CommandAst]},$true)
    $values=@{}
    for ($i=1;$i -lt $cmd.CommandElements.Count;$i+=2) {
        $name=$cmd.CommandElements[$i].ParameterName
        Check ($name -in @('Port','TargetForceN')) 'Field example exposes an extra control'
        $values[$name]=$cmd.CommandElements[$i+1].SafeGetValue()
    }
    Check ($values.Port -eq 'COM6' -and $values.Count -eq 2) 'Documented Target-only contract'
    Capture-Case ('Documented'+$values.TargetForceN) $values.TargetForceN 0 0
}
$wrapperTokens=$null;$wrapperErrors=$null
[void][Management.Automation.Language.Parser]::ParseFile("$root/tools/run_force_characterization.ps1",[ref]$wrapperTokens,[ref]$wrapperErrors)
Check ($wrapperErrors.Count -eq 0) 'Invalid field wrapper syntax'
Write-Output 'BUILD_TO_TARGET_CAPTURE_CASES=27 + INVALID_INPUTS=6 PASS; PHYSICAL_NOT_RUN'
