[CmdletBinding()]
param(
    [ValidateSet('Observe','Parameters','SingleStart')][string]$Mode='Observe',
    [switch]$SelfTest,
    [string]$Port,
    [string]$ConfirmedFirmwareSha256,
    [string]$OutputCsv,
    [string]$ParameterFile,
    [ValidateRange(1,275)][int]$Target=250,
    [ValidateRange(1,60)][int]$MaximumSeconds=60,
    [switch]$ConfirmMotorPowerDisconnected,
    [switch]$ConfirmSupervisedMotion,
    [string]$CurrentLimitSetting,
    [string]$InitialGap,
    [string]$FieldNotes
)
$ErrorActionPreference='Stop'
# Import the existing length-aware CRC/RTU transport. Preserve this script's options.
$saved=@{}; foreach ($name in @('Mode','SelfTest','Port','ConfirmedFirmwareSha256','OutputCsv','ParameterFile','Target',
    'MaximumSeconds','ConfirmMotorPowerDisconnected','ConfirmSupervisedMotion','CurrentLimitSetting','InitialGap','FieldNotes')) {
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
    return [pscustomobject]$values
}
function Read-ForceConfig([scriptblock]$Exchange) {
    $words=@(Read-ForceWords $Exchange 3 0x110 (2*$schema.parameters.Count))
    $values=[ordered]@{}; $i=0
    foreach ($p in $schema.parameters) {
        [uint32]$u=[uint64]$words[$i]*65536+[uint64]$words[$i+1]; $i+=2
        $values[$p.name]=[BitConverter]::ToSingle([BitConverter]::GetBytes($u),0)
    }
    return [pscustomobject]$values
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
    Write-Output 'FORCE_SERVO_CAPTURE_WIRE=PASS SYNTHETIC; NO_SERIAL_PORT'
    return
}
if (-not $Port -or -not $OutputCsv) { throw 'Port and new OutputCsv required' }
if ($Mode -ne 'SingleStart' -and -not $ConfirmMotorPowerDisconnected) { throw 'Observe/Parameters requires physically disconnected motor power confirmation' }
if ($Mode -eq 'SingleStart' -and (-not $ConfirmSupervisedMotion -or -not $CurrentLimitSetting -or -not $InitialGap)) {
    throw 'SingleStart requires supervision, permitted load/travel/thermal exposure, external E-stop, actual current limit and initial gap'
}
$firmware=Join-Path $PSScriptRoot '../output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.hex'
$actualHash=(Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash
if ($ConfirmedFirmwareSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $ConfirmedFirmwareSha256 -ine $actualHash) {
    throw 'Operator flash attestation must match the bundled HEX; this is not MCU binary verification'
}
& python "$PSScriptRoot/verify_force_servo_firmware.py"
if ($LASTEXITCODE -ne 0) { throw 'Bundled firmware verification failed; no serial connection' }
if ($Mode -eq 'Parameters') {
    if (-not $ParameterFile) { throw 'Parameters mode requires a complete JSON file' }
    & python "$PSScriptRoot/force_servo_data.py" --validate $ParameterFile
    if ($LASTEXITCODE -ne 0) { throw 'Invalid parameter group; no serial connection' }
    $desired=Get-Content -Raw -LiteralPath $ParameterFile | ConvertFrom-Json
}
$csvPath=[IO.Path]::GetFullPath($OutputCsv)
$reportPath=[IO.Path]::ChangeExtension($csvPath,'.report.txt')
$metaPath=[IO.Path]::ChangeExtension($csvPath,'.metadata.json')
foreach ($p in @($csvPath,$reportPath,$metaPath)) { if (Test-Path -LiteralPath $p) { throw "Output exists: $p" } }
New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($csvPath)) | Out-Null
$rows=New-Object Collections.ArrayList; $serial=$null; $errorText=''; $startAttempts=0
$activeConfig=$null; $watch=[Diagnostics.Stopwatch]::StartNew(); $enforceDeadline=$false
try {
    $serial=New-Object IO.Ports.SerialPort $Port,115200,None,8,One
    $serial.Handshake=[IO.Ports.Handshake]::None; $serial.ReadTimeout=100;$serial.WriteTimeout=100
    $serial.DtrEnable=$false; $serial.RtsEnable=$false; $serial.Open()
    $exchange={param([byte[]]$Request,[int]$TimeoutMs)
        if ($enforceDeadline) {
            $remaining=$deadline-$watch.ElapsedMilliseconds
            if ($remaining -le 0) { throw 'OBSERVATION_DEADLINE' }
            $TimeoutMs=[int][Math]::Min($TimeoutMs,$remaining)
        }
        Invoke-SerialExchange -Serial $serial -Request $Request -TimeoutMs $TimeoutMs
    }
    $info=@(Read-ForceWords $exchange 4 0x100 8)
    if ($info[0] -ne $schema.schema -or $info[2] -ne 2*$schema.parameters.Count -or
        $info[3] -ne 2*($schema.u32.Count+$schema.floats.Count)) { throw 'ForceServo1 capability/schema required' }
    $activeConfig=Read-ForceConfig $exchange
    $first=Read-ForceSnapshot $exchange 'PREFLIGHT'; [void]$rows.Add($first)
    if ($first.state -ne 1 -or $first.output_off -ne 1 -or $first.lease_active -ne 0 -or $first.fault -ne 0) { throw 'IDLE + OFF required' }
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
    }
    if ($Mode -eq 'SingleStart') {
        if ($info[1] -ne 0 -or $first.locked -ne 0) { throw 'PHYSICAL_OUTPUT_LOCKED: no START sent; hardware qualification pending' }
        Write-ForceWord $exchange 0 $Target
        $ready=Read-ForceSnapshot $exchange 'TARGET_READBACK'
        if ($ready.target -ne $Target -or $ready.state -ne 1 -or $ready.fault -ne 0 -or $ready.output_off -ne 1) { throw 'Target/state readback mismatch' }
        $startAttempts=1 # Set before transmitting. Lost echo never retries START.
        Send-SingleCoil $exchange 0x10 0xFF00 100
    }
    $deadline=$watch.ElapsedMilliseconds+$MaximumSeconds*1000; $enforceDeadline=$true
    while ($watch.ElapsedMilliseconds -lt $deadline) {
        $row=Read-ForceSnapshot $exchange $(if ($Mode -eq 'SingleStart') {'RUN'} else {'OBSERVE'})
        [void]$rows.Add($row)
        if ($row.fault -ne 0) { break }
        Start-Sleep -Milliseconds 10
    }
} catch { if ($_.Exception.Message -ne 'OBSERVATION_DEADLINE') { $errorText=$_.Exception.Message } }
finally {
    $enforceDeadline=$false
    if ($null -ne $serial -and $serial.IsOpen) {
        try {
            Send-SingleCoil $exchange 1 0 100
            [void]$rows.Add((Read-ForceSnapshot $exchange 'STOP_READBACK'))
        } catch { $errorText+='; STOP/readback: '+$_.Exception.Message }
    }
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
if ($rows.Count -gt 0) { $rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath }
else { '"phase","firmware_sha256"' | Set-Content -Encoding UTF8 -LiteralPath $csvPath }
@{mode=$Mode;start_attempts=$startAttempts;error=$errorText;config=$activeConfig;
    current_limit_setting=$CurrentLimitSetting;initial_gap=$InitialGap;field_notes=$FieldNotes;
    firmware_sha256=$actualHash;operator_flash_attestation=$ConfirmedFirmwareSha256;
    commissioning='COMMISSIONING_NOT_TUNED';physical_test_status='OPERATOR_CAPTURE_UNVALIDATED';
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
