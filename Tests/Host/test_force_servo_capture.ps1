param([string]$OutputDirectory='output/ForceServo1_CaptureFix1/capture_tests')
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$testOutput=if ([IO.Path]::IsPathRooted($OutputDirectory)) { [IO.Path]::GetFullPath($OutputDirectory) }
            else { [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory)) }
. "$root/tools/capture_force_servo.ps1" -LibraryOnly
$script:caseCount=0
function Check([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Wire-Case {
    param([string]$Name,[byte[]]$Frame,[int[]]$Chunks=@(),[byte]$Function=3,[string]$ErrorPattern='')
    # Only byte availability/read callbacks are simulated. Framing AND subsequent
    # station/function/CRC checks are the same functions reached from a real port.
    $exchange={param([byte[]]$Request,[int]$TimeoutMs)
        Read-TestLengthAwareResponse -Response $Frame -ChunkSizes $Chunks -TimeoutMs $TimeoutMs
    }.GetNewClosure()
    $caught=''
    try {
        [byte[]]$answer=@(Invoke-ModbusRequest $exchange ([byte[]](1,$Function,0,0,0,1)) $Function 10)
        Check (Test-ByteArraysEqual $answer $Frame) "$Name bytes changed"
    } catch { $caught=$_.Exception.Message }
    if ($ErrorPattern) { Check ($caught -like $ErrorPattern) "$Name unexpected error: $caught" }
    else { Check ($caught -eq '') "$Name failed: $caught" }
    $script:caseCount++; Write-Output "LENGTH_CAPTUREFIX1=$Name PASS"
}

$fc03=Add-ModbusCrc ([byte[]](1,3,2,0x12,0x34))
Wire-Case FC03_FULL $fc03
Wire-Case FC03_FRAGMENTED $fc03 @(1,1,1,1,1,1,1)
Wire-Case FC03_HEADER_TIMEOUT ([byte[]](1,3)) @() 3 'Partial Modbus response:*undetermined*'
Wire-Case FC03_PAYLOAD_TIMEOUT ([byte[]]$fc03[0..4]) @() 3 'Partial Modbus response:*expected 7*'
Wire-Case FC03_NO_BYTES ([byte[]]@()) @() 3 'No Modbus response*'
$bad=[byte[]]$fc03.Clone();$bad[-1]=$bad[-1] -bxor 1
Wire-Case FC03_CRC $bad @() 3 '*CRC is invalid*'
Wire-Case FC03_STATION (Add-ModbusCrc ([byte[]](2,3,2,0,42))) @() 3 'Unexpected Modbus station*'
Wire-Case FC03_FUNCTION (Add-ModbusCrc ([byte[]](1,4,2,0,42))) @() 3 'Unexpected Modbus function*'
Wire-Case FC03_OVERLONG ([byte[]]($fc03+0)) @() 3 '*exceeded its declared length*'
Wire-Case FC03_EXCEPTION (Add-ModbusCrc ([byte[]](1,0x83,2))) @(1,1,1,2) 3 'Modbus exception:*0x83*'
Wire-Case FC03_WRONG_EXCEPTION (Add-ModbusCrc ([byte[]](1,0x84,2))) @() 3 '*unexpected Modbus exception*'
foreach ($function in @(4,5,6)) {
    $frame=if ($function -eq 4) { Add-ModbusCrc ([byte[]](1,4,2,0,42)) } else { Add-ModbusCrc ([byte[]](1,$function,0,1,0,0)) }
    Wire-Case "FC${function}_REGRESSION" $frame @(1,1,1,2,3) $function
    Wire-Case "FC${function}_EXCEPTION" (Add-ModbusCrc ([byte[]](1,($function -bor 0x80),2))) @(2,1,2) $function 'Modbus exception:*'
}

function Invoke-ObserveCase([string]$Name,[string]$Failure='',[string]$CaptureMode='Observe',
                            [bool]$Commissioning=$false,[int]$Target=250,[int]$InitialPressure=30) {
    $caseDir=Join-Path $testOutput $Name
    Check (-not (Test-Path -LiteralPath $caseDir)) "Do not overwrite existing test evidence: $caseDir"
    New-Item -ItemType Directory -Path $caseDir -Force | Out-Null
    $defaults=Get-Content -Raw "$root/Docs/Target250MVP1/default_parameters.json" | ConvertFrom-Json
    $configWords=@()
    foreach ($parameter in $schema.parameters) {
        [uint32]$bits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$defaults.($parameter.name)),0)
        $configWords+=@(($bits -shr 16),($bits -band 65535))
    }
    Check ($configWords.Count -eq 48) 'Expected full active parameter group'
    $clock=[pscustomobject]@{ElapsedMilliseconds=0L}
    $sim=@{Starts=0;Stops=0;Latches=0;Framed=0;Failed=$false;Stopped=$false;Target=0;
           Requests=(New-Object Collections.ArrayList);Frozen=@()}
    $assert=${function:Check}
    $transport={param([byte[]]$Request,[int]$TimeoutMs)
        & $assert (Test-ModbusCrc $Request) 'Bad outbound CRC'
        $f=[int]$Request[1]; $a=256*[int]$Request[2]+[int]$Request[3]; $n=256*[int]$Request[4]+[int]$Request[5]
        [void]$sim.Requests.Add([pscustomobject]@{Function=$f;Address=$a;Value=$n})
        $clock.ElapsedMilliseconds+=20
        if ($f -eq 5) {
            if ($a -eq 1 -and $n -eq 0) { $sim.Stops++;$sim.Stopped=$true }
            elseif ($Commissioning -and $CaptureMode -eq 'SingleStart' -and $a -eq 0x10 -and $n -eq 0xFF00) {
                $sim.Starts++
                & $assert ($sim.Starts -eq 1) 'START must never retry'
            }
            else { $sim.Starts++; throw 'TEST_FORBIDS_START' }
            $response=$Request
        } elseif ($f -eq 6) {
            if ($Commissioning -and $CaptureMode -eq 'SingleStart' -and $a -eq 0) {
                $sim.Target=$n
            } else {
            & $assert ($a -eq 0x102 -and $n -eq 0xD101) 'Only target and snapshot latch writes allowed'
            $sim.Latches++; $values=@{schema=$schema.schema;build_id=$schema.build_id;state=1;locked=[int](-not $Commissioning);output_off=1;
                config_version=1;now_ms=$clock.ElapsedMilliseconds;latest_raw=$InitialPressure;latest_received_ms=$clock.ElapsedMilliseconds}
            if ($sim.Starts -eq 1 -and -not $sim.Stopped) { $values.state=13;$values.start_pending=0 }
            if ($Failure -eq 'DeviceFault' -and $sim.Latches -ge 2) { $values.state=9;$values.fault=2;$values.detail=2 }
            if ($Failure -eq 'RunningFault' -and $sim.Starts -eq 1) { $values.state=9;$values.fault=2;$values.detail=2 }
            $sim.Frozen=@()
            foreach ($name in $schema.u32) {
                [uint32]$u=if ($values.ContainsKey($name)) { $values[$name] } else { 0 }
                $sim.Frozen+=@(($u -shr 16),($u -band 65535))
            }
            foreach ($name in $schema.floats) {
                [single]$value=if ($name -eq 'target') {$sim.Target} else {0}
                [uint32]$u=[BitConverter]::ToUInt32([BitConverter]::GetBytes($value),0)
                $sim.Frozen+=@(($u -shr 16),($u -band 65535))
            }
            }
            $response=$Request
        } elseif ($f -eq 3 -or $f -eq 4) {
            & $assert ($n -ge 1 -and $n -le 11) 'Existing 11-word read bound violated'
            if ($f -eq 3) {
                & $assert ($a -ge 0x110 -and $a+$n -le 0x140) 'FC03 config address'
                $words=@($configWords[($a-0x110)..($a-0x110+$n-1)])
            } elseif ($a -eq 0x100) {
                & $assert ($sim.Requests.Count -eq 1) 'Capability must be first'
                $words=@($schema.schema,[int](-not $Commissioning),48,(2*($schema.u32.Count+$schema.floats.Count)),0,1,0,0)
            } else {
                & $assert ($sim.Latches -gt 0) 'Diagnostics must be frozen before reading'
                $words=@($sim.Frozen[($a-0x200)..($a-0x200+$n-1)])
            }
            [byte[]]$payload=@(1,$f,(2*$n))
            foreach ($word in $words) { $payload+=@([byte]($word -shr 8),[byte]($word -band 255)) }
            $response=Add-ModbusCrc $payload
        } else { throw 'Unexpected request' }
        if (-not $sim.Failed -and -not $sim.Stopped -and
            (($Failure -eq 'StartEchoTimeout' -and $f -eq 5) -or
             ($Failure -eq 'ConfigTimeout' -and $f -eq 3) -or
             ($Failure -eq 'DiagnosticTimeout' -and $f -eq 4 -and $a -eq 0x200 -and $sim.Latches -eq 2))) {
            $response=[byte[]]$response[0..1];$sim.Failed=$true
        }
        # Every reply, including capability, FC03 chunks, latch echoes and STOP,
        # traverses production Read-LengthAwareResponse/Get-ModbusResponseLength.
        $sim.Framed++
        Read-TestLengthAwareResponse -Response $response -ChunkSizes @(1,1,1,2,3,4) -TimeoutMs $TimeoutMs
    }.GetNewClosure()
    $sleep={param($ms) $clock.ElapsedMilliseconds+=$ms}.GetNewClosure()
    $stop={ $Failure -eq 'OperatorStop' -and $sim.Latches -ge 3 }.GetNewClosure()
    $csv=Join-Path $caseDir 'SYNTHETIC.csv'; $caught=''
    try {
        Invoke-ForceCapture -TransportExchange $transport -Watch $clock -SleepMilliseconds $sleep -StopRequested $stop `
            -Mode $CaptureMode -Target $Target -OutputCsv $csv -ActualHash 'SYNTHETIC_NOT_DEVICE' -MaximumSeconds 1 `
            -CurrentLimitSetting 'SYNTHETIC_NOT_MEASURED' -InitialGap 'SYNTHETIC_NOT_MEASURED' `
            -FieldNotes 'SYNTHETIC_NO_SERIAL_NO_HARDWARE' -ConfirmedFirmwareSha256 'SYNTHETIC_NOT_DEVICE' | Out-Null
    } catch { $caught=$_.Exception.Message }
    if ($Failure -like '*Timeout') { Check ($caught -like 'Partial Modbus response:*') "Expected bounded timeout, got: $caught" }
    elseif ($CaptureMode -eq 'SingleStart' -and -not $Commissioning) { Check ($caught -like 'PHYSICAL_OUTPUT_LOCKED*') "Lock refusal missing: $caught" }
    elseif ($Commissioning -and $Target -ne 250) {
        Check ($caught -like 'Target250MVP1 requires*') "Commissioning admission refusal missing: $caught"
        Check (@($sim.Requests | Where-Object {$_.Function -eq 6 -and $_.Address -eq 0}).Count -eq 0) 'Refused session wrote target'
    }
    else { Check ($caught -eq '') "Observe failed: $caught" }
    $expectedStarts=[int]($Commissioning -and $CaptureMode -eq 'SingleStart' -and $Target -eq 250)
    Check ($sim.Starts -eq $expectedStarts -and $sim.Stops -eq 1) 'Expected START count and exactly one STOP required'
    Check ($sim.Framed -eq $sim.Requests.Count) 'A response bypassed production length parsing'
    $metadata=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.metadata.json')) | ConvertFrom-Json
    $report=Get-Content -Raw ([IO.Path]::ChangeExtension($csv,'.report.txt')) | ConvertFrom-Json
    $rows=@(Import-Csv -LiteralPath $csv)
    Check ($rows.Count -gt 0 -and $rows[-1].phase -eq 'STOP_READBACK') 'CSV STOP row missing'
    Check ($report.StopVerified -eq $true -and $metadata.start_attempts -eq $expectedStarts) 'Report/metadata STOP and START count proof'
    Check ($report.data_status -eq 'INSUFFICIENT_DATA') 'Observe must not imply powered tracking acceptance'
    if ($Failure -eq 'StartEchoTimeout') { Check ($null -eq $metadata.start_accepted) 'Lost echo must be unknown, never retried or called rejected' }
    if ($Failure -eq 'OperatorStop') {
        Check ($metadata.stop_reason -eq 'OPERATOR_STOP') 'Operator STOP reason lost'
        Check (@($rows | Where-Object phase -eq RUN).Count -eq 1) 'Operator STOP did not end capture promptly'
    }
    if ($Failure -ne 'ConfigTimeout') {
        foreach ($parameter in $schema.parameters) {
            Check ([single]$metadata.config.($parameter.name) -eq [single]$defaults.($parameter.name)) "Bad config readback: $($parameter.name)"
        }
        $reads=@($sim.Requests | Where-Object Function -eq 3)
        Check ($reads.Count -eq 5 -and ($reads.Value -join ',') -eq '11,11,11,11,4') '48-word FC03 chunking mismatch'
        Check ((($reads | ForEach-Object { $_.Address }) -join ',') -eq '272,283,294,305,316') "FC03 chunk addresses mismatch: $(($reads | ForEach-Object { $_.Address }) -join ',')"
    } else {
        Check ($null -eq $metadata.config) 'Unavailable parameters must remain unknown'
        Check (@($sim.Requests | Where-Object Function -eq 3).Count -eq 1) 'Config timeout must not retry'
    }
    if ($Failure -eq '' -and $CaptureMode -eq 'Observe') {
        Check (@($rows | Where-Object phase -eq OBSERVE).Count -ge 2) 'Finite Observe samples missing'
        Check ($clock.ElapsedMilliseconds -ge 1000 -and $clock.ElapsedMilliseconds -lt 2000) 'Observation must terminate'
    }
    if ($Commissioning -and $Failure -eq '' -and $expectedStarts -eq 1) {
        Check (@($rows | Where-Object phase -eq RUN).Count -ge 2) 'Finite SingleStart samples missing'
        Check ($sim.Target -eq $Target) 'Target write/readback missing'
        Check (@($rows | Where-Object phase -eq TARGET_READBACK).Count -eq 1) 'Pre-start evidence missing'
        Check ($metadata.start_accepted -eq $true) 'Successful START echo missing'
    }
    $sim.Requests | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $caseDir 'wire_requests.json') -Encoding UTF8
    $script:caseCount++; Write-Output "CAPTURE_FLOW=$Name PASS STARTS=$expectedStarts; SYNTHETIC_NO_SERIAL"
}
Invoke-ObserveCase CompleteObserve
Invoke-ObserveCase ConfigTimeout ConfigTimeout
Invoke-ObserveCase DiagnosticTimeout DiagnosticTimeout
Invoke-ObserveCase DeviceFault DeviceFault
Invoke-ObserveCase LockedSingleStart '' SingleStart
Invoke-ObserveCase CommissioningSingleStart '' SingleStart $true
Invoke-ObserveCase CommissioningStartEchoTimeout StartEchoTimeout SingleStart $true
Invoke-ObserveCase CommissioningRunningFault RunningFault SingleStart $true
Invoke-ObserveCase Target250OperatorStop OperatorStop SingleStart $true
Invoke-ObserveCase Target250RejectOtherTarget '' SingleStart $true 60
Invoke-ObserveCase Target250ZeroInitial '' SingleStart $true 250 0
Invoke-ObserveCase Target250InitialOutsideOldGate '' SingleStart $true 250 31
Write-Output "CAPTUREFIX1_TEST_CASES=$script:caseCount PASS; physical test NOT RUN"
