[CmdletBinding()]
param(
    [switch]$SelfTest,
    [switch]$ApproachOnly,
    [string]$Port,
    [ValidateRange(20,275)][int]$Target = 250,
    [switch]$ConfirmStaticTest,
    [switch]$ConfirmMechanicalLimitChecked,
    [string]$ConfirmedFirmwareSha256,
    [string]$OutputCsv,
    [ValidateRange(5,60)][int]$MaximumSeconds = 15
)
$ErrorActionPreference = 'Stop'
$autoArguments = @{SelfTest=$SelfTest; ApproachOnly=$ApproachOnly; Port=$Port; Target=$Target; ConfirmStaticTest=$ConfirmStaticTest;
    ConfirmMechanicalLimitChecked=$ConfirmMechanicalLimitChecked; ConfirmedFirmwareSha256=$ConfirmedFirmwareSha256;
    OutputCsv=$OutputCsv; MaximumSeconds=$MaximumSeconds}
# Reuse the existing CRC, length-aware RTU exchange, serial port I/O, pressure
# safety/admission, STOP coil, fake serial and clock helpers. No new protocol.
. "$PSScriptRoot/capture_pressure_response.ps1" -LibraryOnly
foreach ($entry in $autoArguments.GetEnumerator()) { Set-Variable -Name $entry.Key -Value $entry.Value }
$script:ExpectedProfile = 'PressBoostRetain1 far-band effective-rise boost retention + ApproachMeasure1 search; Ki=0; NOT TUNED'

function Read-AutoWords {
    param([scriptblock]$Exchange, [int]$Address, [int]$Count)
    [byte[]]$payload = @(1,4,($Address -shr 8),($Address -band 255),0,$Count)
    [byte[]]$frame = @(Invoke-ModbusRequest -Exchange $Exchange -Payload $payload -ExpectedFunction 4 -TimeoutMs 100)
    if ($frame.Length -ne (5 + 2*$Count) -or $frame[2] -ne 2*$Count) { throw 'Diagnostic register length mismatch' }
    for ($i=0; $i -lt $Count; ++$i) { Read-U16BE -Bytes $frame -Offset (3+2*$i) }
}

function Join-AutoWords {
    param($Words)
    [uint64]$v = 0
    foreach ($word in $Words) { $v = ($v -shl 16) -bor [uint64]$word }
    return $v
}

function Read-AutoStatus {
    param([scriptblock]$Exchange, [scriptblock]$Clock, [string]$Phase, [string]$Hash)
    # Preserve the 11-register request limit. Fence the multi-request read with
    # sample + request identities; a changing snapshot is logged but not counted.
    $before = @(Read-AutoWords $Exchange 0x14 11)
    $s = Read-MachineStatus -Exchange $Exchange -GetElapsedMs $Clock -Phase $Phase `
        -CaptureDirection AUTO -AttestedFirmwareSha256 $Hash -TimeoutMs 100 -RequestedPollPeriodMs 20
    $tail = @(Read-AutoWords $Exchange 0x1F 6)
    $after = @(Read-AutoWords $Exchange 0x14 11)
    $coherent = ($before -join ',') -ceq ($after -join ',')
    $s | Add-Member -NotePropertyMembers ([ordered]@{
        Target = $tail[2]
        DeviceSampleSequence = (Join-AutoWords $before[0..3]).ToString()
        DeviceReceivedMs = Join-AutoWords $before[4..5]
        RequestDirection = $(if ($before[6] -eq 0) {'PRESS'} else {'RELEASE'})
        RequestCommandMv = $before[7]
        RequestDurationMs = $before[8]
        RequestSequence = Join-AutoWords $before[9..10]
        RequestAtDeviceMs = Join-AutoWords $tail[0..1]
        Capability = $tail[3]
        DeviceNowMs = Join-AutoWords $tail[4..5]
        CoherentSnapshot = $coherent
        PreflightAttempt = 0
        NewSensorSample = $false
        RequestNumberSinceStart = $null
        CoarseApproachRequestObserved = $false
        StateTransitionObserved = ''
        DataSource = 'DEVICE_REGISTER_READBACK'
    })
    return $s
}

function Set-AutoTarget {
    param([scriptblock]$Exchange, [int]$Value)
    [byte[]]$payload = @(1,6,0,0,($Value -shr 8),($Value -band 255))
    [byte[]]$reply = @(Invoke-ModbusRequest -Exchange $Exchange -Payload $payload -ExpectedFunction 6 -TimeoutMs 100)
    if (-not (Test-ByteArraysEqual $reply (Add-ModbusCrc $payload))) { throw 'Target write echo mismatch' }
}

function Read-AutoPreflightStatus {
    param([scriptblock]$Exchange, [scriptblock]$Clock, [scriptblock]$Sleep,
        [ValidateSet('BASELINE','TARGET_READBACK')][string]$Phase, [string]$Hash,
        [System.Collections.ArrayList]$Rows)
    # Ten complete attempts per phase; only incoherence permits a reread.
    # Safety/capability failures and transport errors abort immediately. Never
    # retry a write/START, reuse pieces of a snapshot, or apply this to AUTO.
    for ($attempt=1; $attempt -le 10; ++$attempt) {
        $s = Read-AutoStatus $Exchange $Clock $Phase $Hash
        $s.PreflightAttempt=$attempt
        [void]$Rows.Add($s)
        $admission = Get-AdmissionError $s
        if ($null -ne $admission) { throw $admission }
        if ($s.Capability -ne 0xA701) { throw 'AutoTarget V1 capability required; no AUTO_START sent' }
        if ($s.CoherentSnapshot) { return $s }
        if ($attempt -lt 10) { & $Sleep 20 }
    }
    throw "$Phase coherent snapshot unavailable after 10 complete attempts; no AUTO_START sent"
}

function Get-AutoMetrics {
    param($Samples, [long]$StartedMs, [int]$TargetValue)
    $valid = @($Samples | Where-Object { $_.Phase -eq 'AUTO' -and $_.CoherentSnapshot -and
        $_.NewSensorSample -and $_.PressureFreshValid -and $_.ControlPressure -ne 65535 })
    $peak = $null; $last = $null; $first = $null
    [long]$longestHold = 0; [long]$holdDuration = 0; [long]$stableDuration = 0
    $previous = $null; $previousHold = $false; $previousStable = $false
    foreach ($s in $valid) {
        $p = [int]$s.ControlPressure
        if ($null -eq $peak -or $p -gt $peak) { $peak = $p }
        $last = $s
        $inside = [Math]::Abs($p - $TargetValue) -le 5
        if ($null -eq $first -and $inside) { $first = $s.ResponseReceivedElapsedMs - $StartedMs }
        $healthy = $s.Fault -eq 0 -and $s.FaultDetail -eq 0
        $hold = $healthy -and $s.MachineState -eq 7 -and $s.PhysicalOutputDisabled -and
            -not $s.MotorLogicalActive -and [Math]::Abs($p - $TargetValue) -le 10
        $stable = $hold -and $inside
        [long]$gap = 0
        if ($null -ne $previous) {
            $gap = ([long]$s.DeviceReceivedMs - [long]$previous.DeviceReceivedMs + 4294967296L) % 4294967296L
        }
        $continuous = $null -ne $previous -and $gap -gt 0 -and $gap -le 200 -and
            $s.RequestSequence -eq $previous.RequestSequence
        $holdDuration = if ($hold -and $previousHold -and $continuous) { $holdDuration + $gap } else { 0 }
        $stableDuration = if ($stable -and $previousStable -and $continuous) { $stableDuration + $gap } else { 0 }
        $longestHold = [Math]::Max($longestHold, $holdDuration)
        $previous = $s; $previousHold = $hold; $previousStable = $stable
    }
    $final = @()
    if ($null -ne $last) {
        $final = @($valid | Where-Object {
            $_.ResponseReceivedElapsedMs -ge ($last.ResponseReceivedElapsedMs - 1000) })
    }
    $span = $null; $median = $null
    if ($final.Count -gt 0) {
        $values = @($final | ForEach-Object { [double]$_.ControlPressure })
        $stats = $values | Measure-Object -Minimum -Maximum
        $span = $stats.Maximum - $stats.Minimum
        $median = Get-Median $values
    }
    [pscustomobject]@{
        UniqueSamples=$valid.Count; ObservedPeak=$peak; FirstInToleranceObservedMs=$first
        FinalWindowMedian=$median; FinalWindowPeakToPeak=$span
        LongestObservedContinuousHoldMs=$longestHold; FinalStableDurationMs=$stableDuration
        Stability=$(if ($stableDuration -ge 3000) {'OBSERVED_STABLE_3S'} else {'NOT_STABLE'})
    }
}

function Invoke-AutoCapture {
    param([scriptblock]$Exchange, [scriptblock]$Clock, [scriptblock]$Sleep,
        [int]$TargetValue, [string]$Hash, [int]$MaxMs, [bool]$StopAtContact = $false)
    $rows = New-Object System.Collections.ArrayList
    $errorText = ''; $stopVerified = $false; [long]$started = & $Clock
    [uint64]$lastSeq = 0
    $approachResult = 'NOT_REQUESTED'
    if ($StopAtContact) { $approachResult = 'CONTACT_NOT_OBSERVED' }
    try {
        $baseline = Read-AutoPreflightStatus $Exchange $Clock $Sleep BASELINE $Hash $rows
        Set-AutoTarget $Exchange $TargetValue
        $ready = Read-AutoPreflightStatus $Exchange $Clock $Sleep TARGET_READBACK $Hash $rows
        if ($ready.Target -ne $TargetValue -or -not $ready.TargetValid -or
            -not $ready.CoherentSnapshot -or $ready.Capability -ne 0xA701) { throw 'Target/capability/coherent readback mismatch' }
        if ($StopAtContact -and $ready.ControlPressure -ge 20) { throw 'Approach-only test requires initial pressure below contact threshold 20; no START sent' }
        $lastSeq = [uint64]$ready.DeviceSampleSequence
        [long]$baselineRequest = $ready.RequestSequence
        $previousState = $ready.MachineState
        $started = & $Clock
        Send-SingleCoil -Exchange $Exchange -Address 1 -Value 0xFF00 -TimeoutMs 100
        while ((& $Clock) - $started -lt $MaxMs) {
            $s = Read-AutoStatus $Exchange $Clock AUTO $Hash
            [void]$rows.Add($s)
            if ($s.CoherentSnapshot) {
                [uint64]$current = $s.DeviceSampleSequence
                if ($current -lt $lastSeq) { throw 'Device sample sequence decreased/reset' }
                $s.NewSensorSample = $current -gt $lastSeq
                $lastSeq = $current
                $s.RequestNumberSinceStart = ([long]$s.RequestSequence - $baselineRequest + 4294967296L) % 4294967296L
                $s.CoarseApproachRequestObserved = $s.RequestNumberSinceStart -gt 0 -and
                    $s.RequestDirection -eq 'PRESS' -and $s.RequestCommandMv -eq 10000 -and
                    $s.RequestDurationMs -in @(10,20)
                if ($s.MachineState -ne $previousState) {
                    $s.StateTransitionObserved = "$previousState -> $($s.MachineState)"
                    $previousState = $s.MachineState
                }
            }
            $safety = Get-SampleSafetyError $s
            if ($null -ne $safety) { throw $safety }
            if ($s.Target -ne $TargetValue -or $s.Capability -ne 0xA701) { throw 'Target/capability changed' }
            if ($s.RawPressure -ge 325) { throw 'Observed experiment abort threshold reached' }
            if ($s.MachineState -notin @(4,5,6,7)) { throw 'Automatic state exited unexpectedly' }
            if ($StopAtContact -and $s.CoherentSnapshot) {
                if ($s.NewSensorSample -and $s.PressureFreshValid -and $s.ControlPressure -ge 20) {
                    $approachResult = 'CONTACT_OBSERVED_STOP_REQUESTED'; break
                }
                if ($s.MachineState -in @(6,7)) {
                    $approachResult = 'FINE_HANDOFF_OBSERVED_STOP_REQUESTED'; break
                }
            }
            $metrics = Get-AutoMetrics $rows $started $TargetValue
            if ($metrics.Stability -eq 'OBSERVED_STABLE_3S') { break }
            & $Sleep 20
        }
    }
    catch { $errorText = $_.Exception.Message }
    finally {
        # STOP is always attempted, including a lost AUTO_START echo or preflight
        # error. Readback is required to call shutdown verified.
        try {
            Send-SingleCoil -Exchange $Exchange -Address 1 -Value 0 -TimeoutMs 100
            $off = Read-AutoStatus $Exchange $Clock STOP_READBACK $Hash
            [void]$rows.Add($off)
            $stopVerified = $off.PhysicalOutputDisabled -and -not $off.MotorLogicalActive -and
                $off.MachineState -in @(1,9) -and $off.PlannedTim2Ccr3 -eq 0 -and $off.PlannedTim3Ccr3 -eq 0
            if (-not $stopVerified) { throw 'STOP output-off readback failed' }
        }
        catch { $errorText += '; STOP: ' + $_.Exception.Message }
    }
    [pscustomobject]@{ Samples=$rows; StartedMs=$started; Detail=$errorText; StopVerified=$stopVerified; ApproachResult=$approachResult }
}

function Get-AutoFieldSummary {
    # Report-only, called AFTER capture. Never changes AUTO sampling or exit rules.
    param($Capture, [int]$TargetValue)
    $metrics = Get-AutoMetrics $Capture.Samples $Capture.StartedMs $TargetValue
    [long]$total = 0; [long]$longest = 0; [long]$segment = 0
    $previous = $null; $previousHold = $false
    foreach ($s in $Capture.Samples) {
        if ($s.Phase -ne 'AUTO') { continue }
        if (-not $s.CoherentSnapshot -or -not $s.PressureFreshValid -or
            $s.ControlPressure -eq 65535 -or $s.RawPressure -eq 65535 -or
            $s.Fault -ne 0 -or $s.FaultDetail -ne 0) {
            $previous=$null; $previousHold=$false; $segment=0; continue
        }
        if (-not $s.NewSensorSample) { continue }
        $hold = $s.MachineState -eq 7 -and $s.PhysicalOutputDisabled -and -not $s.MotorLogicalActive
        [long]$gap = 0
        if ($null -ne $previous) {
            $gap=([long]$s.DeviceReceivedMs - [long]$previous.DeviceReceivedMs + 4294967296L) % 4294967296L
        }
        if ($hold -and $previousHold -and $gap -gt 0 -and $gap -le 200 -and
            [uint64]$s.DeviceSampleSequence -gt [uint64]$previous.DeviceSampleSequence -and
            $s.RequestSequence -eq $previous.RequestSequence) {
            $total += $gap; $segment += $gap; $longest=[Math]::Max($longest,$segment)
        } else { $segment=0 }
        $previous=$s; $previousHold=$hold
    }
    $off = @($Capture.Samples | Where-Object { $_.Phase -eq 'STOP_READBACK' } | Select-Object -Last 1)
    $last = @($Capture.Samples | Where-Object { $_.CoherentSnapshot } | Select-Object -Last 1)
    $offValid = $off.Count -gt 0 -and $off[0].CoherentSnapshot
    [pscustomobject][ordered]@{
        ObservedMaximumPressure = $(if ($null -eq $metrics.ObservedPeak) {'NOT_OBSERVED'} else {$metrics.ObservedPeak})
        FirstObservedWithinTargetPlusMinus5Ms = $(if ($null -eq $metrics.FirstInToleranceObservedMs) {'NOT_OBSERVED'} else {$metrics.FirstInToleranceObservedMs})
        TotalObservedAutoHoldMs = $total
        LongestObservedAutoHoldMs = $longest
        FinalStopReadbackState = $(if ($offValid) {$off[0].MachineState} else {'UNKNOWN'})
        FinalStopReadbackFault = $(if ($offValid) {$off[0].Fault} else {'UNKNOWN'})
        FinalStopReadbackFaultDetail = $(if ($offValid) {$off[0].FaultDetail} else {'UNKNOWN'})
        FinalStopReadbackCoherent = $offValid
        LatestCoherentPressure = $(if ($last.Count -and $last[0].ControlPressure -ne 65535) {$last[0].ControlPressure} else {'UNKNOWN'})
        LatestCoherentPressureFreshValid = $(if ($last.Count) {$last[0].PressureFreshValid} else {'UNKNOWN'})
        LastCoherentObservedState = $(if ($last.Count) {$last[0].MachineState} else {'UNKNOWN'})
        LastCoherentObservedFault = $(if ($last.Count) {$last[0].Fault} else {'UNKNOWN'})
        LastCoherentObservedFaultDetail = $(if ($last.Count) {$last[0].FaultDetail} else {'UNKNOWN'})
        StopVerified = $Capture.StopVerified
    }
}

function Get-AutoPressRamReportLines {
    # These fields exist ONLY in the unchanged MCU RAM snapshot, not Modbus.
    # Never replace unavailable values with zero/false or infer a pulse count.
    'PressDiagnosticsSource=RAM_NOT_READ_BY_CAPTURE_SCRIPT'
    'After verified STOP: attach without reset/download using the matching ELF; p g_sd700_approach_diagnostics.press'
    $fields = [ordered]@{
        PressRequestCount='request_count'; LatestPressRequestSequence='request_sequence'
        LatestRequestedMv='requested_mv'; LatestBaseMv='base_mv'; LatestRequestedBoostMv='boost_mv'
        PulseDurationMs='duration_ms'; CompletedRequestSequence='completed_request_sequence'
        CompletedMv='completed_mv'; CompletionReason='end_reason'
        PressureBeforePulse='before_units'; ObservedSampledPeak='observed_peak_units'
        SettledPressureAfterPulse='after_units'; FeedbackValid='feedback_valid'
        OffFallObserved='observed_off_fall'; BeforeSampleSequence='before_sequence'
        AfterSampleSequence='after_sequence'; AfterReceivedMs='after_received_at_ms'
    }
    foreach ($entry in $fields.GetEnumerator()) {
        "$($entry.Key)=RAM_NOT_READ (g_sd700_approach_diagnostics.press.$($entry.Value))"
    }
    'LatestRequestedBoostMv is the retained boost used by the latest accepted PRESS; live controller boost resets on STOP/fault. It is not a post-STOP live boost measurement.'
    'CompletionReason: 0 NONE, 2 NORMAL, 4 STOP, 5 BACKSTOP, 6 EXECUTOR_ERROR, 7 SAFETY_FAULT. Match completed_request_sequence to the completed pulse; latest request may be newer.'
    'Use settled after/OffFallObserved only with feedback_valid=true. A zero after value when invalid is not a measurement. OFF fall is sampled rise then lower settled pressure, not true instantaneous peak.'
}

function Get-AutoApproachRamReportLines {
    'ApproachMeasurementSource=RAM_NOT_READ_BY_CAPTURE_SCRIPT'
    'After verified STOP: attach with matching ELF without reset/download; p g_sd700_approach_diagnostics'
    $fields = [ordered]@{
        ApproachStartedMs='search_started_at_ms'; ApproachElapsedMs='search_elapsed_ms'
        ApproachMeasurementActive='search_active'; FirstContactLatched='first_contact_latched'
        CoarsePulseCount='pulse_count'; FirstContactCoarsePulseCount='first_contact_pulse_count'
        FirstContactPressure='first_contact_pressure_units'; FirstContactReceivedMs='first_contact_received_at_ms'
        FirstContactDecisionMs='first_contact_decided_at_ms'; FirstContactSampleSequence='first_contact_sample_sequence'
        ApproachRequestedMv='requested_mv'; ApproachRequestedDurationMs='requested_duration_ms'
        ApproachCompletionReason='end_reason'; LatestApproachPressure='pressure_units'
        LatestApproachSampleReceivedMs='sample_received_at_ms'; LatestApproachPressureFresh='pressure_fresh'
    }
    foreach ($entry in $fields.GetEnumerator()) {
        "$($entry.Key)=RAM_NOT_READ (g_sd700_approach_diagnostics.$($entry.Value))"
    }
    'ApproachElapsedMs freezes at first contact MCU receive time or STOP/fault. With FirstContactLatched=false, elapsed and CoarsePulseCount are the measured no-contact lower bound; first-contact fields are NOT MEASURED, even if their RAM value is zero.'
    'FirstContactCoarsePulseCount freezes at first contact; CoarsePulseCount includes later recontact. Already-contacted START has zero approach elapsed/pulses. LatestApproachPressure is the search snapshot, not the final PRESS/HOLD pressure; use LatestCoherentPressure with its freshness flag for the latest PC readback.'
    'PC polls cannot recover exact contact time or coarse count. Retain the post-STOP RAM dump alongside CSV/report and record approximate starting gap separately. No contact/HOLD success is inferred from missing RAM data.'
}

function Get-AutoReportLines {
    param($Capture, [int]$TargetValue, [string]$Hash, [bool]$StopAtContact = $false)
    $metrics = Get-AutoMetrics $Capture.Samples $Capture.StartedMs $TargetValue
    return @(
    "Firmware=$Hash (OPERATOR_ATTESTATION, NOT MCU binary identification)",
    "Target=$TargetValue sensor control units (NOT calibrated N)",
    "StopVerified=$($capture.StopVerified); CaptureError=$($capture.Detail)",
    "ApproachOnly=$([bool]$StopAtContact); ApproachResult=$($capture.ApproachResult)",
    'Preflight: BASELINE/TARGET_READBACK each allow 10 full snapshot attempts, 20 ms between incoherent attempts; safety/transport errors abort. START is never retried.',
    ($metrics | Format-List | Out-String),
    ((Get-AutoFieldSummary $Capture $TargetValue) | Format-List | Out-String),
    'HOLD totals/longest count only adjacent new coherent fresh healthy output-OFF state7 observations with increasing sequence, receive gap <=200 ms and unchanged request. Unknown/incoherent gaps break a span; duplicate polls add no time. These are observed intervals, not exact MCU dwell time.',
    'First within +/-5 is PC observation time since the single START attempt, not exact device band-entry time. Maximum pressure uses qualified AUTO observations only; STOP_READBACK is separate.',
    ((Get-AutoPressRamReportLines) -join [Environment]::NewLine),
    ((Get-AutoApproachRamReportLines) -join [Environment]::NewLine),
    'Observed peak is the maximum qualified PC register observation, NOT the true instantaneous peak.',
    'Final window = last 1000 ms of AUTO observations; tolerance = +/-5; HOLD exit = +/-10.',
    'Observed stability requires 3 seconds in HOLD and +/-5 on new coherent frames, gap <=200 ms, no new request.',
    'Missed frames/transients and polling latency remain unobserved; HOLD alone does not establish stable force.',
    'Request mV/direction/duration are accepted commands, NOT measured terminal voltage. No brake/preload.',
    'PressBoostRetain1: PRESS base 400..1000 mV for error 5..20, then 1000..3000 over error 20..120; capped above 120. All pulses 10 ms/backstop 40 ms.',
    'After two normally completed low-response pulses: +300 mV; extra cap 2000 for error >20, 1000 for error 10..20, zero for error <=10. Qualified rise >=2 clears the low-response count; in the same far band (error >20) it retains existing boost within the cap. Fine/near and band-change resets remain unchanged.',
    'Last PRESS completion/before/observed-peak/settled-after: debugger g_sd700_approach_diagnostics.press after verified STOP; observed_off_fall is sampled evidence, not a measured instantaneous peak.',
    'Coarse first=10000 mV/20 ms; recontact=10000 mV/10 ms; backstops=50/40 ms; OFF settle=50 ms + newer fresh frame; no fixed total approach timeout. Initial search does not consume convergence time; the existing 8000 ms convergence budget starts at first valid MCU contact receive time. Recontact does not reset it.',
    'Approach-only STOP follows PC observation; firmware cancels coarse motion at contact >=20 immediately, then retains existing fine behavior until STOP arrives.',
    'RequestNumberSinceStart counts all accepted motor requests, including fine pulses; PC polling can miss entire coarse pulses and state transitions.',
    'Exact coarse count and last end reason: debugger RAM symbol g_sd700_approach_diagnostics after verified STOP. Completion reason is not exposed by existing Modbus.',
    'No contact or no visually confirmed motion: do not retry automatically or increase limits; retain CSV and verify mechanical movement independently.',
    'Mechanical safe limit must be operator checked before powered testing; abort=325 raw counts is an experiment limit.'
)
}

function Test-AutoTargetFraming {
    [byte[]]$echo = @(Add-ModbusCrc ([byte[]](1,6,0,0,0,250)))
    $cases = @(
        @{Name='FC06_COMPLETE'; Frame=$echo; Chunks=@(); Error=''},
        @{Name='FC06_FRAGMENTED'; Frame=$echo; Chunks=@(1,1,2,1,1,1,1); Error=''},
        @{Name='FC06_CRC'; Frame=([byte[]]$echo.Clone()); Chunks=@(2,3); Error='*CRC is invalid*'},
        @{Name='FC06_ECHO'; Frame=(Add-ModbusCrc ([byte[]](1,6,0,0,0,249))); Chunks=@(1,2); Error='Target write echo mismatch'},
        @{Name='FC06_ADDRESS_ECHO'; Frame=(Add-ModbusCrc ([byte[]](1,6,0,1,0,250))); Chunks=@(); Error='Target write echo mismatch'},
        @{Name='FC06_TRUNCATED'; Frame=([byte[]]$echo[0..6]); Chunks=@(1,2); Error='Partial Modbus response:*expected 8*100 ms*'},
        @{Name='FC06_EMPTY_TIMEOUT'; Frame=([byte[]]@()); Chunks=@(); Error='No Modbus response*100 ms*'},
        @{Name='FC06_EXCEPTION'; Frame=(Add-ModbusCrc ([byte[]](1,0x86,2))); Chunks=@(1,1,1); Error='Modbus exception: function=0x86, code=0x02*'},
        @{Name='FC06_WRONG_FUNCTION'; Frame=(Add-ModbusCrc ([byte[]](1,5,0,0,0,250))); Chunks=@(2,2); Error='Unexpected Modbus function*'},
        @{Name='FC06_WRONG_STATION'; Frame=(Add-ModbusCrc ([byte[]](2,6,0,0,0,250))); Chunks=@(); Error='Unexpected Modbus station*'},
        @{Name='FC06_OVERLONG'; Frame=([byte[]]($echo + @(0))); Chunks=@(); Error='Modbus response exceeded*'},
        @{Name='FC06_BAD_EXCEPTION'; Frame=(Add-ModbusCrc ([byte[]](1,0x85,2))); Chunks=@(); Error='Malformed or unexpected Modbus exception*'}
    )
    $cases[2].Frame[-1] = $cases[2].Frame[-1] -bxor 1
    foreach ($case in $cases) {
        $wireExchange = {
            param([byte[]]$Request, [int]$TimeoutMs)
            Read-TestLengthAwareResponse -Response $case.Frame -ChunkSizes $case.Chunks -TimeoutMs $TimeoutMs
        }
        $errorText = ''
        try { Set-AutoTarget $wireExchange 250 }
        catch { $errorText = $_.Exception.Message }
        Assert-SelfTest ($errorText -like $case.Error) ("$($case.Name): $errorText")
        Write-Output ("LENGTH_AWARE_CASE=$($case.Name) PASS")
    }
    foreach ($chunks in @(@(8), @(1,1,2,1,1,1,1))) {
        $wireExchange = {
            param([byte[]]$Request, [int]$TimeoutMs)
            Read-TestLengthAwareResponse -Response $Request -ChunkSizes $chunks -TimeoutMs $TimeoutMs
        }
        Send-SingleCoil $wireExchange 1 0xFF00 100
    }
    [byte[]]$status = @(New-TestFc04Frame (New-TestStatus))
    $wireExchange = {
        param([byte[]]$Request, [int]$TimeoutMs)
        Read-TestLengthAwareResponse -Response $status -ChunkSizes @(1,1,1,2,7) -TimeoutMs $TimeoutMs
    }
    [byte[]]$reply = @(Invoke-ModbusRequest $wireExchange ([byte[]](1,4,0,0,0,11)) 4 100)
    $decoded = ConvertFrom-Fc04Response $reply
    Assert-SelfTest ($decoded.ControlPressure -eq 1000 -and $decoded.MachineState -eq 1) 'FC04 framed status decode'
    Write-Output 'LENGTH_AWARE_CASE=FC04_FC05_REGRESSION PASS'
}

function Test-AutoFieldReport {
    param($Template)
    $rows=@()
    $states=@(6,7,7,7,6,7,7,7,7,7)
    $pressures=@(30,240,245,250,260,250,250,250,250,250)
    for ($i=0; $i -lt 10; ++$i) {
        $s=$Template.PSObject.Copy()
        $s.Phase='AUTO'; $s.CoherentSnapshot=$true; $s.NewSensorSample=$true
        $s.PressureFreshValid=$true; $s.PhysicalOutputDisabled=$true; $s.MotorLogicalActive=$false
        $s.Fault=0; $s.FaultDetail=0; $s.MachineState=$states[$i]
        $s.ControlPressure=$pressures[$i]; $s.RawPressure=$pressures[$i]
        $s.DeviceSampleSequence=([uint64]($i+1)).ToString(); $s.RequestSequence=1
        $s.DeviceReceivedMs=(4294967096L + 100*$i) % 4294967296L
        $s.ResponseReceivedElapsedMs=100*$i
        $rows += $s
    }
    $off=$rows[-1].PSObject.Copy(); $off.Phase='STOP_READBACK'
    $off.MachineState=9; $off.Fault=5; $off.FaultDetail=8
    $off.ControlPressure=300; $off.RawPressure=300; $off.PressureFreshValid=$false
    $c=[pscustomobject]@{Samples=@($rows)+@($off); StartedMs=50L; Detail='SYNTHETIC_INPUT cycle timeout'; StopVerified=$true; ApproachResult='NOT_REQUESTED'}
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.ObservedMaximumPressure -eq 260 -and $s.FirstObservedWithinTargetPlusMinus5Ms -eq 150 -and
        $s.TotalObservedAutoHoldMs -eq 600 -and $s.LongestObservedAutoHoldMs -eq 400 -and
        $s.FinalStopReadbackFault -eq 5 -and $s.FinalStopReadbackFaultDetail -eq 8 -and $s.StopVerified) 'report max/band/HOLD spans/wrap/final fault; STOP pressure excluded'
    [string[]]$report=@(Get-AutoReportLines $c 250 'SYNTHETIC_INPUT' $false)
    $text=$report -join "`n"
    Assert-SelfTest ($text -notmatch 'System.Object\[\]' -and $text -match 'FinalStopReadbackFaultDetail\s*:\s*8' -and
        $text -match 'TotalObservedAutoHoldMs\s*:\s*600' -and $text -match 'StopVerified=True') 'generated report exposes numeric final results'
    foreach ($name in @('PressRequestCount','LatestRequestedMv','LatestBaseMv','LatestRequestedBoostMv',
        'PulseDurationMs','CompletionReason','PressureBeforePulse','ObservedSampledPeak',
        'SettledPressureAfterPulse','FeedbackValid','OffFallObserved')) {
        Assert-SelfTest ($text.Contains("$name=RAM_NOT_READ (g_sd700_approach_diagnostics.press.")) 'RAM-only result must never be invented from Modbus polls'
    }
    Assert-SelfTest ($s.LatestCoherentPressure -eq 300 -and -not $s.LatestCoherentPressureFreshValid) 'latest coherent readback remains separate from qualified maximum/freshness'
    foreach ($name in @('ApproachElapsedMs','CoarsePulseCount','FirstContactCoarsePulseCount',
        'FirstContactPressure','FirstContactReceivedMs','FirstContactDecisionMs',
        'FirstContactSampleSequence','ApproachRequestedMv','ApproachRequestedDurationMs',
        'ApproachCompletionReason','LatestApproachPressure','FirstContactLatched')) {
        Assert-SelfTest ($text.Contains("$name=RAM_NOT_READ (g_sd700_approach_diagnostics.")) 'approach RAM measurements cannot be fabricated from PC polls'
    }
    Assert-SelfTest ($text.Contains('no-contact lower bound') -and $text.Contains('no fixed total approach timeout')) 'measurement report explains lower bound and scope'
    Write-Output 'APPROACH_MEASURE_REPORT=PASS SYNTHETIC_INPUT; RAM_UNREAD_NOT_INVENTED'
    $duplicate=$rows[2].PSObject.Copy(); $duplicate.NewSensorSample=$false
    $c.Samples=@($rows[0..2])+@($duplicate)+@($rows[3..9])+@($off)
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.TotalObservedAutoHoldMs -eq 600 -and $s.LongestObservedAutoHoldMs -eq 400) 'duplicate poll adds no HOLD time'
    $rows[7].CoherentSnapshot=$false; $c.Samples=@($rows)+@($off)
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.TotalObservedAutoHoldMs -eq 400 -and $s.LongestObservedAutoHoldMs -eq 200) 'incoherence breaks observed HOLD span'
    $rows[7].CoherentSnapshot=$true
    $rows[7].DeviceReceivedMs += 201
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.TotalObservedAutoHoldMs -eq 400 -and $s.LongestObservedAutoHoldMs -eq 200) 'long/backward gaps add no HOLD time'
    $rows[7].DeviceReceivedMs -= 201
    foreach ($i in 7..9) { $rows[$i].RequestSequence=2 }
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.TotalObservedAutoHoldMs -eq 500 -and $s.LongestObservedAutoHoldMs -eq 200) 'new request breaks observed HOLD span'
    $off.CoherentSnapshot=$false
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.FinalStopReadbackFault -eq 'UNKNOWN' -and -not $s.FinalStopReadbackCoherent) 'incoherent final readback is not a qualified fault result'
    $c.Samples=@(); $c.StopVerified=$false
    $s=Get-AutoFieldSummary $c 250
    Assert-SelfTest ($s.ObservedMaximumPressure -eq 'NOT_OBSERVED' -and
        $s.FirstObservedWithinTargetPlusMinus5Ms -eq 'NOT_OBSERVED' -and
        $s.FinalStopReadbackFault -eq 'UNKNOWN' -and -not $s.StopVerified) 'missing data stays unknown, not zero pressure/fault or success'
    Write-Output 'AUTO_FIELD_REPORT_SELF_TEST=PASS SYNTHETIC_INPUT; MAX_BAND_HOLD_FINAL_FAULT_RAM_UNKNOWN'
}

function Test-AutoCapture {
    Test-AutoTargetFraming
    # Complete fake RTU transaction flow, including serial length parsing.
    # Pressure is SYNTHETIC_INPUT; no port is constructed or opened.
    $sim = @{}
    $reset = {
        $sim.Clear()
        foreach ($entry in (@{Ms=0L; Sequence=1L; Received=0L; Target=0; Active=$false;
            StopCount=0; StartCount=0; TargetWrites=0; FailStart=$false; Approach=$false;
            StartedMs=0L; NoContact=$false; ReadPart=0; Stage=''; Attempt=0;
            BaselineReads=0; TargetReads=0; BadBaseline=0; BadReadback=0;
            BadSnapshot=$false; StampAttempts=$false; Override=@{}; OverrideStage='BASELINE'; OverrideAttempt=1;
            WireError=''; WireErrorStage='BASELINE'; WireErrorAttempt=1;
            WireReads=0}).GetEnumerator()) { $sim[$entry.Key]=$entry.Value }
    }
    & $reset
    $clock = { return $sim.Ms }
    $sleep = { param($ms) $sim.Ms += $ms }
    $respond = {
        param([byte[]]$Request, [int]$TimeoutMs)
        Assert-SelfTest (Test-ModbusCrc $Request) 'outgoing synthetic request CRC'
        $sim.Ms += 2
        if ($sim.Ms - $sim.Received -ge 100) { $sim.Sequence++; $sim.Received=$sim.Ms }
        if ($Request[1] -eq 6) { $sim.TargetWrites++; $sim.Target = 256*[int]$Request[4]+$Request[5]; return $Request }
        if ($Request[1] -eq 5) {
            Assert-SelfTest ((Read-U16BE $Request 2) -eq 1) 'only existing START/STOP coil'
            $sim.Active = $Request[4] -ne 0
            if ($sim.Active) { $sim.StartedMs=$sim.Ms; $sim.StartCount++; if ($sim.FailStart) { return [byte[]]@() } }
            else { $sim.StopCount++ }
            return $Request
        }
        $address=256*[int]$Request[2]+$Request[3]; $count=[int]$Request[5]
        Assert-SelfTest ($Request[1] -eq 4 -and $address -eq @(20,0,31,20)[$sim.ReadPart] -and
            $count -eq @(11,11,6,11)[$sim.ReadPart]) 'every attempt reads a complete fenced snapshot in order'
        if ($sim.ReadPart -eq 0) {
            $sim.Stage = if ($sim.Active) {'AUTO'} elseif ($sim.StopCount -gt 0) {'STOP_READBACK'} elseif ($sim.TargetWrites -gt 0) {'TARGET_READBACK'} else {'BASELINE'}
            $sim.BadSnapshot=$false
            if ($sim.Stage -eq 'BASELINE') {
                $sim.BaselineReads++; $sim.Attempt=$sim.BaselineReads
                $sim.BadSnapshot=$sim.Attempt -le $sim.BadBaseline
            }
            if ($sim.Stage -eq 'TARGET_READBACK') {
                $sim.TargetReads++; $sim.Attempt=$sim.TargetReads
                $sim.BadSnapshot=$sim.Attempt -le $sim.BadReadback
            }
        }
        if ($sim.ReadPart -eq 3 -and $sim.BadSnapshot) { $sim.Sequence++; $sim.Received=$sim.Ms }
        $sim.ReadPart=($sim.ReadPart+1)%4
        $values = New-Object int[] 37
        $values[0]=250; $values[1]=250; $values[2]=$(if ($sim.Active) {7} else {1})
        $values[10]=0x71; if ($sim.Target -gt 0) { $values[10] = $values[10] -bor 2 }
        $values[23]=$sim.Sequence; $values[25]=$sim.Received
        $values[33]=$sim.Target; $values[34]=0xA701; $values[36]=$sim.Ms
        if ($sim.StampAttempts -and $sim.Stage -in @('BASELINE','TARGET_READBACK')) {
            $values[0]=100+$sim.Attempt; $values[1]=100+$sim.Attempt
        }
        if ($sim.Approach) {
            $age = $sim.Ms - $sim.StartedMs
            $values[0]=0; $values[1]=0
            if ($sim.Active) {
                $values[27]=10000; $values[28]=20
                $values[30]=[int][Math]::Min(2, [Math]::Floor($age / 100) + 1)
                $values[2]=5
                if ($age -lt 200 -and $age % 100 -lt 20) {
                    $values[2]=4; $values[6]=3; $values[7]=10000
                    $values[10]=($values[10] -band (-bnot 16)) -bor 4
                }
                if ($age -ge 200 -and -not $sim.NoContact) { $values[0]=20; $values[1]=20 }
                if ($age -ge 3000 -and $sim.NoContact) { $values[2]=9; $values[3]=5; $values[4]=6 }
            }
        }
        if ($sim.Stage -eq $sim.OverrideStage -and $sim.Attempt -eq $sim.OverrideAttempt) {
            foreach ($entry in $sim.Override.GetEnumerator()) { $values[[int]$entry.Key]=[int]$entry.Value }
        }
        $payload=New-Object System.Collections.Generic.List[byte]
        $payload.Add(1); $payload.Add(4); $payload.Add([byte](2*$count))
        for ($i=$address; $i -lt $address+$count; $i++) {
            $payload.Add([byte]($values[$i] -shr 8)); $payload.Add([byte]($values[$i] -band 255))
        }
        return ([byte[]](Add-ModbusCrc $payload.ToArray()))
    }
    $exchange = {
        param([byte[]]$Request, [int]$TimeoutMs)
        [byte[]]$response = @(& $respond $Request $TimeoutMs)
        $sim.WireReads++
        if ($sim.WireError -ne '' -and $Request[1] -eq 4 -and $sim.ReadPart -eq 1 -and
            $sim.Stage -eq $sim.WireErrorStage -and $sim.Attempt -eq $sim.WireErrorAttempt) {
            if ($sim.WireError -eq 'CRC') { $response[-1]=$response[-1] -bxor 1 }
            else { $response=[byte[]]@() }
            $sim.ReadPart=0 # aborted snapshot; the subsequent STOP read starts anew
        }
        return Read-TestLengthAwareResponse -Response $response -ChunkSizes @(1,1,2,3) -TimeoutMs $TimeoutMs
    }
    $capture = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
    Assert-SelfTest ($capture.Detail -eq '') ("capture detail: " + $capture.Detail)
    Assert-SelfTest ($sim.StartCount -eq 1 -and $sim.StopCount -eq 1 -and $capture.StopVerified) 'one START then verified STOP'
    $metrics = Get-AutoMetrics $capture.Samples $capture.StartedMs 250
    Assert-SelfTest ($metrics.Stability -eq 'OBSERVED_STABLE_3S' -and $metrics.ObservedPeak -eq 250) 'stable synthetic trace'
    Assert-SelfTest ($metrics.UniqueSamples -lt @($capture.Samples).Count) 'poll dedup'
    Test-AutoFieldReport $capture.Samples[2]
    $duplicates = @($capture.Samples | Where-Object { $_.Phase -eq 'AUTO' } | Select-Object -First 2)
    $metrics = Get-AutoMetrics $duplicates 0 250
    Assert-SelfTest ($metrics.Stability -eq 'NOT_STABLE') 'brief HOLD is not stable'
    $metricRows = @()
    for ($i=0; $i -lt 36; ++$i) {
        $s = $capture.Samples[2].PSObject.Copy()
        $s.Phase='AUTO'; $s.CoherentSnapshot=$true; $s.NewSensorSample=$true
        $s.DeviceReceivedMs=([long]4294967096 + 100*$i) % 4294967296L
        $s.ResponseReceivedElapsedMs=100*$i; $s.RequestSequence=1
        $metricRows += $s
    }
    $metrics=Get-AutoMetrics $metricRows 0 250
    Assert-SelfTest ($metrics.Stability -eq 'OBSERVED_STABLE_3S') 'device timestamp wrap in report'
    $metricRows[-1].ControlPressure=270
    $metrics=Get-AutoMetrics $metricRows 0 250
    Assert-SelfTest ($metrics.Stability -eq 'NOT_STABLE' -and $metrics.ObservedPeak -eq 270) 'final drift is not stable'
    $metricRows[-1].ControlPressure=250
    $metricRows[-1].DeviceReceivedMs += 201
    $metrics=Get-AutoMetrics $metricRows 0 250
    Assert-SelfTest ($metrics.Stability -eq 'NOT_STABLE') 'feedback gap breaks stability'
    & $reset
    $sim.FailStart=$true
    $failed = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
    Assert-SelfTest ($failed.Detail -like 'No Modbus response*100 ms*' -and $failed.StopVerified -and
        $sim.StartCount -eq 1 -and $sim.StopCount -eq 1) 'lost AUTO_START echo: length timeout, no resend, still STOP'
    Write-Output 'AUTO_PREFLIGHT_CASE=LOST_START_ECHO_NO_RETRY_STOP PASS'
    & $reset
    $sim.Approach=$true
    $coarse = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000 $true
    Assert-SelfTest ($coarse.Detail -eq '' -and $coarse.StopVerified) 'approach-only capture stops safely'
    Assert-SelfTest ($coarse.ApproachResult -eq 'CONTACT_OBSERVED_STOP_REQUESTED') 'contact exits approach-only capture'
    Assert-SelfTest ($sim.StartCount -eq 1 -and $sim.StopCount -eq 1) 'approach-only one START one STOP'
    $coarseRows = @($coarse.Samples | Where-Object { $_.CoarseApproachRequestObserved })
    Assert-SelfTest ($coarseRows.Count -gt 0 -and $coarseRows[-1].RequestNumberSinceStart -eq 2) 'coarse requests observable after output-off'
    $sim.NoContact=$true
    $zero = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000 $true
    Assert-SelfTest ($zero.Detail -ne '' -and $zero.StopVerified -and $zero.ApproachResult -eq 'CONTACT_NOT_OBSERVED') 'zero response faults; capture STOP verified'
    & $reset
    $sim.BadBaseline=1; $sim.BadReadback=1; $sim.StampAttempts=$true
    $retry = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
    Assert-SelfTest ($retry.Detail -eq '' -and $retry.StopVerified -and $sim.StartCount -eq 1 -and
        $sim.StopCount -eq 1 -and $sim.TargetWrites -eq 1) 'incoherent preflight reread: one target write, one START, one STOP'
    foreach ($phase in @('BASELINE','TARGET_READBACK')) {
        $attempts = @($retry.Samples | Where-Object { $_.Phase -eq $phase })
        Assert-SelfTest ($attempts.Count -eq 2 -and -not $attempts[0].CoherentSnapshot -and
            $attempts[1].CoherentSnapshot -and $attempts[0].ControlPressure -eq 101 -and
            $attempts[1].ControlPressure -eq 102 -and $attempts[1].RawPressure -eq 102 -and
            [uint64]$attempts[1].DeviceSampleSequence -gt [uint64]$attempts[0].DeviceSampleSequence -and
            -not $attempts[0].NewSensorSample) 'whole fresh snapshot replaces attempt; incoherent row never qualified'
    }
    $metrics = Get-AutoMetrics $retry.Samples $retry.StartedMs 250
    Assert-SelfTest ($metrics.Stability -eq 'OBSERVED_STABLE_3S' -and $metrics.ObservedPeak -eq 250) 'preflight attempts excluded from AUTO metrics'
    Write-Output 'AUTO_PREFLIGHT_CASE=BASELINE_AND_TARGET_REREAD_ONE_START PASS'
    & $reset
    $sim.BadBaseline=9; $sim.BadReadback=9; $sim.StampAttempts=$true
    $retry = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
    Assert-SelfTest ($retry.Detail -eq '' -and $retry.StopVerified -and $sim.StartCount -eq 1 -and
        $sim.TargetWrites -eq 1 -and $sim.StopCount -eq 1 -and $sim.BaselineReads -eq 10 -and $sim.TargetReads -eq 10) 'tenth full attempt can admit one START'
    foreach ($phase in @('BASELINE','TARGET_READBACK')) {
        $attempts = @($retry.Samples | Where-Object { $_.Phase -eq $phase })
        Assert-SelfTest (($attempts.PreflightAttempt -join ',') -eq '1,2,3,4,5,6,7,8,9,10') 'exactly ten numbered full snapshots'
        for ($i=0; $i -lt 10; ++$i) {
            Assert-SelfTest ($attempts[$i].CoherentSnapshot -eq ($i -eq 9) -and
                $attempts[$i].ControlPressure -eq (101+$i) -and $attempts[$i].RawPressure -eq (101+$i) -and
                -not $attempts[$i].NewSensorSample) 'attempts 1..9 incoherent, attempt 10 coherent; no mixed snapshot values'
            if ($i -gt 0) {
                Assert-SelfTest ($attempts[$i].ResponseReceivedElapsedMs - $attempts[$i-1].ResponseReceivedElapsedMs -ge 20 -and
                    [uint64]$attempts[$i].DeviceSampleSequence -gt [uint64]$attempts[$i-1].DeviceSampleSequence) 'each retry waits and reads a new complete snapshot'
            }
        }
    }
    Write-Output 'AUTO_PREFLIGHT_CASE=TENTH_ATTEMPT_ONE_TARGET_WRITE_ONE_START PASS'
    foreach ($phase in @('BASELINE','TARGET_READBACK')) {
        & $reset
        if ($phase -eq 'BASELINE') { $sim.BadBaseline=99 } else { $sim.BadReadback=99 }
        $rejected = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
        $attempts = @($rejected.Samples | Where-Object { $_.Phase -eq $phase })
        Assert-SelfTest ($rejected.Detail -like '*coherent*10*no AUTO_START*' -and $attempts.Count -eq 10 -and
            $sim.StartCount -eq 0 -and $sim.StopCount -eq 1 -and $rejected.StopVerified -and
            $sim.TargetWrites -eq $(if ($phase -eq 'BASELINE') {0} else {1})) 'bounded rereads exhausted: no START'
        Write-Output ("AUTO_PREFLIGHT_CASE=$($phase)_EXHAUSTED_ZERO_START PASS")
    }
    foreach ($unsafe in @(
        @{Name='FAULT'; Words=@{3=5}}, @{Name='DETAIL'; Words=@{4=6}},
        @{Name='INVALID_PRESSURE'; Words=@{0=65535}}, @{Name='INVALID_RAW'; Words=@{1=65535}},
        @{Name='STALE'; Words=@{10=0x70}}, @{Name='NOT_IDLE'; Words=@{2=4}},
        @{Name='ACTIVE'; Words=@{10=0x75}}, @{Name='LOCKED'; Words=@{10=0x79}},
        @{Name='OUTPUT_ENABLED'; Words=@{10=0x61}}, @{Name='BOTH_CCR'; Words=@{8=1;9=1}},
        @{Name='CAPABILITY'; Words=@{34=0}}
    )) {
        foreach ($phase in @('BASELINE','TARGET_READBACK')) {
          foreach ($unsafeAttempt in 1..10) {
            & $reset
            $sim.Override=$unsafe.Words; $sim.OverrideStage=$phase; $sim.OverrideAttempt=$unsafeAttempt
            if ($phase -eq 'BASELINE') { $sim.BadBaseline=10 } else { $sim.BadReadback=10 }
            $rejected = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
            Assert-SelfTest ($rejected.Detail -ne '' -and $sim.StartCount -eq 0 -and $rejected.StopVerified -and
                $sim.StopCount -eq 1 -and $sim.TargetWrites -eq $(if ($phase -eq 'BASELINE') {0} else {1}) -and
                @($rejected.Samples | Where-Object { $_.Phase -eq $phase }).Count -eq $unsafeAttempt) 'unsafe preflight on any attempt aborts immediately despite incoherence'
          }
        }
        Write-Output ("AUTO_PREFLIGHT_CASE=SAFETY_$($unsafe.Name)_BOTH_PHASES_ATTEMPTS_1_TO_10 PASS")
    }
    foreach ($wireError in @('CRC','TIMEOUT')) {
        foreach ($phase in @('BASELINE','TARGET_READBACK')) {
            foreach ($wireAttempt in 1..10) {
                & $reset
                $sim.WireError=$wireError; $sim.WireErrorStage=$phase; $sim.WireErrorAttempt=$wireAttempt
                if ($phase -eq 'BASELINE') { $sim.BadBaseline=10 } else { $sim.BadReadback=10 }
                $rejected = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
                $reads = if ($phase -eq 'BASELINE') {$sim.BaselineReads} else {$sim.TargetReads}
                $expectedError = if ($wireError -eq 'CRC') {'*CRC is invalid*'} else {'No Modbus response*'}
                Assert-SelfTest ($rejected.Detail -like $expectedError -and $reads -eq $wireAttempt -and
                    $sim.StartCount -eq 0 -and $sim.StopCount -eq 1 -and $rejected.StopVerified) 'transport error aborts immediately without rereading/START'
            }
        }
        Write-Output ("AUTO_PREFLIGHT_CASE=TRANSPORT_$($wireError)_BOTH_PHASES_ATTEMPTS_1_TO_10 PASS")
    }
    foreach ($badTarget in @(@{33=249}, @{10=0x71})) {
        & $reset
        $sim.OverrideStage='TARGET_READBACK'; $sim.Override=$badTarget
        $rejected = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000
        Assert-SelfTest ($rejected.Detail -like 'Target/capability/coherent readback mismatch' -and
            $sim.StartCount -eq 0 -and $rejected.StopVerified) 'target value and validity still required'
    }
    Write-Output 'AUTO_PREFLIGHT_CASE=TARGET_VALUE_VALIDITY_ZERO_START PASS'
    & $reset
    $rejected = Invoke-AutoCapture $exchange $clock $sleep 250 'SYNTHETIC_INPUT' 5000 $true
    Assert-SelfTest ($rejected.Detail -like 'Approach-only test requires*' -and $sim.StartCount -eq 0 -and
        $rejected.StopVerified) 'ApproachOnly pre-contact refusal unchanged'
    Write-Output 'AUTO_PREFLIGHT_CASE=APPROACH_ONLY_PRECONTACT_ZERO_START PASS'
    Write-Output 'COARSE_APPROACH_CAPTURE_SELF_TEST=PASS SYNTHETIC_INPUT; NO_SERIAL_PORT'
    Write-Output 'AUTO_TARGET_CAPTURE_SELF_TEST=PASS SYNTHETIC_INPUT; NO_SERIAL_PORT'
}

if ($SelfTest) { Test-AutoCapture; return }
if (-not $ConfirmStaticTest -or -not $ConfirmMechanicalLimitChecked) {
    throw 'Requires -ConfirmStaticTest -ConfirmMechanicalLimitChecked after operator checks the permitted mechanical load against the experiment abort threshold. NOT production approved.'
}
if ([string]::IsNullOrWhiteSpace($Port) -or [string]::IsNullOrWhiteSpace($OutputCsv)) { throw 'Port and OutputCsv are required' }
$firmware = Join-Path $PSScriptRoot '../output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.hex'
$actualHash = (Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash
if ($ConfirmedFirmwareSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $ConfirmedFirmwareSha256 -ine $actualHash) {
    throw 'Operator-confirmed firmware SHA256 must match the bundled AutoTarget Release HEX'
}
$csvPath = [IO.Path]::GetFullPath($OutputCsv)
$reportPath = [IO.Path]::ChangeExtension($csvPath, '.report.txt')
if ((Test-Path -LiteralPath $csvPath) -or (Test-Path -LiteralPath $reportPath)) { throw 'Capture output already exists' }
New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($csvPath)) | Out-Null
$serial = $null; $capture = $null
try {
    $serial = New-Object IO.Ports.SerialPort $Port,115200,None,8,One
    $serial.Handshake=[IO.Ports.Handshake]::None
    $serial.ReadTimeout=100; $serial.WriteTimeout=100
    $serial.DtrEnable=$false; $serial.RtsEnable=$false
    $serial.Open()
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $clock={ return [long]$watch.ElapsedMilliseconds }
    $sleep={ param($ms) Start-Sleep -Milliseconds $ms }
    $exchange={ param([byte[]]$Request,[int]$TimeoutMs)
        Invoke-SerialExchange -Serial $serial -Request $Request -TimeoutMs $TimeoutMs }
    $capture=Invoke-AutoCapture $exchange $clock $sleep $Target $actualHash ($MaximumSeconds*1000) ([bool]$ApproachOnly)
}
catch {
    $capture=[pscustomobject]@{ Samples=@(); StartedMs=0; Detail=$_.Exception.Message; StopVerified=$false; ApproachResult='NO_CAPTURE' }
}
finally {
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
$metrics=Get-AutoMetrics $capture.Samples $capture.StartedMs $Target
if ($capture.Samples.Count -gt 0) { $capture.Samples | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath }
else { '"RunResult","RunDetail"', ('"NO_DATA","' + ($capture.Detail -replace '"','""') + '"') | Set-Content -Encoding UTF8 $csvPath }
$report=@(Get-AutoReportLines $capture $Target $actualHash ([bool]$ApproachOnly))
$report | Set-Content -Encoding UTF8 -LiteralPath $reportPath
$report | Write-Output
Write-Output "CSV=$csvPath"
Write-Output "REPORT=$reportPath"
if (-not $capture.StopVerified -or $capture.Detail -ne '') { throw 'Capture incomplete; inspect report and confirm physical shutdown' }
