param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("AUTO_APPROACH", "AUTO_SETTLE", "AUTO_PULSE", "AUTO_HOLD")]
    [string]$TargetState,

    [Parameter(Mandatory = $true)]
    [ValidateRange(100, 60000)]
    [int]$Target,

    [string]$Port = "COM6",

    [switch]$CheckOnly,

    [switch]$ReadPressureFrame,

    [switch]$FastStop,

    [switch]$TrafficStop,

    [switch]$SensorLoss,

    [ValidateRange(0, 60)]
    [int]$ObserveSeconds = 0,

    [Parameter(Mandatory = $true)]
    [switch]$ConfirmMotorPowerDisconnected
)

$ErrorActionPreference = "Stop"

if (-not $ConfirmMotorPowerDisconnected) {
    throw "Refusing to send START: confirm the motor power is physically disconnected."
}

$stateCodes = @{
    AUTO_APPROACH = 4
    AUTO_SETTLE   = 5
    AUTO_PULSE    = 6
    AUTO_HOLD     = 7
}

function Add-ModbusCrc {
    param([byte[]]$Payload)

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

function ConvertTo-HexString {
    param([byte[]]$Bytes)
    return (($Bytes | ForEach-Object { "{0:X2}" -f $_ }) -join " ")
}

function Test-ModbusCrc {
    param([byte[]]$Frame)

    if ($Frame.Length -lt 4) {
        return $false
    }
    [byte[]]$payload = $Frame[0..($Frame.Length - 3)]
    [byte[]]$expected = Add-ModbusCrc $payload
    return (($expected[-2] -eq $Frame[-2]) -and
            ($expected[-1] -eq $Frame[-1]))
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
        [byte[]]$Bytes,
        [int]$Offset
    )

    return (([int]$Bytes[$Offset] -shl 8) -bor [int]$Bytes[$Offset + 1])
}

function Read-SerialResponse {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$TimeoutMs = 500
    )

    $bytes = [System.Collections.Generic.List[byte]]::new()
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $lastByteAt = -1L

    while ($watch.ElapsedMilliseconds -lt $TimeoutMs) {
        $available = $Serial.BytesToRead
        if ($available -gt 0) {
            [byte[]]$chunk = New-Object byte[] $available
            $read = $Serial.Read($chunk, 0, $chunk.Length)
            for ($i = 0; $i -lt $read; ++$i) {
                $bytes.Add($chunk[$i])
            }
            $lastByteAt = $watch.ElapsedMilliseconds
        }
        elseif (($bytes.Count -gt 0) -and
                (($watch.ElapsedMilliseconds - $lastByteAt) -ge 5)) {
            break
        }
        Start-Sleep -Milliseconds 1
    }

    if ($bytes.Count -eq 0) {
        throw "No Modbus response received within ${TimeoutMs} ms."
    }
    return $bytes.ToArray()
}

function Invoke-ModbusRequest {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [byte[]]$Payload,
        [int]$TimeoutMs = 500
    )

    [byte[]]$request = Add-ModbusCrc $Payload
    $Serial.DiscardInBuffer()
    Write-Host ("TX  " + (ConvertTo-HexString $request))
    $Serial.Write($request, 0, $request.Length)
    [byte[]]$response = Read-SerialResponse $Serial $TimeoutMs
    Write-Host ("RX  " + (ConvertTo-HexString $response))

    if (-not (Test-ModbusCrc $response)) {
        throw "Response CRC is invalid."
    }
    if (($response[1] -band 0x80) -ne 0) {
        $exception = if ($response.Length -ge 3) { $response[2] } else { 0xFF }
        throw ("Modbus exception: function=0x{0:X2}, code=0x{1:X2}" -f
               $response[1], $exception)
    }
    return $response
}

function Read-MachineStatus {
    param([System.IO.Ports.SerialPort]$Serial)

    [byte[]]$response = Invoke-ModbusRequest $Serial ([byte[]](0x01, 0x04, 0x00, 0x00, 0x00, 0x05))
    if (($response.Length -ne 15) -or ($response[1] -ne 0x04) -or
        ($response[2] -ne 0x0A)) {
        throw "Unexpected FC04 response length or byte count."
    }

    return [pscustomobject]@{
        Pressure = Read-U16BE $response 3
        Raw      = Read-U16BE $response 5
        State    = Read-U16BE $response 7
        Fault    = Read-U16BE $response 9
        Detail   = Read-U16BE $response 11
    }
}

function Read-LastPressureFrame {
    param([System.IO.Ports.SerialPort]$Serial)

    [byte[]]$response = Invoke-ModbusRequest $Serial ([byte[]](0x01, 0x04, 0x00, 0x0B, 0x00, 0x07))
    if (($response.Length -ne 19) -or ($response[1] -ne 0x04) -or
        ($response[2] -ne 0x0E)) {
        throw "Unexpected pressure-frame diagnostic response."
    }

    [byte[]]$frame = New-Object byte[] 7
    for ($index = 0; $index -lt $frame.Length; ++$index) {
        $high = $response[3 + ($index * 2)]
        $low = $response[4 + ($index * 2)]
        if ($high -ne 0) {
            throw "Pressure-frame diagnostic byte exceeded 8 bits."
        }
        $frame[$index] = $low
    }
    Write-Host ("Last valid pressure frame: " + (ConvertTo-HexString $frame)) -ForegroundColor Cyan
    return $frame
}

function Write-Target {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$Value
    )

    [byte[]]$payload = [byte[]](
        0x01, 0x06, 0x00, 0x00,
        (($Value -shr 8) -band 0xFF),
        ($Value -band 0xFF)
    )
    [byte[]]$response = Invoke-ModbusRequest $Serial $payload
    if (($response.Length -ne 8) -or
        (-not (Test-ByteArraysEqual $response (Add-ModbusCrc $payload)))) {
        throw "Target write was not echoed correctly."
    }
}

function Send-Coil {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [bool]$On
    )

    $coilHigh = if ($On) { 0xFF } else { 0x00 }
    [byte[]]$payload = [byte[]](0x01, 0x05, 0x00, 0x01, $coilHigh, 0x00)
    [byte[]]$response = Invoke-ModbusRequest $Serial $payload
    if (($response.Length -ne 8) -or
        (-not (Test-ByteArraysEqual $response (Add-ModbusCrc $payload)))) {
        throw "Coil write was not echoed correctly."
    }
}

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    115200,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 100
$serial.WriteTimeout = 100
$startSent = $false

try {
    $serial.Open()
    Write-Host "Opened $Port at 115200 8N1."

    $initial = Read-MachineStatus $serial
    Write-Host ("Initial: pressure={0}, raw={1}, state={2}, fault={3}, detail={4}" -f
        $initial.Pressure, $initial.Raw, $initial.State,
        $initial.Fault, $initial.Detail)
    if (($initial.State -ne 1) -or ($initial.Fault -ne 0)) {
        throw "Device must be IDLE with no fault before this test."
    }

    if ($ReadPressureFrame) {
        $null = Read-LastPressureFrame $serial
    }

    if ($ObserveSeconds -gt 0) {
        Write-Host ("Observing pressure for {0} seconds; no target, START, or STOP command will be sent." -f $ObserveSeconds)
        for ($second = 1; $second -le $ObserveSeconds; ++$second) {
            Start-Sleep -Seconds 1
            $observed = Read-MachineStatus $serial
            Write-Host ("T+{0}s: pressure={1}, raw={2}, state={3}, fault={4}, detail={5}" -f
                $second, $observed.Pressure, $observed.Raw,
                $observed.State, $observed.Fault, $observed.Detail)
        }
        Write-Host "PRESSURE_STARTUP_OBSERVATION=COMPLETE" -ForegroundColor Green
        return
    }

    if ($CheckOnly) {
        Write-Host "PRECHECK=PASS; no target, START, or STOP command was sent." -ForegroundColor Green
        return
    }

    switch ($TargetState) {
        "AUTO_APPROACH" {
            if ($initial.Pressure -ge 100) {
                throw "AUTO_APPROACH requires initial pressure below contact threshold 100."
            }
        }
        "AUTO_SETTLE" {
            if ($initial.Pressure -lt 100) {
                throw "AUTO_SETTLE requires initial pressure at or above contact threshold 100."
            }
        }
        "AUTO_PULSE" {
            if (($initial.Pressure -lt 100) -or
                ($initial.Pressure -ge ($Target - 5))) {
                throw "AUTO_PULSE requires contact pressure below target minus 5 counts."
            }
        }
        "AUTO_HOLD" {
            if ([Math]::Abs($initial.Pressure - $Target) -gt 5) {
                throw "AUTO_HOLD requires initial pressure within target +/- 5 counts."
            }
        }
    }

    Write-Target $serial $Target
    Send-Coil $serial $true
    $startSent = $true

    if ($FastStop) {
        Write-Host "START echoed. Sending STOP immediately."
        Send-Coil $serial $false
        $startSent = $false
        Start-Sleep -Milliseconds 20
        $final = Read-MachineStatus $serial
        Write-Host ("Final: pressure={0}, state={1}, fault={2}, detail={3}" -f
            $final.Pressure, $final.State, $final.Fault, $final.Detail)
        if (($final.State -ne 1) -or ($final.Fault -ne 0) -or
            ($final.Detail -ne 0)) {
            throw "Fast STOP did not return the device to fault-free IDLE."
        }
        Write-Host "FAST_START_STOP_TEST=PASS" -ForegroundColor Green
        return
    }

    if ($TrafficStop) {
        Write-Host "START echoed. Sending read traffic before STOP."
        for ($trafficIndex = 0; $trafficIndex -lt 3; ++$trafficIndex) {
            $status = Read-MachineStatus $serial
            Write-Host ("Traffic {0}: pressure={1}, state={2}, fault={3}, detail={4}" -f
                ($trafficIndex + 1), $status.Pressure, $status.State,
                $status.Fault, $status.Detail)
            if ($status.Fault -ne 0) {
                throw "Device entered fault during read traffic."
            }
        }
        Write-Host "Sending STOP under read traffic."
        Send-Coil $serial $false
        $startSent = $false
        Start-Sleep -Milliseconds 20
        $final = Read-MachineStatus $serial
        Write-Host ("Final: pressure={0}, state={1}, fault={2}, detail={3}" -f
            $final.Pressure, $final.State, $final.Fault, $final.Detail)
        if (($final.State -ne 1) -or ($final.Fault -ne 0) -or
            ($final.Detail -ne 0)) {
            throw "Traffic STOP did not return the device to fault-free IDLE."
        }
        Write-Host "MODBUS_TRAFFIC_STOP_TEST=PASS" -ForegroundColor Green
        return
    }

    $wantedState = $stateCodes[$TargetState]
    $pollWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $captured = $null
    while ($pollWatch.ElapsedMilliseconds -lt 1500) {
        $status = Read-MachineStatus $serial
        Write-Host ("Poll: {0} ms, pressure={1}, state={2}, fault={3}, detail={4}" -f
            $pollWatch.ElapsedMilliseconds, $status.Pressure,
            $status.State, $status.Fault, $status.Detail)
        if ($status.Fault -ne 0) {
            throw "Device entered fault before target state was captured."
        }
        if ($status.State -eq $wantedState) {
            $captured = $status
            break
        }
    }

    if ($null -eq $captured) {
        throw "Target state $TargetState was not observed within 1500 ms."
    }

    if ($SensorLoss) {
        Write-Host "Captured $TargetState. Disconnect the pressure sensor now, then press Enter."
        [void][Console]::ReadLine()
        $lossWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $faulted = $null
        while ($lossWatch.ElapsedMilliseconds -lt 3000) {
            $status = Read-MachineStatus $serial
            Write-Host ("SensorLoss: {0} ms, pressure={1}, state={2}, fault={3}, detail={4}" -f
                $lossWatch.ElapsedMilliseconds, $status.Pressure,
                $status.State, $status.Fault, $status.Detail)
            if (($status.State -eq 9) -and ($status.Fault -eq 2)) {
                $faulted = $status
                break
            }
            Start-Sleep -Milliseconds 50
        }
        if ($null -eq $faulted) {
            throw "Sensor loss did not enter pressure-sensor FAULT within 3000 ms."
        }
        Write-Host "SENSOR_LOSS_ACTIVE_TEST=PASS" -ForegroundColor Green
        $startSent = $false
        return
    }

    Write-Host "Captured $TargetState. Sending STOP immediately."
    Send-Coil $serial $false
    $startSent = $false
    Start-Sleep -Milliseconds 20
    $final = Read-MachineStatus $serial
    Write-Host ("Final: pressure={0}, state={1}, fault={2}, detail={3}" -f
        $final.Pressure, $final.State, $final.Fault, $final.Detail)
    if (($final.State -ne 1) -or ($final.Fault -ne 0) -or ($final.Detail -ne 0)) {
        throw "STOP did not return the device to fault-free IDLE."
    }

    Write-Host "STOP_PRIORITY_TEST=PASS ($TargetState)" -ForegroundColor Green
}
catch {
    if ($serial.IsOpen -and $startSent) {
        try {
            Write-Warning "Test failed after START; sending best-effort STOP."
            Send-Coil $serial $false
        }
        catch {
            Write-Warning "Best-effort STOP also failed; use hardware RESET."
        }
    }
    throw
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
}
