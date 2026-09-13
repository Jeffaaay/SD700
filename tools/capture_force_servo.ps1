[CmdletBinding()]
param(
    [ValidateSet('Observe','Parameters','SingleStart')][string]$Mode='Observe',
    [switch]$SelfTest,
    [switch]$LibraryOnly,
    [string]$Port,
    [string]$ConfirmedFirmwareSha256,
    [string]$OutputCsv,
    [string]$ParameterFile,
    [ValidateRange(1,65535)][int]$Target=250,
    [switch]$TargetN,
    [int]$ProfileId=1,
    [ValidateRange(1,60)][int]$MaximumSeconds=5,
    [switch]$ConfirmMotorPowerDisconnected,
    [switch]$ConfirmSupervisedMotion,
    [string]$CurrentLimitSetting,
    [string]$InitialGap,
    [string]$FieldNotes
)
$ErrorActionPreference='Stop'
# Import the existing length-aware CRC/RTU transport. Preserve this script's options.
$saved=@{}; foreach ($name in @('Mode','SelfTest','LibraryOnly','Port','ConfirmedFirmwareSha256','OutputCsv','ParameterFile','Target',
    'TargetN','ProfileId','MaximumSeconds','ConfirmMotorPowerDisconnected','ConfirmSupervisedMotion','CurrentLimitSetting','InitialGap','FieldNotes')) {
    $saved[$name]=Get-Variable -Name $name -ValueOnly
}
. "$PSScriptRoot/capture_pressure_response.ps1" -LibraryOnly
foreach ($entry in $saved.GetEnumerator()) { Set-Variable -Name $entry.Key -Value $entry.Value }
$schema=(& python "$PSScriptRoot/force_servo_data.py" --schema | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw 'ForceServo schema unavailable' }

function Read-ForceWords([scriptblock]$Exchange,[int]$Function,[int]$Address,[int]$Count) {
    for ($offset=0;$offset -lt $Count;$offset+=11) {
        $n=[Math]::Min(11,$Count-$offset); $a=$Address+$offset
        [byte[]]$payload=@(1,$Function,($a -shr 8),($a -band 255),0,$n)
        [byte[]]$reply=@(Invoke-ModbusRequest $Exchange $payload $Function 100)
        if ($reply.Length -ne 5+2*$n -or $reply[2] -ne 2*$n) { throw 'Snapshot length mismatch' }
        for ($j=0;$j -lt $n;$j++) { Read-U16BE $reply (3+2*$j) }
    }
}
function Write-ForceWord([scriptblock]$Exchange,[int]$Address,[int]$Value) {
    [byte[]]$payload=@(1,6,($Address -shr 8),($Address -band 255),($Value -shr 8),($Value -band 255))
    [byte[]]$reply=@(Invoke-ModbusRequest $Exchange $payload 6 100)
    if (-not (Test-ByteArraysEqual $reply (Add-ModbusCrc $payload))) { throw 'Write echo mismatch; no retry' }
}
function Read-ForceSnapshot([scriptblock]$Exchange,[string]$Phase) {
    Write-ForceWord $Exchange 0x102 0xD101
    $words=@(Read-ForceWords $Exchange 4 0x200 (2*($schema.u32.Count+$schema.floats.Count)))
    $values=[ordered]@{phase=$Phase;firmware_sha256=$actualHash;pc_elapsed_ms=$watch.ElapsedMilliseconds}
    $i=0
    foreach ($name in $schema.u32) {
        $values[$name]=[uint32]([uint64]$words[$i]*65536+[uint64]$words[$i+1]); $i+=2
    }
    foreach ($name in $schema.floats) {
        [uint32]$bits=[uint64]$words[$i]*65536+[uint64]$words[$i+1]; $i+=2
        $value=[BitConverter]::ToSingle([BitConverter]::GetBytes($bits),0)
        if ([Single]::IsNaN($value) -or [Single]::IsInfinity($value)) { throw 'Non-finite device diagnostic' }
        $values[$name]=$value
    }
    if ($values.schema -ne $schema.schema -or $values.build_id -ne $schema.build_id) { throw 'Snapshot identity mismatch' }
    $values.feedback_gap_ms=$activeConfig.feedback_gap_ms
    $values.hold_enter_units=$activeConfig.hold_enter
    $values.feedback_age_ms=([long]$values.now_ms-[long]$values.latest_received_ms+4294967296) % 4294967296
    $values.at_output_cap=[int]($null -ne $activeConfig -and $values.current_committed -ne 0 -and
        ($values.current_committed -ge $activeConfig.press_cap -or $values.current_committed -le -$activeConfig.release_cap))
    $values.saturated=[int](($values.limits -band 1) -ne 0 -or $values.at_output_cap -ne 0)
    $values.direction=[Math]::Sign($values.current_committed)
    return [pscustomobject]$values
}
function Read-ForceConfig([scriptblock]$Exchange) {
    $words=@(Read-ForceWords $Exchange 3 0x110 (2*$schema.parameters.Count))
    $values=[ordered]@{}; $i=0
    foreach ($p in $schema.parameters) {
        [uint32]$u=[uint64]$words[$i]*65536+[uint64]$words[$i+1]; $i+=2
        $values[$p.name]=[BitConverter]::ToSingle([BitConverter]::GetBytes($u),0)
    }
    $config=[pscustomobject]$values
    Assert-ForceConfig $config
    return $config
}
function Assert-ForceConfig($Config) {
    foreach ($p in $schema.parameters) {
        $v=[single]$Config.($p.name)
        if ($null -eq $Config.($p.name) -or [Single]::IsNaN($v) -or [Single]::IsInfinity($v) -or
            $v -lt $p.minimum -or $v -gt $p.maximum -or
            ($p.name.EndsWith('_ms') -and [Math]::Floor($v) -ne $v)) { throw "Invalid active config: $($p.name)" }
    }
    $c=$Config
    if ($c.integral_min -ge $c.integral_max -or $c.hold_enter -ge $c.hold_exit -or
        $c.control_min_ms -ge $c.feedback_gap_ms -or $c.feedback_gap_ms -ge $c.lease_ms -or
        $c.sample_age_ms -ge $c.lease_ms -or $c.tracking_gain*$c.feedback_gap_ms*0.001 -gt 1 -or
        $c.saturation_ms -gt $c.session_ms -or $c.tracking_ms -gt $c.session_ms) { throw 'Invalid active config relationships' }
}
function Get-ForceDigest($Values,$Fields) {
    [uint64]$h=2166136261
    foreach ($p in $Fields) {
        foreach ($b in [BitConverter]::GetBytes([single]$Values.($p.name))) {
            $h=(($h -bxor [uint64]$b)*[uint64]16777619) -band [uint64]4294967295
        }
    }
    return [uint32]$h
}
function Read-ForceProfile([scriptblock]$Exchange) {
    $words=@(Read-ForceWords $Exchange 3 0x180 (2*$schema.profile.Count))
    $values=[ordered]@{}; $i=0
    foreach ($p in $schema.profile) {
        [uint32]$bits=[uint64]$words[$i]*65536+[uint64]$words[$i+1]; $i+=2
        $value=[BitConverter]::ToSingle([BitConverter]::GetBytes($bits),0)
        if ([Single]::IsNaN($value) -or [Single]::IsInfinity($value) -or $value -ne [single]$p.default) {
            throw "Reviewed operating profile mismatch: $($p.name); no START"
        }
        $values[$p.name]=$value
    }
    return [pscustomobject]$values
}
function Assert-ForceAdmission($Profile,[int]$Target,[bool]$Newtons,[int]$Id,[int]$Seconds) {
    if ($Id -ne $Profile.id) { throw 'Reviewed operating profile ID unavailable; no START' }
    if ($Newtons -or $Profile.unit -eq 1) {
        $reasons=@('MISSING_CONFIRMED_SENSOR_RANGE','MISSING_FORCE_N_CALIBRATION',
            'MISSING_WEAKEST_MECHANICAL_BOUNDARY','MISSING_CURRENT_AND_ON_OFF_TIME_ENVELOPE')
        for ($i=0;$i -lt 4;$i++) {
            if (([int]$Profile.qualifications -band (1 -shl $i)) -eq 0) { throw "$($reasons[$i]); no START" }
        }
    }
    if ($Newtons -ne ($Profile.unit -eq 1)) { throw 'Target unit differs from selected profile; no START' }
    if ($Target -le 0 -or $Target -gt $Profile.operating_max) {
        throw "TARGET_OUTSIDE_OPERATING_RANGE: maximum $($Profile.operating_max) in profile unit $($Profile.unit); no START"
    }
    if ($Newtons -and ($Target -lt $Profile.calibration_min -or $Target -gt $Profile.calibration_max)) {
        throw 'TARGET_OUTSIDE_CALIBRATION_COVERAGE; no START'
    }
    if ($Seconds*1000 -gt $Profile.capture_ms -or $Seconds*1000 -gt $Profile.energized_ms) {
        throw "Selected profile requires MaximumSeconds <= $($Profile.capture_ms/1000); no serial connection or START"
    }
}
function Assert-ForcePlan($Profile,$Config,[int]$Target,[bool]$Newtons,[int]$Id,[int]$Seconds,[single]$Measured) {
    Assert-ForceAdmission $Profile $Target $Newtons $Id $Seconds
    Assert-ForceConfig $Config
    if ($Config.press_cap -gt $Profile.continuous_press -or $Config.release_cap -gt $Profile.release) {
        throw 'Config exceeds selected continuous-output profile; no START'
    }
    if ($Measured -lt 0 -or $Measured -ge $Profile.force_trip) { throw 'Invalid initial measurement; no START' }
    $distance=[Math]::Abs($Target-$Measured)
    $planned=[Math]::Max(1.5*$distance/$Config.reference_rate,[Math]::Sqrt(6*$distance/$Config.reference_acceleration))*1000
    $budget=[Math]::Min($Config.session_ms,[Math]::Min($Profile.session_ms,[Math]::Min($Profile.energized_ms,$Seconds*1000)))
    if ($planned+$Config.hold_dwell_ms -gt $budget -or $planned -gt $Profile.build_ms) {
        throw "TRAJECTORY_HOLD_EXCEEDS_BUDGET: reference=${planned}ms hold=$($Config.hold_dwell_ms)ms budget=${budget}ms; no START"
    }
}
function Invoke-ForceCapture {
    param(
        [Parameter(Mandatory=$true)][scriptblock]$TransportExchange,
        [Parameter(Mandatory=$true)]$Watch,
        [Parameter(Mandatory=$true)][scriptblock]$SleepMilliseconds,
        [scriptblock]$StopRequested={ $false },
        [ValidateSet('Observe','Parameters','SingleStart')][string]$Mode,
        [Parameter(Mandatory=$true)][string]$OutputCsv,
        [Parameter(Mandatory=$true)][string]$ActualHash,
        [ValidateRange(1,60)][int]$MaximumSeconds=5,
        [ValidateRange(1,65535)][int]$Target=250,
    [switch]$TargetN,
    [int]$ProfileId=1,
        $Desired,
        [string]$CurrentLimitSetting,
        [string]$InitialGap,
        [string]$FieldNotes,
        [string]$ConfirmedFirmwareSha256
    )

    $csvPath=[IO.Path]::GetFullPath($OutputCsv)
    $reportPath=[IO.Path]::ChangeExtension($csvPath,'.report.txt')
    $metaPath=[IO.Path]::ChangeExtension($csvPath,'.metadata.json')
    foreach ($p in @($csvPath,$reportPath,$metaPath)) { if (Test-Path -LiteralPath $p) { throw "Output exists: $p" } }
    New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($csvPath)) | Out-Null
    $rows=New-Object Collections.ArrayList; $errorText=''; $startAttempts=0
    $startAccepted=$false; $stopReason='OBSERVATION_COMPLETE'
    $activeConfig=$null; $activeProfile=$null; $plannedReference=$null; $enforceDeadline=$false
    try {
        $exchange={param([byte[]]$Request,[int]$TimeoutMs)
            if ($enforceDeadline) {
                $remaining=$deadline-$watch.ElapsedMilliseconds
                if ($remaining -le 0) { throw 'OBSERVATION_DEADLINE' }
                $TimeoutMs=[int][Math]::Min($TimeoutMs,$remaining)
            }
            & $TransportExchange -Request $Request -TimeoutMs $TimeoutMs
        }
        $info=@(Read-ForceWords $exchange 4 0x100 8)
        if ($info[0] -ne $schema.schema -or $info[2] -ne 2*$schema.parameters.Count -or
            $info[3] -ne 2*($schema.u32.Count+$schema.floats.Count)) { throw 'ForceServo1 capability/schema required' }
        $activeConfig=Read-ForceConfig $exchange
        $activeProfile=Read-ForceProfile $exchange
        $configDigest=Get-ForceDigest $activeConfig $schema.parameters
        $profileDigest=Get-ForceDigest $activeProfile $schema.profile
        if (([uint64]$info[6]*65536+$info[7]) -ne $configDigest) { throw 'Active configuration digest mismatch' }
        $first=Read-ForceSnapshot $exchange 'PREFLIGHT'; [void]$rows.Add($first)
        if ($first.profile_digest -ne $profileDigest -or $first.profile_id -ne $activeProfile.id -or
            $first.unit -ne $activeProfile.unit -or $first.config_digest -ne $configDigest) { throw 'Profile/config diagnostic readback mismatch' }
        if ($first.state -ne 1 -or $first.start_pending -ne 0 -or $first.output_off -ne 1 -or $first.lease_active -ne 0 -or $first.fault -ne 0) { throw 'IDLE + OFF required' }
        if ($Mode -eq 'Parameters') {
            Write-ForceWord $exchange 0x100 0xB101
            $i=0
            foreach ($p in $schema.parameters) {
                [uint32]$bits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$desired.($p.name)),0)
                Write-ForceWord $exchange (0x110+$i) ($bits -shr 16); $i++
                Write-ForceWord $exchange (0x110+$i) ($bits -band 65535); $i++
            }
            Write-ForceWord $exchange 0x101 0xC101
            $activeConfig=Read-ForceConfig $exchange
            foreach ($p in $schema.parameters) { if ([single]$activeConfig.($p.name) -ne [single]$desired.($p.name)) { throw 'Parameter readback mismatch' } }
            $configDigest=Get-ForceDigest $activeConfig $schema.parameters
            $updated=Read-ForceSnapshot $exchange 'CONFIG_READBACK'; [void]$rows.Add($updated)
            if ($updated.config_digest -ne $configDigest -or $updated.profile_digest -ne $profileDigest) { throw 'Updated config/profile digest mismatch' }
        }
        if ($Mode -eq 'SingleStart') {
            if ($info[1] -ne 0 -or $first.locked -ne 0) { throw 'PHYSICAL_OUTPUT_LOCKED: no START sent; hardware qualification pending' }
            if ($first.measured_valid -ne 1) { throw 'Initial calibrated/control measurement unavailable; no START' }
            Assert-ForcePlan $activeProfile $activeConfig $Target $TargetN $ProfileId $MaximumSeconds $first.measured
            $distance=[Math]::Abs($Target-$first.measured)
            $plannedReference=[Math]::Max(1.5*$distance/$activeConfig.reference_rate,[Math]::Sqrt(6*$distance/$activeConfig.reference_acceleration))
            Write-Output "PLANNED_REFERENCE_SECONDS=$plannedReference; HOLD_DWELL_MS=$($activeConfig.hold_dwell_ms); UNIT=$($activeProfile.unit); PROFILE=$ProfileId"
            Write-ForceWord $exchange 0x104 $ProfileId
            $selected=@(Read-ForceWords $exchange 3 0x104 1)
            if ($selected[0] -ne $ProfileId) { throw 'Profile selection readback mismatch; no START' }
            Write-ForceWord $exchange $(if ($TargetN) {0x103} else {0}) $Target
            $ready=Read-ForceSnapshot $exchange 'TARGET_READBACK'
            [void]$rows.Add($ready)
            if ($ready.profile_digest -ne $profileDigest -or $ready.config_digest -ne $configDigest -or $ready.unit -ne $activeProfile.unit -or $ready.target -ne $Target -or $ready.state -ne 1 -or $ready.fault -ne 0 -or $ready.output_off -ne 1) { throw 'Target/state readback mismatch' }
            $deadline=$watch.ElapsedMilliseconds+$MaximumSeconds*1000; $enforceDeadline=$true
            $startAttempts=1 # Set before transmitting. Lost echo never retries START.
            $startAccepted=$null # Unknown if the echo is lost; never infer rejection.
            Send-SingleCoil $exchange 0x10 0xFF00 100
            $startAccepted=$true
        }
        if ($Mode -ne 'SingleStart') { $deadline=$watch.ElapsedMilliseconds+$MaximumSeconds*1000; $enforceDeadline=$true }
        while ($watch.ElapsedMilliseconds -lt $deadline) {
            if (& $StopRequested) { $stopReason='OPERATOR_STOP'; break }
            $row=Read-ForceSnapshot $exchange $(if ($Mode -eq 'SingleStart') {'RUN'} else {'OBSERVE'})
            [void]$rows.Add($row)
            if ($row.fault -ne 0) { $stopReason="DEVICE_FAULT_$($row.fault)_DETAIL_$($row.detail)"; break }
            if ($Mode -eq 'SingleStart' -and $row.state -eq 1 -and $row.start_pending -eq 0) {
                $stopReason='DEVICE_IDLE_AFTER_START'; break
            }
            & $SleepMilliseconds 10
        }
    } catch {
        if ($_.Exception.Message -ne 'OBSERVATION_DEADLINE') {
            $errorText=$_.Exception.Message; $stopReason='CAPTURE_ERROR'
            if ($startAttempts -eq 1 -and $null -eq $startAccepted -and $errorText -like 'Modbus exception:*') { $startAccepted=$false }
        }
    }
    finally {
        $enforceDeadline=$false
        try {
                Send-SingleCoil $exchange 1 0 100
                [void]$rows.Add((Read-ForceSnapshot $exchange 'STOP_READBACK'))
        } catch { $errorText+='; STOP/readback: '+$_.Exception.Message }
    }
    if ($rows.Count -gt 0) { $rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath }
    else { '"phase","firmware_sha256"' | Set-Content -Encoding UTF8 -LiteralPath $csvPath }
    @{mode=$Mode;start_attempts=$startAttempts;start_accepted=$startAccepted;stop_reason=$stopReason;error=$errorText;config=$activeConfig;
        current_limit_setting=$CurrentLimitSetting;initial_gap=$InitialGap;field_notes=$FieldNotes;
        firmware_sha256=$actualHash;operator_flash_attestation=$ConfirmedFirmwareSha256;
        commissioning='StaticForce3000_1';profile=$activeProfile;profile_digest=$profileDigest;config_digest=$configDigest;target=$Target;target_N=[bool]$TargetN;planned_reference_seconds=$plannedReference;qualification='SHORT_SUPERVISED_EXPERIMENT_NOT_CONTINUOUS_RATING';powered_test_ready=$schema.powered_test_ready;physical_test_status='OPERATOR_CAPTURE_UNVALIDATED';
        pwm_counts='tim2/tim3 are PLANNED; no external electrical measurement';
        maximum_observation_seconds=$MaximumSeconds;capture_wall_ms=$watch.ElapsedMilliseconds;
        bus='115200 8N1; frozen snapshot in <=11-register chunks; polling misses are reported'} |
        ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $metaPath
    & python "$PSScriptRoot/force_servo_data.py" --report $csvPath --metadata $metaPath
    if ($LASTEXITCODE -ne 0) { throw 'Report generation failed; raw CSV and metadata preserved' }
    Write-Output "CSV=$csvPath`nREPORT=$reportPath"
    if ($errorText) { throw $errorText }
    $last=$rows[$rows.Count-1]
    if ($last.phase -ne 'STOP_READBACK' -or $last.output_off -ne 1 -or $last.lease_active -ne 0 -or
        $last.tim2 -ne 0 -or $last.tim3 -ne 0 -or $last.current_committed -ne 0 -or $last.state -notin @(1,9)) {
        throw 'STOP_NOT_VERIFIED: inspect report and confirm physical shutdown'
    }

}

if ($SelfTest) {
    & python "$PSScriptRoot/force_servo_data.py" --self-test
    if ($LASTEXITCODE -ne 0) { throw 'Statistics/schema test failed' }
    # Exercise the shared wire path, chunk limits, writes and coherent serialization without a serial port.
    $script:requests=0
    $fake={param([byte[]]$Request,[int]$TimeoutMs)
        $script:requests++
        if ($Request[1] -eq 6) { return $Request }
        $count=$Request[5]; if ($count -gt 11) { throw 'Read exceeded existing RTU bound' }
        $payload=New-Object byte[] (3+2*$count); $payload[0]=1;$payload[1]=$Request[1];$payload[2]=2*$count
        return Add-ModbusCrc $payload
    }
    Write-ForceWord $fake 0x102 0xD101
    $words=@(Read-ForceWords $fake 4 0x200 108)
    if ($words.Count -ne 108 -or $script:requests -ne 11) { throw 'Chunked protocol self-test failed' }
    $actualHash='SYNTHETIC'; $watch=[Diagnostics.Stopwatch]::StartNew()
    $activeConfig=[pscustomobject]@{feedback_gap_ms=40}
    $script:wireWords=@()
    foreach ($name in $schema.u32) {
        [uint32]$v=0
        if ($name -eq 'schema') {$v=$schema.schema}
        if ($name -eq 'build_id') {$v=$schema.build_id}
        if ($name -eq 'control_sequence') {$v=4294967295}
        $script:wireWords+=@(($v -shr 16),($v -band 65535))
    }
    foreach ($name in $schema.floats) {
        [single]$v=0
        if ($name -eq 'control_committed') {$v=-123.5}
        [uint32]$u=[BitConverter]::ToUInt32([BitConverter]::GetBytes($v),0)
        $script:wireWords+=@(($u -shr 16),($u -band 65535))
    }
    $snapshotFake={param([byte[]]$Request,[int]$TimeoutMs)
        if ($Request[1] -eq 6) {return $Request}
        $address=256*[int]$Request[2]+[int]$Request[3]; $n=[int]$Request[5]
        [byte[]]$payload=@(1,4,(2*$n))
        for ($j=0;$j -lt $n;$j++) {
            $w=$script:wireWords[$address-0x200+$j]
            $payload+=@([byte]($w -shr 8),[byte]($w -band 255))
        }
        return Add-ModbusCrc $payload
    }
    $decoded=Read-ForceSnapshot $snapshotFake RUN
    if ($decoded.control_sequence -ne [uint32]::MaxValue -or $decoded.control_committed -ne -123.5) {
        throw 'Snapshot integer/float endian decode failed'
    }
    Write-Output 'FORCE_SERVO_CAPTURE_DECODE=PASS SYNTHETIC; NO_SERIAL_PORT'
    # Exercise production framing and the full Observe runner as part of SelfTest.
    $testOutput='output/ForceServo1_CaptureFix1/selftest-'+[Guid]::NewGuid().ToString('N')
    & "$PSScriptRoot/../Tests/Host/test_force_servo_capture.ps1" -OutputDirectory $testOutput
    return
}
if ($LibraryOnly) { return }
if ($Mode -eq 'SingleStart') {
    $compiledProfile=[pscustomobject]@{}
    foreach ($p in $schema.profile) { $compiledProfile | Add-Member -NotePropertyName $p.name -NotePropertyValue $p.default }
    Assert-ForceAdmission $compiledProfile $Target $TargetN $ProfileId $MaximumSeconds
}
if ($Mode -eq 'SingleStart' -and -not $schema.powered_test_ready) {
    throw 'POWERED_TEST_READY=NO: higher continuous motor/board rating evidence missing; no serial connection or START'
}
if (-not $Port -or -not $OutputCsv) { throw 'Port and new OutputCsv required' }
if ($Mode -ne 'SingleStart' -and -not $ConfirmMotorPowerDisconnected) { throw 'Observe/Parameters requires physically disconnected motor power confirmation' }
if ($Mode -eq 'SingleStart' -and (-not $ConfirmSupervisedMotion -or -not $CurrentLimitSetting -or -not $InitialGap)) {
    throw 'SingleStart requires supervision, permitted load/travel/thermal exposure, external E-stop, actual current limit and initial gap'
}
$firmware=Join-Path $PSScriptRoot '../output/StaticForce3000_1/firmware/SD700_ForceServo1_StaticForce3000_1_RealBench_Release.hex'
$actualHash=(Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash
if ($ConfirmedFirmwareSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $ConfirmedFirmwareSha256 -ine $actualHash) {
    throw 'Operator flash attestation must match the repository HEX; this is not MCU binary verification'
}
& python "$PSScriptRoot/verify_force_servo_firmware.py"
if ($LASTEXITCODE -ne 0) { throw 'Repository firmware verification failed; no serial connection' }
if ($Mode -eq 'Parameters') {
    if (-not $ParameterFile) { throw 'Parameters mode requires a complete JSON file' }
    & python "$PSScriptRoot/force_servo_data.py" --validate $ParameterFile
    if ($LASTEXITCODE -ne 0) { throw 'Invalid parameter group; no serial connection' }
    $desired=Get-Content -Raw -LiteralPath $ParameterFile | ConvertFrom-Json
}
# Check all output names before opening a real port as well as inside the common runner.
$csvPath=[IO.Path]::GetFullPath($OutputCsv)
foreach ($path in @($csvPath,[IO.Path]::ChangeExtension($csvPath,'.report.txt'),[IO.Path]::ChangeExtension($csvPath,'.metadata.json'))) {
    if (Test-Path -LiteralPath $path) { throw "Output exists: $path" }
}
$serial=$null
try {
    $serial=New-Object IO.Ports.SerialPort $Port,115200,None,8,One
    $serial.Handshake=[IO.Ports.Handshake]::None; $serial.ReadTimeout=100; $serial.WriteTimeout=100
    $serial.DtrEnable=$false; $serial.RtsEnable=$false; $serial.Open()
    $transport={param([byte[]]$Request,[int]$TimeoutMs)
        Invoke-SerialExchange -Serial $serial -Request $Request -TimeoutMs $TimeoutMs
    }
    Invoke-ForceCapture -TransportExchange $transport -Watch ([Diagnostics.Stopwatch]::StartNew()) `
        -SleepMilliseconds {param($ms) Start-Sleep -Milliseconds $ms} -Mode $Mode `
        -StopRequested {
            if (-not [Console]::IsInputRedirected -and [Console]::KeyAvailable) {
                $key=[Console]::ReadKey($true).Key
                return ($key -eq [ConsoleKey]::S -or $key -eq [ConsoleKey]::Escape)
            }
            return $false
        } `
        -OutputCsv $OutputCsv -ActualHash $actualHash -MaximumSeconds $MaximumSeconds `
        -Target $Target -TargetN:$TargetN -ProfileId $ProfileId -Desired $desired -CurrentLimitSetting $CurrentLimitSetting `
        -InitialGap $InitialGap -FieldNotes $FieldNotes -ConfirmedFirmwareSha256 $ConfirmedFirmwareSha256
} finally {
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
