[CmdletBinding()]
param(
    [switch]$ListPorts,
    [switch]$SelfTest,
    [switch]$LibraryOnly,
    [string]$Port,
    [ValidateSet('PRESS', 'RELEASE')]
    [string]$Direction,
    [switch]$ConfirmSinglePulse,
    [string]$ConfirmedFirmwareSha256,
    [string]$OutputCsv
)

$ErrorActionPreference = 'Stop'

$script:Station = [byte]1
$script:ExpectedFirmwareName = 'SD700_GuardFixV5_RealBench_Release.hex'
$script:ExpectedFirmwareSha256 =
    '1D5AADF5C957B97654F7B3305813D47594B8C42C3373EA1182B6357559241C04'
$script:ExpectedProfile = '10000 mV / 100 ms / 150 ms backstop'
$script:DisconnectedValue = 0xFFFF

function Add-ModbusCrc {
    param([Parameter(Mandatory = $true)][byte[]]$Payload)

    [uint32]$crc = 0xFFFF
    foreach ($value in $Payload) {
        $crc = $crc -bxor $value
        for ($bit = 0; $bit -lt 8; ++$bit) {
            if (($crc -band 1) -ne 0) {
                $crc = (($crc -shr 1) -bxor 0xA001)
            }
            else {
                $crc = $crc -shr 1
            }
        }
    }

    [byte[]]$frame = New-Object byte[] ($Payload.Length + 2)
    [Array]::Copy($Payload, $frame, $Payload.Length)
    $frame[$Payload.Length] = [byte]($crc -band 0xFF)
    $frame[$Payload.Length + 1] = [byte](($crc -shr 8) -band 0xFF)
    return $frame
}

function Test-ModbusCrc {
    param([Parameter(Mandatory = $true)][byte[]]$Frame)

    if ($Frame.Length -lt 4) {
        return $false
    }
    [byte[]]$payload = $Frame[0..($Frame.Length - 3)]
    [byte[]]$expected = @(Add-ModbusCrc -Payload $payload)
    return (($expected[$expected.Length - 2] -eq $Frame[$Frame.Length - 2]) -and
            ($expected[$expected.Length - 1] -eq $Frame[$Frame.Length - 1]))
}

function Test-ByteArraysEqual {
    param(
        [byte[]]$Left,
        [byte[]]$Right
    )

    if (($null -eq $Left) -or ($null -eq $Right) -or
        ($Left.Length -ne $Right.Length)) {
        return $false
    }
    for ($index = 0; $index -lt $Left.Length; ++$index) {
        if ($Left[$index] -ne $Right[$index]) {
            return $false
        }
    }
    return $true
}

function Read-U16BE {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Bytes,
        [Parameter(Mandatory = $true)][int]$Offset
    )

    if (($Offset -lt 0) -or (($Offset + 1) -ge $Bytes.Length)) {
        throw 'Cannot read a big-endian UInt16 outside the supplied byte array.'
    }
    return (([int]$Bytes[$Offset] -shl 8) -bor [int]$Bytes[$Offset + 1])
}

function Get-ModbusResponseLength {
    param([Parameter(Mandatory = $true)]$Bytes)

    if ($Bytes.Count -lt 2) {
        return -1
    }
    $function = [int]$Bytes[1]
    if (($function -band 0x80) -ne 0) {
        return 5
    }
    if (($function -eq 0x05) -or ($function -eq 0x06)) {
        return 8
    }
    if ($function -eq 0x04) {
        if ($Bytes.Count -lt 3) {
            return -1
        }
        return (5 + [int]$Bytes[2])
    }
    throw ('Unsupported Modbus response function 0x{0:X2}.' -f $function)
}

function Read-LengthAwareResponse {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$GetAvailable,
        [Parameter(Mandatory = $true)][scriptblock]$ReadBytes,
        [Parameter(Mandatory = $true)][scriptblock]$GetElapsedMs,
        [Parameter(Mandatory = $true)][scriptblock]$SleepMilliseconds,
        [ValidateRange(1, 60000)][int]$TimeoutMs
    )

    $bytes = New-Object 'System.Collections.Generic.List[byte]'
    [long]$startedAt = & $GetElapsedMs

    while ($true) {
        $expectedLength = Get-ModbusResponseLength -Bytes $bytes
        if (($expectedLength -ge 0) -and ($bytes.Count -eq $expectedLength)) {
            return $bytes.ToArray()
        }
        if (($expectedLength -ge 0) -and ($bytes.Count -gt $expectedLength)) {
            throw ("Modbus response exceeded its declared length: {0} bytes, expected {1}." -f
                   $bytes.Count, $expectedLength)
        }

        [long]$elapsed = ([long](& $GetElapsedMs)) - $startedAt
        if ($elapsed -ge $TimeoutMs) {
            if ($bytes.Count -eq 0) {
                throw [System.TimeoutException]::new(
                    "No Modbus response received within $TimeoutMs ms.")
            }
            $expectedText = if ($expectedLength -ge 0) {
                [string]$expectedLength
            }
            else {
                'an as-yet undetermined length'
            }
            throw [System.TimeoutException]::new(
                ("Partial Modbus response: received {0} bytes, expected {1}, within {2} ms." -f
                 $bytes.Count, $expectedText, $TimeoutMs))
        }

        [int]$available = & $GetAvailable
        if ($available -gt 0) {
            [byte[]]$chunk = @(& $ReadBytes -Count $available)
            if ($chunk.Length -eq 0) {
                & $SleepMilliseconds 1
                continue
            }
            foreach ($value in $chunk) {
                $bytes.Add($value)
            }
        }
        else {
            & $SleepMilliseconds 1
        }
    }
}

function Invoke-SerialExchange {
    param(
        [Parameter(Mandatory = $true)][System.IO.Ports.SerialPort]$Serial,
        [Parameter(Mandatory = $true)][byte[]]$Request,
        [ValidateRange(1, 60000)][int]$TimeoutMs
    )

    $Serial.DiscardInBuffer()
    $Serial.Write($Request, 0, $Request.Length)

    $readWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $getAvailable = { return $Serial.BytesToRead }.GetNewClosure()
    $readBytes = {
        param([int]$Count)
        [byte[]]$buffer = New-Object byte[] $Count
        $read = $Serial.Read($buffer, 0, $buffer.Length)
        if ($read -eq $buffer.Length) {
            return $buffer
        }
        if ($read -le 0) {
            return [byte[]]@()
        }
        return [byte[]]$buffer[0..($read - 1)]
    }.GetNewClosure()
    $getElapsed = { return [long]$readWatch.ElapsedMilliseconds }.GetNewClosure()
    $sleep = { param([int]$Milliseconds) Start-Sleep -Milliseconds $Milliseconds }

    return Read-LengthAwareResponse -GetAvailable $getAvailable `
        -ReadBytes $readBytes -GetElapsedMs $getElapsed `
        -SleepMilliseconds $sleep -TimeoutMs $TimeoutMs
}

function Invoke-ModbusRequest {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Exchange,
        [Parameter(Mandatory = $true)][byte[]]$Payload,
        [Parameter(Mandatory = $true)][byte]$ExpectedFunction,
        [ValidateRange(1, 60000)][int]$TimeoutMs
    )

    [byte[]]$request = @(Add-ModbusCrc -Payload $Payload)
    [byte[]]$response = @(& $Exchange -Request $request -TimeoutMs $TimeoutMs)

    if ($response.Length -lt 5) {
        throw "Modbus response is too short: $($response.Length) bytes."
    }
    if ($response[0] -ne $script:Station) {
        throw ("Unexpected Modbus station {0}; expected {1}." -f
               $response[0], $script:Station)
    }
    if (-not (Test-ModbusCrc -Frame $response)) {
        throw 'Modbus response CRC is invalid.'
    }

    if (($response[1] -band 0x80) -ne 0) {
        if (($response.Length -ne 5) -or
            ($response[1] -ne ($ExpectedFunction -bor 0x80))) {
            throw 'Malformed or unexpected Modbus exception response.'
        }
        throw ("Modbus exception: function=0x{0:X2}, code=0x{1:X2}." -f
               $response[1], $response[2])
    }
    if ($response[1] -ne $ExpectedFunction) {
        throw ("Unexpected Modbus function 0x{0:X2}; expected 0x{1:X2}." -f
               $response[1], $ExpectedFunction)
    }
    return $response
}

function ConvertFrom-Fc04Response {
    param([Parameter(Mandatory = $true)][byte[]]$Response)

    if (($Response.Length -ne 27) -or ($Response[0] -ne $script:Station) -or
        ($Response[1] -ne 0x04) -or ($Response[2] -ne 22)) {
        throw 'FC04 response did not contain exactly 11 input registers.'
    }
    if (-not (Test-ModbusCrc -Frame $Response)) {
        throw 'FC04 response CRC is invalid.'
    }

    return [pscustomobject][ordered]@{
        ControlPressure = Read-U16BE -Bytes $Response -Offset 3
        RawPressure = Read-U16BE -Bytes $Response -Offset 5
        MachineState = Read-U16BE -Bytes $Response -Offset 7
        Fault = Read-U16BE -Bytes $Response -Offset 9
        FaultDetail = Read-U16BE -Bytes $Response -Offset 11
        LastCommandResult = Read-U16BE -Bytes $Response -Offset 13
        LastMotorAction = Read-U16BE -Bytes $Response -Offset 15
        CommandMv = Read-U16BE -Bytes $Response -Offset 17
        PlannedTim2Ccr3 = Read-U16BE -Bytes $Response -Offset 19
        PlannedTim3Ccr3 = Read-U16BE -Bytes $Response -Offset 21
        StatusFlags = Read-U16BE -Bytes $Response -Offset 23
    }
}

function Decode-StatusFlags {
    param([Parameter(Mandatory = $true)][int]$StatusFlags)

    return [pscustomobject][ordered]@{
        PressureFreshValid = (($StatusFlags -band 0x0001) -ne 0)
        TargetValid = (($StatusFlags -band 0x0002) -ne 0)
        MotorLogicalActive = (($StatusFlags -band 0x0004) -ne 0)
        PhysicalOutputLocked = (($StatusFlags -band 0x0008) -ne 0)
        PhysicalOutputDisabled = (($StatusFlags -band 0x0010) -ne 0)
        BenchRawCountsControl = (($StatusFlags -band 0x0020) -ne 0)
        BenchReleaseCorrection = (($StatusFlags -band 0x0040) -ne 0)
    }
}

function Read-MachineStatus {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Exchange,
        [Parameter(Mandatory = $true)][scriptblock]$GetElapsedMs,
        [Parameter(Mandatory = $true)][string]$Phase,
        [Parameter(Mandatory = $true)][string]$CaptureDirection,
        [Parameter(Mandatory = $true)][string]$AttestedFirmwareSha256,
        [ValidateRange(1, 60000)][int]$TimeoutMs,
        [int]$RequestedPollPeriodMs
    )

    [long]$requestStartElapsed = & $GetElapsedMs
    $requestStartUtc = [DateTime]::UtcNow.ToString('o')
    [byte[]]$response = @(Invoke-ModbusRequest -Exchange $Exchange `
        -Payload ([byte[]](0x01, 0x04, 0x00, 0x00, 0x00, 0x0B)) `
        -ExpectedFunction 0x04 -TimeoutMs $TimeoutMs)
    [long]$responseReceivedElapsed = & $GetElapsedMs
    $responseReceivedUtc = [DateTime]::UtcNow.ToString('o')
    $registers = ConvertFrom-Fc04Response -Response $response
    $flags = Decode-StatusFlags -StatusFlags $registers.StatusFlags

    return [pscustomobject][ordered]@{
        RequestStartUtc = $requestStartUtc
        ResponseReceivedUtc = $responseReceivedUtc
        RequestStartElapsedMs = $requestStartElapsed
        ResponseReceivedElapsedMs = $responseReceivedElapsed
        Phase = $Phase
        EventMarker = ''
        Direction = $CaptureDirection
        AttestedFirmwareSha256 = $AttestedFirmwareSha256
        ExpectedProfile = $script:ExpectedProfile
        RequestedPollPeriodMs = $RequestedPollPeriodMs
        ControlPressure = $registers.ControlPressure
        RawPressure = $registers.RawPressure
        MachineState = $registers.MachineState
        Fault = $registers.Fault
        FaultDetail = $registers.FaultDetail
        LastCommandResult = $registers.LastCommandResult
        LastMotorAction = $registers.LastMotorAction
        CommandMv = $registers.CommandMv
        PlannedTim2Ccr3 = $registers.PlannedTim2Ccr3
        PlannedTim3Ccr3 = $registers.PlannedTim3Ccr3
        StatusFlags = $registers.StatusFlags
        PressureFreshValid = $flags.PressureFreshValid
        TargetValid = $flags.TargetValid
        MotorLogicalActive = $flags.MotorLogicalActive
        PhysicalOutputLocked = $flags.PhysicalOutputLocked
        PhysicalOutputDisabled = $flags.PhysicalOutputDisabled
        BenchRawCountsControl = $flags.BenchRawCountsControl
        BenchReleaseCorrection = $flags.BenchReleaseCorrection
        RunResult = ''
        RunDetail = ''
    }
}

function Send-SingleCoil {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Exchange,
        [Parameter(Mandatory = $true)][int]$Address,
        [Parameter(Mandatory = $true)][int]$Value,
        [ValidateRange(1, 60000)][int]$TimeoutMs
    )

    [byte[]]$payload = [byte[]](
        $script:Station, 0x05,
        (($Address -shr 8) -band 0xFF), ($Address -band 0xFF),
        (($Value -shr 8) -band 0xFF), ($Value -band 0xFF)
    )
    [byte[]]$response = @(Invoke-ModbusRequest -Exchange $Exchange `
        -Payload $payload -ExpectedFunction 0x05 -TimeoutMs $TimeoutMs)
    [byte[]]$expectedEcho = @(Add-ModbusCrc -Payload $payload)
    if (($response.Length -ne 8) -or
        (-not (Test-ByteArraysEqual -Left $response -Right $expectedEcho))) {
        throw 'FC05 response was not the exact request echo.'
    }
}

function Get-SampleSafetyError {
    param([Parameter(Mandatory = $true)]$Sample)

    if (($Sample.ControlPressure -eq $script:DisconnectedValue) -or
        ($Sample.RawPressure -eq $script:DisconnectedValue)) {
        return 'Pressure or raw pressure is invalid (0xFFFF).'
    }
    if (-not $Sample.PressureFreshValid) {
        return 'PressureFreshValid is false.'
    }
    if (($Sample.Fault -ne 0) -or ($Sample.FaultDetail -ne 0)) {
        return ("Fault status is nonzero (Fault={0}, FaultDetail={1})." -f
                $Sample.Fault, $Sample.FaultDetail)
    }
    if (($Sample.PlannedTim2Ccr3 -ne 0) -and
        ($Sample.PlannedTim3Ccr3 -ne 0)) {
        return 'Both planned directional CCR values are nonzero.'
    }
    return $null
}

function Get-AdmissionError {
    param([Parameter(Mandatory = $true)]$Sample)

    $safetyError = Get-SampleSafetyError -Sample $Sample
    if ($null -ne $safetyError) {
        return $safetyError
    }
    if ($Sample.MachineState -ne 1) {
        return "Machine state is $($Sample.MachineState), not IDLE (1)."
    }
    if ($Sample.MotorLogicalActive) {
        return 'MotorLogicalActive is true.'
    }
    if ($Sample.PhysicalOutputLocked) {
        return 'PhysicalOutputLocked is true.'
    }
    if (-not $Sample.PhysicalOutputDisabled) {
        return 'PhysicalOutputDisabled is false.'
    }
    return $null
}

function Test-ExpectedActiveSample {
    param(
        [Parameter(Mandatory = $true)]$Sample,
        [Parameter(Mandatory = $true)][string]$CaptureDirection
    )

    if ((-not $Sample.MotorLogicalActive) -or
        $Sample.PhysicalOutputLocked -or $Sample.PhysicalOutputDisabled -or
        ($Sample.CommandMv -ne 10000)) {
        return $false
    }
    if ($CaptureDirection -eq 'PRESS') {
        return (($Sample.MachineState -eq 10) -and
                ($Sample.LastMotorAction -eq 3) -and
                ($Sample.PlannedTim2Ccr3 -eq 0) -and
                ($Sample.PlannedTim3Ccr3 -gt 0))
    }
    return (($Sample.MachineState -eq 11) -and
            ($Sample.LastMotorAction -eq 4) -and
            ($Sample.PlannedTim2Ccr3 -gt 0) -and
            ($Sample.PlannedTim3Ccr3 -eq 0))
}

function Test-DisabledIdleSample {
    param([Parameter(Mandatory = $true)]$Sample)

    return (($Sample.MachineState -eq 1) -and
            (-not $Sample.MotorLogicalActive) -and
            $Sample.PhysicalOutputDisabled -and
            ($Sample.PlannedTim2Ccr3 -eq 0) -and
            ($Sample.PlannedTim3Ccr3 -eq 0))
}

function Wait-UntilPollDue {
    param(
        [Parameter(Mandatory = $true)][long]$DueAtMs,
        [Parameter(Mandatory = $true)][scriptblock]$GetElapsedMs,
        [Parameter(Mandatory = $true)][scriptblock]$SleepMilliseconds
    )

    [long]$remaining = $DueAtMs - [long](& $GetElapsedMs)
    if ($remaining -gt 0) {
        & $SleepMilliseconds ([int]$remaining)
    }
}

function Invoke-OnePulseCapture {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Exchange,
        [Parameter(Mandatory = $true)][scriptblock]$GetElapsedMs,
        [Parameter(Mandatory = $true)][scriptblock]$SleepMilliseconds,
        [Parameter(Mandatory = $true)][string]$CaptureDirection,
        [Parameter(Mandatory = $true)][string]$AttestedFirmwareSha256,
        [Parameter(Mandatory = $true)]$Options
    )

    $samples = New-Object System.Collections.ArrayList
    $errors = New-Object System.Collections.ArrayList
    $motionAttempted = $false
    $motionAcknowledged = $false
    $activeObserved = $false
    $offObserved = $false
    $completedPostOff = $false
    $stopAttempted = $false
    $shutdownVerified = $false
    $offSample = $null

    try {
        [long]$baselineStartedAt = & $GetElapsedMs
        [long]$nextPollAt = $baselineStartedAt
        while ($true) {
            $sample = Read-MachineStatus -Exchange $Exchange `
                -GetElapsedMs $GetElapsedMs -Phase 'Baseline' `
                -CaptureDirection $CaptureDirection `
                -AttestedFirmwareSha256 $AttestedFirmwareSha256 `
                -TimeoutMs $Options.SerialTimeoutMs `
                -RequestedPollPeriodMs $Options.PollPeriodMs
            [void]$samples.Add($sample)
            $admissionError = Get-AdmissionError -Sample $sample
            if ($null -ne $admissionError) {
                throw "Admission rejected during baseline: $admissionError"
            }
            if (($sample.ResponseReceivedElapsedMs - $baselineStartedAt) -ge
                $Options.BaselineMs) {
                break
            }
            $nextPollAt += $Options.PollPeriodMs
            Wait-UntilPollDue -DueAtMs $nextPollAt `
                -GetElapsedMs $GetElapsedMs `
                -SleepMilliseconds $SleepMilliseconds
        }

        $preMotion = Read-MachineStatus -Exchange $Exchange `
            -GetElapsedMs $GetElapsedMs -Phase 'PreMotion' `
            -CaptureDirection $CaptureDirection `
            -AttestedFirmwareSha256 $AttestedFirmwareSha256 `
            -TimeoutMs $Options.SerialTimeoutMs `
            -RequestedPollPeriodMs $Options.PollPeriodMs
        [void]$samples.Add($preMotion)
        $admissionError = Get-AdmissionError -Sample $preMotion
        if ($null -ne $admissionError) {
            throw "Admission rejected immediately before motion: $admissionError"
        }

        $motionAddress = if ($CaptureDirection -eq 'PRESS') { 0x0003 } else { 0x0002 }
        $motionAttempted = $true
        [long]$commandAttemptedAt = & $GetElapsedMs
        try {
            Send-SingleCoil -Exchange $Exchange -Address $motionAddress `
                -Value 0xFF00 -TimeoutMs $Options.SerialTimeoutMs
            $motionAcknowledged = $true
        }
        catch {
            throw ("Motion acknowledgement failed; the command was not retried: {0}" -f
                   $_.Exception.Message)
        }

        [long]$nextMotionPollAt = $commandAttemptedAt
        [long]$offObservedAt = -1
        while ($true) {
            $phase = if ($offObserved) { 'PostOutputOff' } else { 'MotionObservation' }
            $sample = Read-MachineStatus -Exchange $Exchange `
                -GetElapsedMs $GetElapsedMs -Phase $phase `
                -CaptureDirection $CaptureDirection `
                -AttestedFirmwareSha256 $AttestedFirmwareSha256 `
                -TimeoutMs $Options.SerialTimeoutMs `
                -RequestedPollPeriodMs $Options.PollPeriodMs
            [void]$samples.Add($sample)

            $safetyError = Get-SampleSafetyError -Sample $sample
            if ($null -ne $safetyError) {
                throw "Capture aborted: $safetyError"
            }

            if (-not $activeObserved) {
                if (Test-ExpectedActiveSample -Sample $sample `
                    -CaptureDirection $CaptureDirection) {
                    $activeObserved = $true
                    $sample.EventMarker = 'ActiveObserved'
                }
                elseif (($sample.MotorLogicalActive) -or
                        ($sample.MachineState -eq 10) -or
                        ($sample.MachineState -eq 11) -or
                        (-not (Test-DisabledIdleSample -Sample $sample))) {
                    throw 'Unexpected motion, direction, state, or physical-output status was observed.'
                }
            }
            elseif (-not $offObserved) {
                if (Test-ExpectedActiveSample -Sample $sample `
                    -CaptureDirection $CaptureDirection) {
                    # The requested pulse is still active.
                }
                elseif (Test-DisabledIdleSample -Sample $sample) {
                    $offObserved = $true
                    $offSample = $sample
                    $sample.EventMarker = 'ObservedOutputOff'
                    $offObservedAt = $sample.ResponseReceivedElapsedMs
                    if (($offObservedAt - $commandAttemptedAt) -gt
                        $Options.MotionDeadlineMs) {
                        throw ("The active-to-disabled transition was not observed within {0} ms." -f
                               $Options.MotionDeadlineMs)
                    }
                }
                else {
                    throw 'Unexpected motion, direction, state, or physical-output status was observed.'
                }
            }
            elseif (-not (Test-DisabledIdleSample -Sample $sample)) {
                throw 'Output did not remain inactive, physically disabled, and IDLE after the pulse.'
            }

            if ($offObserved -and
                (($sample.ResponseReceivedElapsedMs - $offObservedAt) -ge
                 $Options.PostOutputOffMs)) {
                $completedPostOff = $true
                break
            }

            if ((-not $offObserved) -and
                (($sample.ResponseReceivedElapsedMs - $commandAttemptedAt) -ge
                 $Options.MotionDeadlineMs)) {
                if (-not $activeObserved) {
                    throw ("Active motion was not observed within {0} ms; capture is incomplete and no second pulse will be sent." -f
                           $Options.MotionDeadlineMs)
                }
                throw ("The active-to-disabled transition was not observed within {0} ms." -f
                       $Options.MotionDeadlineMs)
            }

            $nextMotionPollAt += $Options.PollPeriodMs
            Wait-UntilPollDue -DueAtMs $nextMotionPollAt `
                -GetElapsedMs $GetElapsedMs `
                -SleepMilliseconds $SleepMilliseconds
        }
    }
    catch {
        [void]$errors.Add($_.Exception.Message)
    }
    finally {
        if ($motionAttempted) {
            $stopAttempted = $true
            try {
                Send-SingleCoil -Exchange $Exchange -Address 0x0001 `
                    -Value 0x0000 -TimeoutMs $Options.SerialTimeoutMs
            }
            catch {
                [void]$errors.Add("Priority STOP attempt failed: $($_.Exception.Message)")
            }

            try {
                $shutdownSample = Read-MachineStatus -Exchange $Exchange `
                    -GetElapsedMs $GetElapsedMs -Phase 'Cleanup' `
                    -CaptureDirection $CaptureDirection `
                    -AttestedFirmwareSha256 $AttestedFirmwareSha256 `
                    -TimeoutMs $Options.SerialTimeoutMs `
                    -RequestedPollPeriodMs $Options.PollPeriodMs
                [void]$samples.Add($shutdownSample)
                $shutdownSample.EventMarker = 'FinalShutdownCheck'
                $shutdownVerified = Test-DisabledIdleSample -Sample $shutdownSample
                if (-not $shutdownVerified) {
                    [void]$errors.Add(
                        'Final status did not confirm inactive, physically disabled, IDLE output.')
                }
            }
            catch {
                [void]$errors.Add("Final shutdown status read failed: $($_.Exception.Message)")
            }
        }
    }

    $dataCaptured = ($motionAttempted -and $motionAcknowledged -and
                     $activeObserved -and $offObserved -and
                     $completedPostOff -and $shutdownVerified -and
                     ($errors.Count -eq 0))
    $detail = if ($errors.Count -gt 0) {
        ($errors -join '; ')
    }
    elseif (-not $dataCaptured) {
        'Required capture observations were incomplete.'
    }
    else {
        ''
    }

    return [pscustomobject][ordered]@{
        Samples = $samples
        MotionAttempted = $motionAttempted
        MotionAcknowledged = $motionAcknowledged
        ActiveObserved = $activeObserved
        OffObserved = $offObserved
        CompletedPostOff = $completedPostOff
        StopAttempted = $stopAttempted
        ShutdownVerified = $shutdownVerified
        DataCaptured = $dataCaptured
        Detail = $detail
        OffSample = $offSample
    }
}

function Get-Median {
    param([object[]]$Values)

    if (($null -eq $Values) -or ($Values.Count -eq 0)) {
        return $null
    }
    [double[]]$sorted = @($Values | ForEach-Object { [double]$_ } | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) {
        return $sorted[$middle]
    }
    return (($sorted[$middle - 1] + $sorted[$middle]) / 2.0)
}

function Get-CaptureMetrics {
    param(
        [Parameter(Mandatory = $true)]$Samples,
        [Parameter(Mandatory = $true)][string]$CaptureDirection,
        [Parameter(Mandatory = $true)][bool]$CompletedPostOff,
        [Parameter(Mandatory = $true)][int]$FinalWindowMs
    )

    $validSamples = @($Samples | Where-Object {
        ($_.ControlPressure -ne $script:DisconnectedValue) -and
        ($_.RawPressure -ne $script:DisconnectedValue) -and
        $_.PressureFreshValid
    })
    $baseline = @($validSamples | Where-Object { $_.Phase -eq 'Baseline' })
    $capture = @($validSamples | Where-Object { $_.Phase -ne 'Cleanup' })
    $postCommand = @($validSamples | Where-Object {
        ($_.Phase -eq 'MotionObservation') -or ($_.Phase -eq 'PostOutputOff')
    })
    $off = @($validSamples | Where-Object {
        $_.EventMarker -eq 'ObservedOutputOff'
    } | Select-Object -First 1)

    $before = Get-Median -Values @($baseline | ForEach-Object { $_.ControlPressure })
    $observedMax = $null
    $observedMin = $null
    if ($postCommand.Count -gt 0) {
        $observedMax = [double](($postCommand | Measure-Object -Property ControlPressure -Maximum).Maximum)
        $observedMin = [double](($postCommand | Measure-Object -Property ControlPressure -Minimum).Minimum)
    }

    $pressureAtOff = $null
    $endWindowMedian = $null
    $residual = $null
    if ($off.Count -gt 0) {
        $pressureAtOff = [double]$off[0].ControlPressure
    }
    if ($CompletedPostOff -and ($off.Count -gt 0) -and ($postCommand.Count -gt 0)) {
        [long]$lastCaptureAt = ($postCommand |
            Measure-Object -Property ResponseReceivedElapsedMs -Maximum).Maximum
        $endWindow = @($postCommand | Where-Object {
            $_.ResponseReceivedElapsedMs -ge ($lastCaptureAt - $FinalWindowMs)
        })
        $endWindowMedian = Get-Median -Values @(
            $endWindow | ForEach-Object { $_.ControlPressure })

        $postOff = @($postCommand | Where-Object {
            $_.ResponseReceivedElapsedMs -ge $off[0].ResponseReceivedElapsedMs
        })
        if ($postOff.Count -gt 0) {
            if ($CaptureDirection -eq 'PRESS') {
                $directionalExtreme = [double](($postOff |
                    Measure-Object -Property ControlPressure -Maximum).Maximum)
            }
            else {
                $directionalExtreme = [double](($postOff |
                    Measure-Object -Property ControlPressure -Minimum).Minimum)
            }
            $residual = $directionalExtreme - $pressureAtOff
        }
    }

    $finalPressure = $null
    if ($capture.Count -gt 0) {
        $finalPressure = [double]$capture[$capture.Count - 1].ControlPressure
    }
    $settledDelta = $null
    if (($null -ne $before) -and ($null -ne $endWindowMedian)) {
        $settledDelta = $endWindowMedian - $before
    }

    return [pscustomobject][ordered]@{
        BeforePressure = $before
        ObservedMax = $observedMax
        ObservedMin = $observedMin
        PressureAtObservedOff = $pressureAtOff
        EndWindowMedian = $endWindowMedian
        FinalPressure = $finalPressure
        SettledDeltaEstimate = $settledDelta
        ResidualChangeEstimate = $residual
    }
}

function Format-MetricValue {
    param($Value)

    if ($null -eq $Value) {
        return 'N/A'
    }
    return ([Convert]::ToString($Value, [Globalization.CultureInfo]::InvariantCulture))
}

function Export-CaptureCsv {
    param(
        [Parameter(Mandatory = $true)]$Samples,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $columns = @(
        'RequestStartUtc', 'ResponseReceivedUtc',
        'RequestStartElapsedMs', 'ResponseReceivedElapsedMs',
        'Phase', 'EventMarker', 'Direction', 'AttestedFirmwareSha256',
        'ExpectedProfile', 'RequestedPollPeriodMs',
        'ControlPressure', 'RawPressure', 'MachineState', 'Fault',
        'FaultDetail', 'LastCommandResult', 'LastMotorAction', 'CommandMv',
        'PlannedTim2Ccr3', 'PlannedTim3Ccr3', 'StatusFlags',
        'PressureFreshValid', 'TargetValid', 'MotorLogicalActive',
        'PhysicalOutputLocked', 'PhysicalOutputDisabled',
        'BenchRawCountsControl', 'BenchReleaseCorrection',
        'RunResult', 'RunDetail'
    )

    $csvLines = @()
    if ($Samples.Count -gt 0) {
        $csvLines = @($Samples | Select-Object -Property $columns |
            ConvertTo-Csv -NoTypeInformation)
    }
    else {
        $blank = [ordered]@{}
        foreach ($column in $columns) {
            $blank[$column] = ''
        }
        $csvLines = @(([pscustomobject]$blank | ConvertTo-Csv -NoTypeInformation)[0])
    }

    $utf8Bom = New-Object System.Text.UTF8Encoding -ArgumentList $true
    $stream = New-Object System.IO.FileStream(
        $Path,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.StreamWriter($stream, $utf8Bom)
        try {
            foreach ($line in $csvLines) {
                $writer.WriteLine($line)
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
    }
}

function Assert-SelfTest {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw "Self-test failed: $Message"
    }
}

function New-TestStatus {
    param([hashtable]$Override = @{})

    $values = [ordered]@{
        ControlPressure = 1000
        RawPressure = 2000
        MachineState = 1
        Fault = 0
        FaultDetail = 0
        LastCommandResult = 0
        LastMotorAction = 0
        CommandMv = 0
        PlannedTim2Ccr3 = 0
        PlannedTim3Ccr3 = 0
        StatusFlags = 0x0011
    }
    foreach ($key in $Override.Keys) {
        $values[$key] = $Override[$key]
    }
    return [pscustomobject]$values
}

function New-TestFc04Frame {
    param([Parameter(Mandatory = $true)]$Status)

    $payload = New-Object 'System.Collections.Generic.List[byte]'
    foreach ($value in [byte[]](0x01, 0x04, 0x16)) {
        $payload.Add($value)
    }
    foreach ($value in @(
        $Status.ControlPressure, $Status.RawPressure, $Status.MachineState,
        $Status.Fault, $Status.FaultDetail, $Status.LastCommandResult,
        $Status.LastMotorAction, $Status.CommandMv,
        $Status.PlannedTim2Ccr3, $Status.PlannedTim3Ccr3,
        $Status.StatusFlags)) {
        $payload.Add([byte](($value -shr 8) -band 0xFF))
        $payload.Add([byte]($value -band 0xFF))
    }
    return Add-ModbusCrc -Payload $payload.ToArray()
}

function New-TestExchange {
    param(
        [Parameter(Mandatory = $true)][object[]]$Statuses,
        [switch]$LoseMotionAcknowledgement,
        [int]$TimeoutOnStatusRead = 0
    )

    $tracker = @{
        MotionCommands = 0
        StopCommands = 0
        StatusReads = 0
        Events = New-Object System.Collections.ArrayList
        Queue = New-Object System.Collections.Queue
        LastStatus = $null
        LoseMotionAcknowledgement = [bool]$LoseMotionAcknowledgement
        TimeoutOnStatusRead = $TimeoutOnStatusRead
    }
    foreach ($status in $Statuses) {
        $tracker.Queue.Enqueue($status)
        $tracker.LastStatus = $status
    }

    $exchange = {
        param([byte[]]$Request, [int]$TimeoutMs)
        if (-not (Test-ModbusCrc -Frame $Request)) {
            throw 'Fake transport received a request with bad CRC.'
        }
        if ($Request[1] -eq 0x04) {
            ++$tracker.StatusReads
            [void]$tracker.Events.Add('STATUS')
            if (($tracker.TimeoutOnStatusRead -gt 0) -and
                ($tracker.StatusReads -eq $tracker.TimeoutOnStatusRead)) {
                throw [System.TimeoutException]::new('Fake status timeout.')
            }
            if ($tracker.Queue.Count -gt 0) {
                $tracker.LastStatus = $tracker.Queue.Dequeue()
            }
            if ($null -eq $tracker.LastStatus) {
                throw 'Fake transport has no status to return.'
            }
            return New-TestFc04Frame -Status $tracker.LastStatus
        }
        if ($Request[1] -eq 0x05) {
            $address = Read-U16BE -Bytes $Request -Offset 2
            $value = Read-U16BE -Bytes $Request -Offset 4
            if (($address -eq 0x0002) -or ($address -eq 0x0003)) {
                ++$tracker.MotionCommands
                [void]$tracker.Events.Add('MOTION')
                if ($tracker.LoseMotionAcknowledgement) {
                    throw [System.TimeoutException]::new('Fake lost motion acknowledgement.')
                }
            }
            elseif (($address -eq 0x0001) -and ($value -eq 0x0000)) {
                ++$tracker.StopCommands
                [void]$tracker.Events.Add('STOP')
            }
            else {
                throw 'Fake transport received an unapproved FC05 request.'
            }
            return $Request
        }
        throw 'Fake transport received an unapproved function.'
    }.GetNewClosure()

    return [pscustomobject]@{
        Exchange = $exchange
        Tracker = $tracker
    }
}

function New-TestClock {
    $clock = @{ Now = [long]0 }
    return [pscustomobject]@{
        GetElapsedMs = { return [long]$clock.Now }.GetNewClosure()
        SleepMilliseconds = {
            param([int]$Milliseconds)
            $clock.Now += $Milliseconds
        }.GetNewClosure()
    }
}

function Read-TestLengthAwareResponse {
    # Byte-level fake serial input. Exercise the SAME framing path as serial I/O;
    # CRC/function/echo validation remains in the callers, after framing.
    param(
        [AllowEmptyCollection()][byte[]]$Response,
        [int[]]$ChunkSizes = @(),
        [int]$TimeoutMs = 100
    )
    $wire = @{ Bytes=$Response; Offset=0; Chunk=0; Now=0L; NextAt=0L }
    $available = {
        if ($wire.Now -lt $wire.NextAt) { return 0 }
        $remaining = $wire.Bytes.Length - $wire.Offset
        if ($wire.Chunk -lt $ChunkSizes.Count) {
            return [Math]::Min($remaining, $ChunkSizes[$wire.Chunk])
        }
        return $remaining
    }.GetNewClosure()
    $read = {
        param([int]$Count)
        [byte[]]$chunk = $wire.Bytes[$wire.Offset..($wire.Offset+$Count-1)]
        $wire.Offset += $Count; $wire.Chunk++; $wire.NextAt=$wire.Now+1
        return $chunk
    }.GetNewClosure()
    $now = { return [long]$wire.Now }.GetNewClosure()
    $sleep = { param([int]$Milliseconds) $wire.Now += $Milliseconds }.GetNewClosure()
    return Read-LengthAwareResponse -GetAvailable $available -ReadBytes $read `
        -GetElapsedMs $now -SleepMilliseconds $sleep -TimeoutMs $TimeoutMs
}

function New-TestOptions {
    return [pscustomobject]@{
        BaselineMs = 20
        PollPeriodMs = 10
        PostOutputOffMs = 20
        FinalWindowMs = 10
        SerialTimeoutMs = 10
        MotionDeadlineMs = 50
    }
}

function New-MetricSample {
    param(
        [int]$Pressure,
        [long]$At,
        [string]$Phase,
        [string]$Marker = ''
    )

    return [pscustomobject]@{
        ControlPressure = $Pressure
        RawPressure = 1
        PressureFreshValid = $true
        ResponseReceivedElapsedMs = $At
        Phase = $Phase
        EventMarker = $Marker
    }
}

function Invoke-CaptureSelfTest {
    [byte[]]$request = @(Add-ModbusCrc -Payload ([byte[]](
        0x01, 0x04, 0x00, 0x00, 0x00, 0x0B)))
    Assert-SelfTest -Condition (Test-ModbusCrc -Frame $request) `
        -Message 'valid CRC was rejected'
    $corrupted = [byte[]]$request.Clone()
    $corrupted[$corrupted.Length - 1] = $corrupted[$corrupted.Length - 1] -bxor 0x01
    Assert-SelfTest -Condition (-not (Test-ModbusCrc -Frame $corrupted)) `
        -Message 'corrupted CRC was accepted'

    $decodeStatus = New-TestStatus -Override @{
        ControlPressure = 101; RawPressure = 202; MachineState = 10
        Fault = 3; FaultDetail = 4; LastCommandResult = 5
        LastMotorAction = 3; CommandMv = 10000
        PlannedTim2Ccr3 = 0; PlannedTim3Ccr3 = 777
        StatusFlags = 0x0055
    }
    [byte[]]$decodeFrame = @(New-TestFc04Frame -Status $decodeStatus)
    $decoded = ConvertFrom-Fc04Response -Response $decodeFrame
    Assert-SelfTest -Condition (
        ($decoded.ControlPressure -eq 101) -and
        ($decoded.RawPressure -eq 202) -and
        ($decoded.MachineState -eq 10) -and
        ($decoded.CommandMv -eq 10000) -and
        ($decoded.PlannedTim3Ccr3 -eq 777)) `
        -Message 'FC04 11-register decoding failed'

    [byte[]]$exceptionFrame = @(Add-ModbusCrc -Payload ([byte[]](0x01, 0x84, 0x02)))
    $exceptionExchange = { param([byte[]]$Request, [int]$TimeoutMs) return $exceptionFrame }.GetNewClosure()
    $exceptionRejected = $false
    try {
        [void](Invoke-ModbusRequest -Exchange $exceptionExchange `
            -Payload ([byte[]](0x01, 0x04, 0, 0, 0, 0x0B)) `
            -ExpectedFunction 0x04 -TimeoutMs 10)
    }
    catch {
        $exceptionRejected = ($_.Exception.Message -like 'Modbus exception:*')
    }
    Assert-SelfTest -Condition $exceptionRejected `
        -Message 'FC04 exception response was not rejected'

    $chunkState = @{
        Queue = New-Object System.Collections.Queue
        Now = [long]0
    }
    $chunkState.Queue.Enqueue([byte[]]$decodeFrame[0..1])
    $chunkState.Queue.Enqueue([byte[]]$decodeFrame[2..8])
    $chunkState.Queue.Enqueue([byte[]]$decodeFrame[9..($decodeFrame.Length - 1)])
    $getAvailable = {
        if ($chunkState.Queue.Count -eq 0) { return 0 }
        return $chunkState.Queue.Peek().Length
    }.GetNewClosure()
    $readBytes = { param([int]$Count) return $chunkState.Queue.Dequeue() }.GetNewClosure()
    $getElapsed = { return [long]$chunkState.Now }.GetNewClosure()
    $sleep = { param([int]$Milliseconds) $chunkState.Now += $Milliseconds }.GetNewClosure()
    [byte[]]$assembled = @(Read-LengthAwareResponse `
        -GetAvailable $getAvailable -ReadBytes $readBytes `
        -GetElapsedMs $getElapsed -SleepMilliseconds $sleep -TimeoutMs 10)
    Assert-SelfTest -Condition (Test-ByteArraysEqual -Left $assembled -Right $decodeFrame) `
        -Message 'partial chunks were not assembled into a complete response'

    $partialState = @{
        Queue = New-Object System.Collections.Queue
        Now = [long]0
    }
    $partialState.Queue.Enqueue([byte[]]$decodeFrame[0..3])
    $partialAvailable = {
        if ($partialState.Queue.Count -eq 0) { return 0 }
        return $partialState.Queue.Peek().Length
    }.GetNewClosure()
    $partialRead = { param([int]$Count) return $partialState.Queue.Dequeue() }.GetNewClosure()
    $partialNow = { return [long]$partialState.Now }.GetNewClosure()
    $partialSleep = { param([int]$Milliseconds) $partialState.Now += $Milliseconds }.GetNewClosure()
    $partialTimedOut = $false
    try {
        [void](Read-LengthAwareResponse -GetAvailable $partialAvailable `
            -ReadBytes $partialRead -GetElapsedMs $partialNow `
            -SleepMilliseconds $partialSleep -TimeoutMs 5)
    }
    catch [System.TimeoutException] {
        $partialTimedOut = ($_.Exception.Message -like 'Partial Modbus response:*')
    }
    Assert-SelfTest -Condition $partialTimedOut `
        -Message 'partial response did not produce a bounded timeout'

    $flags = Decode-StatusFlags -StatusFlags 0x0055
    Assert-SelfTest -Condition (
        $flags.PressureFreshValid -and (-not $flags.TargetValid) -and
        $flags.MotorLogicalActive -and (-not $flags.PhysicalOutputLocked) -and
        $flags.PhysicalOutputDisabled -and (-not $flags.BenchRawCountsControl) -and
        $flags.BenchReleaseCorrection) `
        -Message 'status flags were decoded incorrectly'

    $pressMetricSamples = @(
        (New-MetricSample 100 0 'Baseline'),
        (New-MetricSample 110 10 'Baseline'),
        (New-MetricSample 130 20 'MotionObservation' 'ActiveObserved'),
        (New-MetricSample 140 30 'MotionObservation' 'ObservedOutputOff'),
        (New-MetricSample 170 40 'PostOutputOff'),
        (New-MetricSample 160 50 'PostOutputOff'))
    $pressMetrics = Get-CaptureMetrics -Samples $pressMetricSamples `
        -CaptureDirection 'PRESS' -CompletedPostOff $true -FinalWindowMs 10
    Assert-SelfTest -Condition (
        ($pressMetrics.BeforePressure -eq 105) -and
        ($pressMetrics.ObservedMax -eq 170) -and
        ($pressMetrics.PressureAtObservedOff -eq 140) -and
        ($pressMetrics.EndWindowMedian -eq 165) -and
        ($pressMetrics.FinalPressure -eq 160) -and
        ($pressMetrics.SettledDeltaEstimate -eq 60) -and
        ($pressMetrics.ResidualChangeEstimate -eq 30)) `
        -Message 'PRESS metric calculation failed'

    $releaseMetricSamples = @(
        (New-MetricSample 200 0 'Baseline'),
        (New-MetricSample 220 10 'Baseline'),
        (New-MetricSample 190 20 'MotionObservation' 'ActiveObserved'),
        (New-MetricSample 180 30 'MotionObservation' 'ObservedOutputOff'),
        (New-MetricSample 150 40 'PostOutputOff'),
        (New-MetricSample 160 50 'PostOutputOff'))
    $releaseMetrics = Get-CaptureMetrics -Samples $releaseMetricSamples `
        -CaptureDirection 'RELEASE' -CompletedPostOff $true -FinalWindowMs 10
    Assert-SelfTest -Condition (
        ($releaseMetrics.BeforePressure -eq 210) -and
        ($releaseMetrics.ObservedMin -eq 150) -and
        ($releaseMetrics.EndWindowMedian -eq 155) -and
        ($releaseMetrics.SettledDeltaEstimate -eq -55) -and
        ($releaseMetrics.ResidualChangeEstimate -eq -30)) `
        -Message 'RELEASE metric calculation failed'

    $options = New-TestOptions
    $idle = New-TestStatus
    $pressActive = New-TestStatus -Override @{
        MachineState = 10; LastMotorAction = 3; CommandMv = 10000
        PlannedTim3Ccr3 = 500; StatusFlags = 0x0005
    }
    $releaseActive = New-TestStatus -Override @{
        MachineState = 11; LastMotorAction = 4; CommandMv = 10000
        PlannedTim2Ccr3 = 500; StatusFlags = 0x0005
    }

    $rejected = New-TestStatus -Override @{ MachineState = 2 }
    $fake = New-TestExchange -Statuses @($rejected)
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'PRESS' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        (-not $capture.MotionAttempted) -and
        ($fake.Tracker.MotionCommands -eq 0) -and
        ($fake.Tracker.StopCommands -eq 0) -and
        (-not $capture.DataCaptured)) `
        -Message 'admission rejection sent a motion command'

    $successStatuses = @($idle, $idle, $idle, $idle, $pressActive, $idle, $idle, $idle, $idle)
    $fake = New-TestExchange -Statuses $successStatuses
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'PRESS' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    $motionEvent = $fake.Tracker.Events.IndexOf('MOTION')
    $stopEvent = $fake.Tracker.Events.IndexOf('STOP')
    Assert-SelfTest -Condition (
        $capture.DataCaptured -and
        ($fake.Tracker.MotionCommands -eq 1) -and
        ($fake.Tracker.StopCommands -eq 1) -and
        ($motionEvent -ge 0) -and ($stopEvent -gt $motionEvent)) `
        -Message 'successful capture did not send exactly one motion followed by STOP'

    $lostAcknowledgementStatuses = @($idle, $idle, $idle, $idle, $idle)
    $fake = New-TestExchange -Statuses $lostAcknowledgementStatuses `
        -LoseMotionAcknowledgement
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'PRESS' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        ($fake.Tracker.MotionCommands -eq 1) -and
        ($fake.Tracker.StopCommands -eq 1) -and
        (-not $capture.DataCaptured) -and $capture.StopAttempted) `
        -Message 'lost motion acknowledgement was retried or skipped STOP'

    $faultStatus = New-TestStatus -Override @{ Fault = 1; FaultDetail = 2 }
    $faultStatuses = @($idle, $idle, $idle, $idle, $faultStatus, $idle)
    $fake = New-TestExchange -Statuses $faultStatuses
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'PRESS' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        (-not $capture.DataCaptured) -and $capture.StopAttempted -and
        ($fake.Tracker.StopCommands -eq 1)) `
        -Message 'fault did not cause cleanup and incomplete reporting'

    $timeoutStatuses = @($idle, $idle, $idle, $idle, $idle)
    $fake = New-TestExchange -Statuses $timeoutStatuses -TimeoutOnStatusRead 5
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'PRESS' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        (-not $capture.DataCaptured) -and $capture.StopAttempted -and
        ($fake.Tracker.StopCommands -eq 1)) `
        -Message 'status timeout did not cause cleanup and incomplete reporting'

    $missingActiveStatuses = @($idle, $idle, $idle, $idle, $idle, $idle, $idle, $idle, $idle, $idle, $idle)
    $fake = New-TestExchange -Statuses $missingActiveStatuses
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'RELEASE' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        (-not $capture.DataCaptured) -and (-not $capture.ActiveObserved) -and
        $capture.StopAttempted -and ($fake.Tracker.MotionCommands -eq 1) -and
        ($fake.Tracker.StopCommands -eq 1)) `
        -Message 'missing active observation did not clean up without a second pulse'

    $missingTransitionMetrics = Get-CaptureMetrics `
        -Samples @(
            (New-MetricSample 100 0 'Baseline'),
            (New-MetricSample 120 10 'MotionObservation' 'ActiveObserved')) `
        -CaptureDirection 'PRESS' -CompletedPostOff $false -FinalWindowMs 10
    Assert-SelfTest -Condition (
        ($null -eq $missingTransitionMetrics.PressureAtObservedOff) -and
        ($null -eq $missingTransitionMetrics.EndWindowMedian) -and
        ($null -eq $missingTransitionMetrics.SettledDeltaEstimate) -and
        ($null -eq $missingTransitionMetrics.ResidualChangeEstimate)) `
        -Message 'missing transition produced fabricated metrics instead of N/A'

    # Exercise the RELEASE active-state path in the shared capture state machine.
    $releaseStatuses = @($idle, $idle, $idle, $idle, $releaseActive, $idle, $idle, $idle, $idle)
    $fake = New-TestExchange -Statuses $releaseStatuses
    $clock = New-TestClock
    $capture = Invoke-OnePulseCapture -Exchange $fake.Exchange `
        -GetElapsedMs $clock.GetElapsedMs `
        -SleepMilliseconds $clock.SleepMilliseconds `
        -CaptureDirection 'RELEASE' `
        -AttestedFirmwareSha256 $script:ExpectedFirmwareSha256 -Options $options
    Assert-SelfTest -Condition (
        $capture.DataCaptured -and ($fake.Tracker.MotionCommands -eq 1) -and
        ($fake.Tracker.StopCommands -eq 1)) `
        -Message 'RELEASE capture path failed'

    Write-Output 'CAPTURE_PRESSURE_RESPONSE_SELF_TEST=PASS'
}

function Show-Usage {
    Write-Output 'Usage:'
    Write-Output '  .\tools\capture_pressure_response.ps1 -ListPorts'
    Write-Output '  .\tools\capture_pressure_response.ps1 -SelfTest'
    Write-Output '  .\tools\capture_pressure_response.ps1 -Port COMx -Direction PRESS|RELEASE -ConfirmSinglePulse -ConfirmedFirmwareSha256 <SHA256> -OutputCsv <path>'
}

if ($LibraryOnly) { return }

$hasCaptureArgument =
    $PSBoundParameters.ContainsKey('Port') -or
    $PSBoundParameters.ContainsKey('Direction') -or
    $PSBoundParameters.ContainsKey('ConfirmSinglePulse') -or
    $PSBoundParameters.ContainsKey('ConfirmedFirmwareSha256') -or
    $PSBoundParameters.ContainsKey('OutputCsv')
$modeCount = [int][bool]$ListPorts + [int][bool]$SelfTest + [int][bool]$hasCaptureArgument

if ($modeCount -eq 0) {
    Show-Usage
    return
}
if ($modeCount -ne 1) {
    throw '-ListPorts, -SelfTest, and motion capture are mutually exclusive modes.'
}
if ($ListPorts) {
    [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    return
}
if ($SelfTest) {
    Invoke-CaptureSelfTest
    return
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    throw 'Motion capture requires an explicit -Port. There is no default COM port.'
}
if ([string]::IsNullOrWhiteSpace($Direction)) {
    throw 'Motion capture requires -Direction PRESS or RELEASE.'
}
if (-not $ConfirmSinglePulse) {
    throw 'Motion capture requires the explicit -ConfirmSinglePulse switch.'
}
if ([string]::IsNullOrWhiteSpace($ConfirmedFirmwareSha256) -or
    ($ConfirmedFirmwareSha256 -ine $script:ExpectedFirmwareSha256)) {
    throw ("-ConfirmedFirmwareSha256 must match the GuardFixV5 candidate hash: {0}" -f
           $script:ExpectedFirmwareSha256)
}
if ([string]::IsNullOrWhiteSpace($OutputCsv)) {
    throw 'Motion capture requires an explicit -OutputCsv path.'
}

$fullOutputPath = [IO.Path]::GetFullPath($OutputCsv)
if (Test-Path -LiteralPath $fullOutputPath) {
    throw "Refusing to overwrite existing output: $fullOutputPath"
}
$outputDirectory = [IO.Path]::GetDirectoryName($fullOutputPath)
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    throw "Output directory does not exist: $outputDirectory"
}

$Direction = $Direction.ToUpperInvariant()
Write-Output 'FIRMWARE_ATTESTATION=OPERATOR_SUPPLIED_NOT_READ_FROM_MCU'
Write-Output 'The supplied SHA-256 is operator attestation; this tool does not identify firmware from the MCU.'
Write-Output ("EXPECTED_FIRMWARE={0}" -f $script:ExpectedFirmwareName)
Write-Output ("EXPECTED_PROFILE={0}" -f $script:ExpectedProfile)
Write-Output 'GUARDFIX_V5_HARDWARE_STATUS=NOT_TESTED'

$options = [pscustomobject]@{
    BaselineMs = 500
    PollPeriodMs = 10
    PostOutputOffMs = 2000
    FinalWindowMs = 300
    SerialTimeoutMs = 100
    MotionDeadlineMs = 500
}

$serial = $null
$result = $null
$topLevelError = $null
try {
    $serial = New-Object System.IO.Ports.SerialPort
    $serial.PortName = $Port
    $serial.BaudRate = 115200
    $serial.DataBits = 8
    $serial.Parity = [System.IO.Ports.Parity]::None
    $serial.StopBits = [System.IO.Ports.StopBits]::One
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = $options.SerialTimeoutMs
    $serial.WriteTimeout = $options.SerialTimeoutMs
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.Open()

    $captureWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $getElapsed = { return [long]$captureWatch.ElapsedMilliseconds }.GetNewClosure()
    $sleep = { param([int]$Milliseconds) Start-Sleep -Milliseconds $Milliseconds }
    $exchange = {
        param([byte[]]$Request, [int]$TimeoutMs)
        return Invoke-SerialExchange -Serial $serial -Request $Request -TimeoutMs $TimeoutMs
    }.GetNewClosure()

    $result = Invoke-OnePulseCapture -Exchange $exchange `
        -GetElapsedMs $getElapsed -SleepMilliseconds $sleep `
        -CaptureDirection $Direction `
        -AttestedFirmwareSha256 $ConfirmedFirmwareSha256.ToUpperInvariant() `
        -Options $options
}
catch {
    $topLevelError = $_.Exception.Message
}
finally {
    try {
        if (($null -ne $serial) -and $serial.IsOpen) {
            $serial.Close()
        }
    }
    catch {
        $closeError = "Serial port close failed: $($_.Exception.Message)"
        $topLevelError = (($topLevelError, $closeError | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        }) -join '; ')
    }
    if ($null -ne $serial) {
        try {
            $serial.Dispose()
        }
        catch {
            $disposeError = "Serial port disposal failed: $($_.Exception.Message)"
            $topLevelError = (($topLevelError, $disposeError | Where-Object {
                -not [string]::IsNullOrWhiteSpace($_)
            }) -join '; ')
        }
    }
}

if ($null -eq $result) {
    $result = [pscustomobject][ordered]@{
        Samples = New-Object System.Collections.ArrayList
        MotionAttempted = $false
        MotionAcknowledged = $false
        ActiveObserved = $false
        OffObserved = $false
        CompletedPostOff = $false
        StopAttempted = $false
        ShutdownVerified = $false
        DataCaptured = $false
        Detail = $topLevelError
        OffSample = $null
    }
}
elseif ($null -ne $topLevelError) {
    $result.DataCaptured = $false
    $result.Detail = (($result.Detail, $topLevelError | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    }) -join '; ')
}

$runResult = if ($result.DataCaptured) { 'DATA_CAPTURED' } else { 'INCOMPLETE_CAPTURE' }
foreach ($sample in $result.Samples) {
    $sample.RunResult = $runResult
    $sample.RunDetail = $result.Detail
}

$csvError = $null
try {
    Export-CaptureCsv -Samples $result.Samples -Path $fullOutputPath
}
catch {
    $csvError = $_.Exception.Message
    $result.DataCaptured = $false
    $runResult = 'INCOMPLETE_CAPTURE'
}

$metrics = Get-CaptureMetrics -Samples $result.Samples `
    -CaptureDirection $Direction `
    -CompletedPostOff $result.CompletedPostOff `
    -FinalWindowMs $options.FinalWindowMs

Write-Output ("BeforePressure={0}" -f (Format-MetricValue $metrics.BeforePressure))
Write-Output ("ObservedMax={0}" -f (Format-MetricValue $metrics.ObservedMax))
Write-Output ("ObservedMin={0}" -f (Format-MetricValue $metrics.ObservedMin))
Write-Output ("PressureAtObservedOff={0}" -f (Format-MetricValue $metrics.PressureAtObservedOff))
Write-Output ("EndWindowMedian={0}" -f (Format-MetricValue $metrics.EndWindowMedian))
Write-Output ("FinalPressure={0}" -f (Format-MetricValue $metrics.FinalPressure))
Write-Output ("SettledDeltaEstimate={0}" -f (Format-MetricValue $metrics.SettledDeltaEstimate))
Write-Output ("ResidualChangeEstimate={0}" -f (Format-MetricValue $metrics.ResidualChangeEstimate))
Write-Output "FIELD_RESULT=$runResult"
if ($null -eq $csvError) {
    Write-Output "OUTPUT_CSV=$fullOutputPath"
}
else {
    Write-Output "OUTPUT_CSV_ERROR=$csvError"
}
Write-Output 'Pressure is not assumed calibrated in Newtons.'
Write-Output 'Repeated polls can contain the same sensor update; observed extrema can miss the true peak.'
Write-Output 'Observed output-off timing has polling and communication uncertainty.'
Write-Output 'The final-window median does not prove mechanical settling.'
Write-Output 'PC polling and cleanup are not safety-rated protection and do not replace MCU stop paths or a physical emergency stop.'

if (-not $result.DataCaptured) {
    if ($result.MotionAttempted -and (-not $result.ShutdownVerified)) {
        Write-Output 'SHUTDOWN_NOT_VERIFIED: Use the physical emergency stop or remove motor power immediately.'
    }
    $failureDetail = (($result.Detail, $csvError | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    }) -join '; ')
    throw "Capture incomplete: $failureDetail"
}
