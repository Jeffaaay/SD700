[CmdletBinding()]
param(
    [ValidateSet('Observe','Parameters','SingleStart')][string]$Mode='Observe',
    [switch]$SelfTest,
    [switch]$LibraryOnly,
    [switch]$Characterization,
    [switch]$BuildToTarget,
    [string]$Port,
    [string]$ConfirmedFirmwareSha256,
    [string]$OutputCsv,
    [string]$ParameterFile,
    [ValidateRange(1,65535)][int]$Target=250,
    [switch]$TargetN,
    [int]$ProfileId=4,
    [ValidateRange(1,60)][int]$MaximumSeconds=5,
    [switch]$ConfirmMotorPowerDisconnected,
    [switch]$ConfirmSupervisedMotion,
    [string]$CurrentLimitSetting,
    [string]$InitialGap,
    [string]$FieldNotes
)
$ErrorActionPreference='Stop'
# Import the existing length-aware CRC/RTU transport. Preserve this script's options.
$saved=@{}; foreach ($name in @('Mode','SelfTest','LibraryOnly','Characterization','BuildToTarget','Port','ConfirmedFirmwareSha256','OutputCsv','ParameterFile','Target',
    'TargetN','ProfileId','MaximumSeconds','ConfirmMotorPowerDisconnected','ConfirmSupervisedMotion','CurrentLimitSetting','InitialGap','FieldNotes')) {
    $saved[$name]=Get-Variable -Name $name -ValueOnly
}
. "$PSScriptRoot/capture_pressure_response.ps1" -LibraryOnly
foreach ($entry in $saved.GetEnumerator()) { Set-Variable -Name $entry.Key -Value $entry.Value }
if ($BuildToTarget) { $Characterization=$true }
$schemaOption=if ($BuildToTarget) {'--build-to-target-schema'} elseif ($Characterization) {'--characterization-schema'} else {'--schema'}
$schema=(& python "$PSScriptRoot/force_servo_data.py" $schemaOption | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw 'ForceServo schema unavailable' }
. "$PSScriptRoot/force_characterization_plan.ps1"

# A transaction retains its normal100 ms timeout. Do not start it without
# that budget AND100 ms for STOP; never create an artificial2 ms serial timeout.
$forceTransactionMs=100
$forceStopReserveMs=100
function Stop-ForceCaptureBudget([string]$Reason) {
    $exception=New-Object OperationCanceledException $Reason
    $exception.Data['ForceCaptureBudgetStop']=$true
    throw $exception
}
function Read-ForceCandidates([scriptblock]$Exchange) {
    $words=@(Read-ForceWords $Exchange 3 0x400 ($schema.candidates.Count*2*$schema.profile.Count))
    $i=0; $result=@()
    foreach ($candidate in $schema.candidates) {
        $values=[ordered]@{}
        foreach ($field in $schema.profile) {
            [uint32]$bits=[uint64]$words[$i]*65536+[uint64]$words[$i+1]; $i+=2
            $value=[BitConverter]::ToSingle([BitConverter]::GetBytes($bits),0)
            if ($value -ne [single]$candidate.($field.name) -or [single]::IsNaN($value) -or [single]::IsInfinity($value)) {
                throw "Candidate catalog mismatch: $($candidate.id).$($field.name); no START"
            }
            $values[$field.name]=$value
        }
        $result+= [pscustomobject]$values
    }
    return $result
}
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
    if ($schema.build_to_target -and ($values.build_mode -ne 1 -or $values.build_config_digest -ne $schema.build_digest)) {
        throw 'Build execution contract diagnostic mismatch'
    }
    $values.feedback_gap_ms=$activeConfig.feedback_gap_ms
    $values.hold_enter_units=$activeConfig.hold_enter
    $values.feedback_age_ms=([long]$values.now_ms-[long]$values.latest_received_ms+4294967296) % 4294967296
    $values.at_output_cap=[int]($null -ne $activeConfig -and $values.current_committed -ne 0 -and
        ($values.current_committed -ge $activeConfig.press_cap -or $values.current_committed -le -$activeConfig.release_cap))
    $values.saturated=[int](($values.limits -band 1) -ne 0 -or $values.at_output_cap -ne 0)
    if ($schema.build_to_target) {
        $cap=if ($values.brake_active) {0} elseif ($values.segment_mode -eq 1) {$schema.build_profile.approach_ceiling} elseif ($values.target -eq 250) {
            if ($values.segment_normal_ms -le 8) {$schema.build_profile.precision_250_ceiling} else {$schema.build_profile.far_press_ceiling}
        } else {$schema.build_profile.build_ceiling}
        $values.at_output_cap=[int]($values.current_committed -gt 0 -and $values.current_committed -ge $cap)
        $values.saturated=$values.at_output_cap # PID diagnostic clipping is not a build command cap.
    }
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
    if ($schema.characterization) {
        foreach ($p in $schema.parameters) {
            if ($p.name -eq 'press_cap') {
                if ($Config.press_cap -lt 0 -or $Config.press_cap -gt $schema.live_continuous_ceiling -or [Math]::Floor($Config.press_cap) -ne $Config.press_cap) { throw 'Invalid runtime continuous ceiling' }
            } elseif ([single]$Config.($p.name) -ne [single]$p.default) { throw "Firmware-owned parameter changed: $($p.name)" }
        }
    }
    $c=$Config
    if ($c.integral_min -ge $c.integral_max -or $c.hold_enter -ge $c.hold_exit -or
        $c.control_min_ms -ge $c.feedback_gap_ms -or $c.feedback_gap_ms -ge $c.lease_ms -or
        $c.sample_age_ms -ge $c.lease_ms -or $c.tracking_gain*$c.feedback_gap_ms*0.001 -gt 1 -or
        ((-not $schema.characterization -or $c.session_ms -ne 0) -and
         ($c.saturation_ms -gt $c.session_ms -or $c.tracking_ms -gt $c.session_ms))) { throw 'Invalid active config relationships' }
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
        if ([Single]::IsNaN($value) -or [Single]::IsInfinity($value) -or
            (($schema.characterization -and $p.name -in @('continuous_press','peak_press')) -and
             ($value -lt 0 -or $value -gt $(if ($schema.build_to_target) {0} elseif ($p.name -eq 'peak_press') {9600} else {2400}) -or [Math]::Floor($value) -ne $value)) -or
            (-not ($schema.characterization -and $p.name -in @('continuous_press','peak_press')) -and $value -ne [single]$p.default)) {
            throw "Reviewed operating profile mismatch: $($p.name); no START"
        }
        $values[$p.name]=$value
    }
    return [pscustomobject]$values
}
function Read-BuildProfile([scriptblock]$Exchange) {
    if (-not $schema.build_to_target) { return $null }
    $fields=@($schema.build_profile.PSObject.Properties)
    $words=@(Read-ForceWords $Exchange 3 0x600 (2*$fields.Count))
    $values=[ordered]@{}; $i=0
    foreach ($p in $fields) {
        [uint32]$value=[uint64]$words[$i]*65536+$words[$i+1]; $i+=2
        if ($value -ne $p.Value) { throw "Immutable Build profile mismatch: $($p.Name); ZERO START" }
        $values[$p.Name]=$value
    }
    return [pscustomobject]$values
}
function Assert-ForceAdmission($Profile,[int]$Target,[bool]$Newtons,[int]$Id,[int]$Seconds) {
    if ($Profile.experiment_enabled -ne 1 -or $Profile.limits_source -eq 0) {
        throw 'EXPERIMENT_LIMITS_UNREVIEWED: assist rise/end/cutoff/cumulative, continuous/HOLD exposure and required OFF/cooling not approved; no START'
    }
    if ($Id -ne $Profile.id) { throw 'Reviewed operating profile ID unavailable; no START' }
    if (($Newtons -and $Profile.unit -ne 2) -or $Profile.unit -eq 1) {
        $reasons=@('MISSING_CONFIRMED_SENSOR_RANGE','MISSING_FORCE_N_CALIBRATION',
            'MISSING_WEAKEST_MECHANICAL_BOUNDARY','MISSING_CURRENT_AND_ON_OFF_TIME_ENVELOPE')
        for ($i=0;$i -lt 4;$i++) {
            if (([int]$Profile.qualifications -band (1 -shl $i)) -eq 0) { throw "$($reasons[$i]); no START" }
        }
    }
    if ($Newtons -ne ($Profile.unit -ne 0)) { throw 'Target unit differs from selected profile; no START' }
    if ($Target -le 0 -or $Target -gt $Profile.operating_max) {
        throw "TARGET_OUTSIDE_OPERATING_RANGE: maximum $($Profile.operating_max) in profile unit $($Profile.unit); no START"
    }
    if ($Profile.unit -eq 1 -and ($Target -lt $Profile.calibration_min -or $Target -gt $Profile.calibration_max)) {
        throw 'TARGET_OUTSIDE_CALIBRATION_COVERAGE; no START'
    }
    if ($schema.characterization -and $Profile.unit -eq 2) {
        if ($Seconds -ne 0 -or $Profile.capture_ms -ne 0 -or $Profile.energized_ms -ne 0) {
            throw 'Runtime characterization requires operator-ended capture and the compiled no-overall-deadline profile'
        }
    } elseif ($Seconds*1000 -gt $Profile.capture_ms -or $Seconds*1000 -gt $Profile.energized_ms) {
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
    if ($schema.characterization -and $Profile.unit -eq 2) { return } # Operator-ended run; target attainment is not guaranteed.
    $snapshotBudget=(1+[Math]::Ceiling((2*($schema.u32.Count+$schema.floats.Count))/11))*$forceTransactionMs
    $captureRequired=250+$planned+$Config.hold_dwell_ms+$snapshotBudget+$forceStopReserveMs
    if ($planned+$Config.hold_dwell_ms -gt $budget -or $planned -gt $Profile.build_ms -or $captureRequired -gt $Seconds*1000) {
        throw "TRAJECTORY_HOLD_EXCEEDS_BUDGET: reference=${planned}ms hold=$($Config.hold_dwell_ms)ms snapshot=${snapshotBudget}ms start=250ms stop=${forceStopReserveMs}ms required=${captureRequired}ms budget=${budget}ms; no START"
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
        [ValidateRange(0,60)][int]$MaximumSeconds=5,
        [ValidateRange(1,65535)][int]$Target=250,
    [switch]$TargetN,
    [int]$ProfileId=4,
        $Desired,
        [string]$CurrentLimitSetting,
        [string]$InitialGap,
        [string]$FieldNotes,
        [string]$ConfirmedFirmwareSha256,
        $CharacterizationPlan,
        [scriptblock]$ConfirmStart,
        [string]$RepositoryCommit
    )

    $csvPath=[IO.Path]::GetFullPath($OutputCsv)
    $reportPath=[IO.Path]::ChangeExtension($csvPath,'.report.txt')
    $metaPath=[IO.Path]::ChangeExtension($csvPath,'.metadata.json')
    foreach ($p in @($csvPath,$reportPath,$metaPath)) { if (Test-Path -LiteralPath $p) { throw "Output exists: $p" } }
    New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($csvPath)) | Out-Null
    if ($MaximumSeconds -eq 0 -and (-not $schema.characterization -or $Mode -ne 'SingleStart')) {
        throw 'Only supervised runtime SingleStart supports operator-ended capture'
    }
    # Stream complete snapshots with constant capture memory; keep partial evidence
    # on disk during an operator-ended run. A failed write still enters STOP finally.
    $csvWriter=New-Object IO.StreamWriter ([IO.File]::Open($csvPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read))
    $csvWriter.AutoFlush=$true
    $recordState=@{last=$null;count=0}
    $saveRow={param($row)
        $lines=@($row | ConvertTo-Csv -NoTypeInformation)
        if ($recordState.count -eq 0) { $csvWriter.WriteLine($lines[0]) }
        $csvWriter.WriteLine($lines[1]); $recordState.last=$row; $recordState.count++
    }
    $errorText=''; $startAttempts=0
    $startAccepted=$false; $stopReason='OBSERVATION_COMPLETE'
    $activeConfig=$null; $activeProfile=$null; $activeBuildProfile=$null; $activeCandidates=$null; $plannedReference=$null; $enforceDeadline=$false; $superviseActive=$false
    $budgetState=@{phase='PREFLIGHT';snapshot_open=$false;snapshot_start_transaction=0;discarded_snapshots=0;skipped_polls=0;transactions=0;
        stop_sent_ms=$null;errors=(New-Object Collections.ArrayList);last_transaction=$null}
    try {
        $exchange={param([byte[]]$Request,[int]$TimeoutMs)
            if ($superviseActive -and (& $StopRequested)) { Stop-ForceCaptureBudget 'OPERATOR_STOP' }
            if ($enforceDeadline) {
                $remaining=$deadline-$watch.ElapsedMilliseconds
                if ($remaining -lt $TimeoutMs+$forceStopReserveMs) { Stop-ForceCaptureBudget 'OBSERVATION_COMPLETE' }
            }
            $transaction=[ordered]@{phase=$budgetState.phase;started_ms=$watch.ElapsedMilliseconds;
                timeout_ms=$TimeoutMs;request_hex=([BitConverter]::ToString($Request));response_hex=$null;error=$null}
            $budgetState.last_transaction=$transaction; $budgetState.transactions++
            try {
                [byte[]]$response=@(& $TransportExchange -Request $Request -TimeoutMs $TimeoutMs)
                $transaction.response_hex=[BitConverter]::ToString($response)
                return $response
            } catch {
                $transaction.error=$_.Exception.Message
                [void]$budgetState.errors.Add([pscustomobject]$transaction)
                throw # True communication errors are never reclassified as budget expiry.
            }
        }
        $info=@(Read-ForceWords $exchange 4 0x100 8)
        if ($info[0] -ne $schema.schema -or $info[2] -ne 2*$schema.parameters.Count -or
            $info[3] -ne 2*($schema.u32.Count+$schema.floats.Count)) { throw 'ForceServo1 capability/schema required' }
        $activeConfig=Read-ForceConfig $exchange
        $activeProfile=Read-ForceProfile $exchange
        $activeCandidates=@(Read-ForceCandidates $exchange)
        $activeBuildProfile=Read-BuildProfile $exchange
        $configDigest=Get-ForceDigest $activeConfig $schema.parameters
        $profileDigest=Get-ForceDigest $activeProfile $schema.profile
        if (([uint64]$info[6]*65536+$info[7]) -ne $configDigest) { throw 'Active configuration digest mismatch' }
        $first=Read-ForceSnapshot $exchange 'PREFLIGHT'; & $saveRow $first
        if ($first.profile_digest -ne $profileDigest -or $first.profile_id -ne $activeProfile.id -or
            $first.unit -ne $activeProfile.unit -or $first.config_digest -ne $configDigest) { throw 'Profile/config diagnostic readback mismatch' }
        if ($first.state -ne 1 -or $first.start_pending -ne 0 -or $first.output_off -ne 1 -or $first.lease_active -ne 0 -or $first.fault -ne 0) { throw 'IDLE + OFF required' }
        if ($Mode -eq 'Parameters') {
            if ($schema.characterization) { throw 'Characterization parameters are firmware-owned; no engineering tuning writes' }
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
            $updated=Read-ForceSnapshot $exchange 'CONFIG_READBACK'; & $saveRow $updated
            if ($updated.config_digest -ne $configDigest -or $updated.profile_digest -ne $profileDigest) { throw 'Updated config/profile digest mismatch' }
        }
        if ($Mode -eq 'SingleStart') {
            if ($info[1] -ne 0 -or $first.locked -ne 0) { throw 'PHYSICAL_OUTPUT_LOCKED: no START sent; hardware qualification pending' }
            if ($first.measured_valid -ne 1) { throw 'Initial calibrated/control measurement unavailable; no START' }
            if ($schema.characterization) {
                if (-not $CharacterizationPlan -or -not $ConfirmStart) { throw 'Use run_force_characterization.ps1: atomic plan and operator confirmation required' }
                $verifiedPlan=Submit-CharacterizationPlan $exchange $CharacterizationPlan
                $Target=[int]$verifiedPlan.target_N; $TargetN=$true; $ProfileId=[int]$activeProfile.id
                $activeConfig=Read-ForceConfig $exchange; $activeProfile=Read-ForceProfile $exchange
                $configDigest=Get-ForceDigest $activeConfig $schema.parameters
                $profileDigest=Get-ForceDigest $activeProfile $schema.profile
                if ($activeConfig.press_cap -ne $verifiedPlan.continuous_cap -or
                    $activeProfile.continuous_press -ne $verifiedPlan.continuous_cap -or
                    $activeProfile.peak_press -ne $verifiedPlan.assist_command) { throw 'Runtime config/profile differs from complete plan' }
            }
            Assert-ForcePlan $activeProfile $activeConfig $Target $TargetN $ProfileId $MaximumSeconds $first.measured
            $distance=[Math]::Abs($Target-$first.measured)
            $plannedReference=[Math]::Max(1.5*$distance/$activeConfig.reference_rate,[Math]::Sqrt(6*$distance/$activeConfig.reference_acceleration))
            Write-Output "PLANNED_REFERENCE_SECONDS=$plannedReference; HOLD_DWELL_MS=$($activeConfig.hold_dwell_ms); UNIT=$($activeProfile.unit); PROFILE=$ProfileId"
            if (-not $schema.characterization) {
            Write-ForceWord $exchange 0x104 $ProfileId
            $selected=@(Read-ForceWords $exchange 3 0x104 1)
            if ($selected[0] -ne $ProfileId) { throw 'Profile selection readback mismatch; no START' }
            Write-ForceWord $exchange $(if ($TargetN) {0x103} else {0}) $Target
            }
            $ready=Read-ForceSnapshot $exchange 'TARGET_READBACK'
            & $saveRow $ready
            if ($ready.profile_digest -ne $profileDigest -or $ready.config_digest -ne $configDigest -or $ready.unit -ne $activeProfile.unit -or $ready.target -ne $Target -or $ready.state -ne 1 -or $ready.fault -ne 0 -or $ready.output_off -ne 1) { throw 'Target/state readback mismatch' }
            if ($schema.characterization) {
                if ($ready.plan_version -ne $verifiedPlan.version -or $ready.plan_digest -ne $verifiedPlan.digest -or $ready.cooling_active -or ($schema.build_to_target -and ($ready.exposure_inhibited -or $ready.post_pulse_pending -or $ready.build_no_response_ms -ge $schema.build_profile.no_response_ms))) { throw 'Plan identity or inter-run lockout prevents START' }
                if (-not (& $ConfirmStart $verifiedPlan)) { throw 'Operator cancelled; ZERO START' }
                # Echo exactly the version/digest read back; firmware additionally requires full plan read coverage.
                Write-ForceWord $exchange 0x520 ($verifiedPlan.version -shr 16)
                Write-ForceWord $exchange 0x521 ($verifiedPlan.version -band 65535)
                Write-ForceWord $exchange 0x522 ($verifiedPlan.digest -shr 16)
                Write-ForceWord $exchange 0x523 ($verifiedPlan.digest -band 65535)
                Write-ForceWord $exchange 0x524 0xA501
            }
            $deadline=$watch.ElapsedMilliseconds+$MaximumSeconds*1000; $enforceDeadline=$MaximumSeconds -gt 0; $superviseActive=$true
            $startAttempts=1 # Set before transmitting. Lost echo never retries START.
            $startAccepted=$null # Unknown if the echo is lost; never infer rejection.
            Send-SingleCoil $exchange 0x10 0xFF00 100
            $startAccepted=$true
        }
        if ($Mode -ne 'SingleStart') { $deadline=$watch.ElapsedMilliseconds+$MaximumSeconds*1000; $enforceDeadline=$MaximumSeconds -gt 0; $superviseActive=$true }
        while (-not $enforceDeadline -or $watch.ElapsedMilliseconds -lt $deadline) {
            if (& $StopRequested) { $stopReason='OPERATOR_STOP'; break }
            $budgetState.phase=if ($Mode -eq 'SingleStart') {'RUN'} else {'OBSERVE'}
            $budgetState.snapshot_open=$true; $budgetState.snapshot_start_transaction=$budgetState.transactions
            $row=Read-ForceSnapshot $exchange $budgetState.phase
            $budgetState.snapshot_open=$false
            & $saveRow $row
            if ($row.fault -ne 0) { $stopReason="DEVICE_FAULT_$($row.fault)_DETAIL_$($row.detail)"; break }
            if ($Mode -eq 'SingleStart' -and $row.state -eq 1 -and $row.start_pending -eq 0) {
                $stopReason='DEVICE_IDLE_AFTER_START'; break
            }
            & $SleepMilliseconds 10
        }
    } catch {
        if ($budgetState.snapshot_open) {
            if ($budgetState.transactions -gt $budgetState.snapshot_start_transaction) { $budgetState.discarded_snapshots++ }
            else { $budgetState.skipped_polls++ }
        }
        if ($_.Exception.Data['ForceCaptureBudgetStop'] -eq $true) {
            $stopReason=$_.Exception.Message
        } else {
            $errorText=$_.Exception.Message; $stopReason='CAPTURE_ERROR'
            if ($startAttempts -eq 1 -and $null -eq $startAccepted -and $errorText -like 'Modbus exception:*') { $startAccepted=$false }
        }
    }
    finally {
        $enforceDeadline=$false; $superviseActive=$false; $budgetState.phase='STOP_READBACK'
        $budgetState.stop_sent_ms=$watch.ElapsedMilliseconds
        try {
                Send-SingleCoil $exchange 1 0 100
                & $saveRow (Read-ForceSnapshot $exchange 'STOP_READBACK')
        } catch { $errorText+='; STOP/readback: '+$_.Exception.Message }
        finally {
            if ($recordState.count -eq 0) { $csvWriter.WriteLine('"phase","firmware_sha256"') }
            $csvWriter.Dispose()
        }
    }
    @{mode=$Mode;start_attempts=$startAttempts;start_accepted=$startAccepted;stop_reason=$stopReason;error=$errorText;config=$activeConfig;
        current_limit_setting=$CurrentLimitSetting;initial_gap=$InitialGap;field_notes=$FieldNotes;
        firmware_sha256=$actualHash;operator_flash_attestation=$ConfirmedFirmwareSha256;
        build_profile=$activeBuildProfile;build_config_digest=$schema.build_digest;
        commissioning=$(if ($schema.build_to_target) {'BuildToTarget3_SourcePort1'} elseif ($schema.characterization) {'StaticForceRuntimeCharacterization2'} else {'StaticForceAuthority2_ReviewFix'});runtime_plan=$verifiedPlan;repository_commit=$RepositoryCommit;sensor_unit_source=$(if ($schema.characterization) {'USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT'} else {'LEGACY_COUNTS'});profile=$activeProfile;candidate_catalog=$activeCandidates;profile_digest=$profileDigest;config_digest=$configDigest;target=$Target;target_N=[bool]$TargetN;planned_reference_seconds=$plannedReference;qualification='SHORT_SUPERVISED_EXPERIMENT_NOT_CONTINUOUS_RATING';powered_test_ready=$schema.powered_test_ready;physical_test_status='OPERATOR_CAPTURE_UNVALIDATED';
        pwm_counts='tim2/tim3 are PLANNED; no external electrical measurement';
        maximum_observation_seconds=$(if ($MaximumSeconds -gt 0) {$MaximumSeconds} else {$null});capture_end_policy=$(if ($MaximumSeconds -eq 0) {'OPERATOR_STOP_OR_DEVICE_TERMINAL_OR_FAULT'} else {'FINITE_OBSERVATION'});capture_wall_ms=$watch.ElapsedMilliseconds;
        stop_reserve_ms=$forceStopReserveMs;transaction_timeout_ms=$forceTransactionMs;
        stop_sent_pc_ms=$budgetState.stop_sent_ms;discarded_snapshots=$budgetState.discarded_snapshots;skipped_polls=$budgetState.skipped_polls;
        communication_errors=@($budgetState.errors);last_transaction=$budgetState.last_transaction;
        bus='115200 8N1; frozen snapshot in <=11-register chunks; polling misses are reported'} |
        ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $metaPath
    & python "$PSScriptRoot/force_servo_data.py" --report $csvPath --metadata $metaPath
    if ($LASTEXITCODE -ne 0) { throw 'Report generation failed; raw CSV and metadata preserved' }
    Write-Output "CSV=$csvPath`nREPORT=$reportPath"
    if ($errorText) { throw $errorText }
    $last=$recordState.last
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
if ($Characterization -and $Mode -ne 'Observe') { throw 'Use run_force_characterization.ps1; field PI and generic START are unavailable' }
if ($Mode -eq 'SingleStart') {
    $compiledProfile=[pscustomobject]@{}
    foreach ($p in $schema.profile) { $compiledProfile | Add-Member -NotePropertyName $p.name -NotePropertyValue $p.default }
    $selectedCandidate=@($schema.candidates | Where-Object id -eq $ProfileId)
    if ($selectedCandidate.Count -eq 1) { $compiledProfile=$selectedCandidate[0] }
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
$firmware=Join-Path $PSScriptRoot '../output/BuildToTarget3_SourcePort1/firmware/SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release.hex'
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
