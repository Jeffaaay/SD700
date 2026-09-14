# SD700 - StaticForce3000_Boost1

**physical test NOT RUN.** Current candidate from StaticForce3000_1,
`ed3c213b689929524dba2af016d85724097c7850`. Local HEAD and origin/main matched
and the worktree was clean before changes. GitHub main is the delivery; no ZIP.

The user reports that720 continuous command reached TIM3=144 but produced no
motion or pressure rise (27 ->27). PSU display stayed about0.028 A; a scope
captured one approximately600 ns /22 V pulse. STOP removed PWM, output_off=1,
and no abnormal noise/heat/jamming was reported. See the exact
[input evidence and its limits](Docs/StaticForce3000_Boost1/FIELD_EVIDENCE.md).

This candidate enables one **short supervised breakaway experiment** using the
existing continuous ForceServo boost path. It is **not a continuous25% rating
or3000 N qualification**. No serial connection, flashing or motion was performed.

[Current HEX](output/StaticForce3000_Boost1/firmware/SD700_ForceServo1_StaticForce3000_Boost1_RealBench_Release.hex)
and [ELF](output/StaticForce3000_Boost1/firmware/SD700_ForceServo1_StaticForce3000_Boost1_RealBench_Release.elf).

HEX SHA256: `4EA0CDB953C06B4DB32F77746B16677B4421C752D828AC9F423F24BBC4DFF7E0`

ELF SHA256: `A301D51A61F312BE680D259832B83C34DAB2EF6A0D4D1527C310BF1E83A58D82`

## Current experiment

| Setting | Value |
| --- | --- |
| Profile / protocol / build |3 /F105 /46530107 |
| Field target |250 legacy control units, **not N** |
| Existing target range / raw abort |1..275 /325, unchanged |
| Kp / Ki / Kd |10 /0 /0 |
| Continuous PRESS / RELEASE |720 /100 command units |
| Peak PRESS |6000 command units; nominal25%, CCR1200/4800 |
| Boost duration / total reservation |10 /10 ms; at most one admission per START |
| Normal handoff attempt |8 ms, reduction to the already computed normal command <=720 |
| Independent cutoff if no handoff |Existing TIM5 reserve: approximately9 ms after boost admission |
| PWM frequency |20 kHz, unchanged |
| Ordinary increasing-output slew |1000 command units/s, unchanged |
| Reference rate / acceleration |200 /1000, unchanged |
| Pressure age / active gap / receive-anchored lease |20 /125 /130 ms, unchanged |
| Pending START / control minimum / direction OFF |250 /5 /at least2 ms, unchanged |
| Build / energized / session / PC capture maximum |5000 ms, unchanged |
| No-response / saturation policy |Existing5 s bounded policy, unchanged |
| Field PSU setting |**0.5 A unchanged**; display current is not winding current |

The PID, cubic trajectory, active HOLD, START admission, STOP priority,
pressure/contact/overpressure checks, lease expiry, break-before-make, output
guard and fault latch remain. Plain `-ForceServo` stays compiled LOCKED with no
runtime unlock. Newton qualification bits remain0: this is a count-domain
experiment, not a new sensor calibration or mechanical/thermal qualification.

## Bounded boost and handoff

Admission requires the exact compiled profile, Target250, the above P-only
settings, valid contact, fresh ordered pressure, healthy continuous owner,
positive ordinary demand, both target and reference margin above72 control
units (720/Kp10), and remaining fixed/cumulative budgets. Lower targets or other
valid tuning retain ordinary control; they do not admit this experimental peak.
No runtime parameter can select a different peak profile or increase its budget.

Simply raising the old cap would not reach6000 within10 ms at the existing slew.
During this one admitted stage the request is explicitly6000; diagnostics set
`FS_LIMIT_BOOST=16`. The ordinary PID step still runs and computes a <=720
handoff request. The controller records the executor's actual command with Ki0.
The ordinary PID algorithm and its slew are unchanged outside this bounded
peak stage. The continuous profile and executor reject persistent PRESS>720;
6000 cannot be enabled without an armed, fixed boost deadline.

The unslept main loop attempts reduction at deadline-2 ms. It installs the lower
hardware compare and verifies the existing guard before releasing the peak
compare. It restores only the previously accepted pressure frame's original
receive lease, bounded by the original absolute session deadline. There is no
new pressure sequence, PID integration or lease extension from the handoff time.
Fresh feedback can taper/exit sooner. Pending expiry wins even during transfer.
Late servicing, STOP, a fault, feedback loss or lease expiry forces OFF; there
is no automatic revival after the independent cutoff. **10 ms is a configured
maximum budget, not a claim of exactly10 ms measured peak PWM.** Nominal timely
handoff is8 ms; field scheduling and the waveform remain unmeasured.

The full10 ms is reserved at admission and never refunded by early exit, STOP,
fault reset, config writes or later explicit STARTs in the same MCU boot. This
preserves the existing stronger cumulative-budget rule: **only the first admitted
boost in that boot can fire**. Reboot is not permission to repeat the experiment.

## Telemetry and capture

The frozen schema has69 u32 +25 float fields (188 words); config remains50 FC03
words and profile52 FC03 words. Existing framing, CRC, station/function, exact
length and timeout checks remain. Scripts verify the current firmware offline
before serial access. Python/PowerShell field use does not require ARM tools.

`boost_active`, `boost_peak_command`, configured `boost_duration_ms`, reserved
`boost_spent_ms`, `boost_started_ms`, `boost_deadline_ms`, `boost_elapsed_ms`,
`boost_end_ms`, `boost_end_reason`, and `boost_handoff_command` distinguish the
peak and its handoff. End reason0=none,1=active,2=verified lower handoff,3=STOP/fault.
The event survives STOP/fault and slow PC polling; a new session starts a new
event record while retaining spent budget. Report `boost_event` may therefore
show a6000 peak even when no PC RUN snapshot happened during those milliseconds.

`boost_pressure_before` has its accepted receive timestamp. `boost_pressure_after`
is unavailable until `boost_after_valid=1`: it is the first fresh, ordered,
in-range pressure frame after the recorded end, with its own timestamp.
At roughly101 ms feedback cadence, this does **not** measure pressure at10 ms.
Abort elapsed is a capped main-service-time bound, not measured PWM on-time.
CCR fields and verified output OFF concern MCU registers, not physical cessation.

## One supervised field capture

Use the existing supervised setup, confirmed travel/fixture and accessible
physical stop. Verify correct firmware, IDLE/output OFF, live pressure, initial
gap/contact and unchanged0.5 A limit. Do not increase current, test3000 N/rotating
load, or automatically repeat. One script invocation sends the only START;
do not add a manual START. Stop immediately for abnormal movement/noise/current,
visible limiting/dropout, heat or mechanical problems. Return CSV/report/metadata
and a brief account of movement, PSU behavior and physical STOP.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -ProfileId 3 -Target 250 -MaximumSeconds 5 `
  -ConfirmSupervisedMotion -ConfirmedFirmwareSha256 4EA0CDB953C06B4DB32F77746B16677B4421C752D828AC9F423F24BBC4DFF7E0 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'One supervised breakaway experiment; record movement, current display, waveform and physical STOP' `
  -OutputCsv "captures/static-force3000-boost1-$stamp.csv"
```

## Software verification

[Actual test record](Docs/StaticForce3000_Boost1/TEST_RESULTS.md),
[machine-readable execution record](Docs/StaticForce3000_Boost1/verification.json),
[default parameters](Docs/StaticForce3000_Boost1/default_parameters.json),
[protocol schema](Docs/StaticForce3000_Boost1/protocol_schema.json).
Firmware hashes are pinned in `Firmware/ForceServo1.SHA256SUMS.txt`;
`Firmware/StaticForce3000_1.SHA256SUMS.txt` preserves the predecessor pair.

```powershell
python tools/run_force_servo_tests.py --commissioning --output output/StaticForce3000_Boost1/recheck-host
.\tools\build_gcc.ps1 -ForceServo -StaticForce3000Boost1 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForce3000_Boost1/rebuild
python tools/verify_force_servo_firmware.py --objcopy-cross-check
```

The last command verifies the tracked current pair; development objcopy is an
optional extra cross-check. Test/build PASS does not establish hardware timing,
physical STOP, static Target250 performance, or rotating-load performance.
**HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated.**
