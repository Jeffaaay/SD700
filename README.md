# SD700 - ForceServo1 CommissioningUnlock1

Current candidate: **CommissioningUnlock1**, a bounded supervised commissioning
build from clean `main` / `origin/main`
`5fb80d7cfc85176d9a82688ab37d22194a2577e4`. No later changes were discarded.
**GitHub main is the source of truth; delivery is the pushed commit. No ZIP.**

**physical test NOT RUN. COMMISSIONING_NOT_TUNED.**
HARDWARE_STOP_VALIDATION, STATIC_250 and ROTATING_LOAD remain NOT_VALIDATED.

[Current RealBench Release HEX](output/CommissioningUnlock1/firmware/SD700_ForceServo1_CommissioningUnlock1_RealBench_Release.hex)

HEX SHA256: `F96936FF7962ADC65737C0C6C97F5EBC49F30E8A3B33A20B75C450E3657DD46C`

[Matching ELF](output/CommissioningUnlock1/firmware/SD700_ForceServo1_CommissioningUnlock1_RealBench_Release.elf)

ELF SHA256: `0925C1CDAEEDB7DCBFCD08EC0BF5E37505ABAA2175B2B11A17112D747CB86D41`

## Scope and limits

The existing trajectory -> PID -> executor -> monitored continuous output ->
active HOLD path is retained. There is no controller redesign, extended timeout
or lengthened pulse. Only the explicit `-ForceServo -CommissioningUnlock1` build,
with the existing RealBench acknowledgement and master arming guard, enables
physical output. A plain `-ForceServo` build remains locked. No RAM unlock exists.

The continuous **command ceiling is 100 mV in either direction**. Its source is
the lower original ForceServo1 default, `release_cap=100`, present at `29d0a90`
and the starting main; the original press default was 250. No physically
qualified continuous rating exists in the repository. This conservative seed is
not measured coil voltage/current or proof of safe continuous thermal duty.
The existing 24000 mV PWM reference and 4799 timer period are unchanged; a 100 mV
command maps to compare 20. **Keep the existing supply current limit unchanged.**

Default Kp=1, Ki=0, Kd=0, reference rate20, acceleration40 and output slew1000
are unchanged. Both cap defaults and RAM upper bounds are now100. Existing
timing defaults are retained; RAM cannot relax these freshness/lease limits:

| Constraint | Commissioning value |
| --- | --- |
| Minimum control interval | 5 ms default |
| Maximum accepted sample age | 20 ms |
| Missing-feedback fault budget | 40 ms |
| Receive-anchored TIM5 lease | 50 ms |
| Direction-change OFF time | at least 2 ms |
| Contact-to-first-HOLD deadline | 30000 ms, unchanged |
| Total session deadline | 45000 ms default, unchanged |

The existing TIM5 comparison reserves 1 ms: a fresh 50 ms lease expires at49 ms
in the host timer model. Main-loop pressure safety faults after a gap exceeding
40 ms. Old/missing feedback cannot renew output; telemetry cannot renew leases.
Physical interrupt latency and electrical shutdown timing still need measurement.
STOP priority, hardware output matching guard, overpressure, contact loss,
fault latching and no automatic restart remain in force.

Commissioning START requires fresh, already-established contact (at least20
sensor units). This profile rejects all legacy pulse/run commands, including
the former 10000 mV initial approach. The capture script additionally requires
initial raw pressure20..30 and target at most60 for this supervised session.
Firmware target max275 and raw abort325 are unchanged. Units are **sensor control
units, not calibrated N**. No Target250, 500 or 3500 N trial is authorized here.
If low output cannot move/build pressure, retain the cap and report that result.

## One supervised field session, A-E

Use COM5, retain the existing current limit and record its actual value, initial
gap/contact setup, supply behavior and temperature observations. The supervisor
must permit the load, travel and cumulative thermal exposure and have an external
E-stop available. Do not run an automatic loop or automatically retry START.
This is one session; no separate Observe-only day or timing-tuning round is needed.

**A. Verify idle/OFF.** With motor power physically disconnected and MCU/sensor
correctly powered, update and verify the repository, then flash the identified
HEX using the established local flashing procedure. Establish light contact
safely with power disconnected (fresh raw20..30; no powered approach). Confirm
IDLE, no fault, no lease, both PWM compares zero, and physical driver/output OFF
before admitting motor power. The short Observe command below checks software
readback in this same session; inspect actual output electrically as well.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
# After flashing: this is an operator attestation, not a MCU flash readback.
$hexHash = 'F96936FF7962ADC65737C0C6C97F5EBC49F30E8A3B33A20B75C450E3657DD46C'
$currentLimit = Read-Host 'Actual existing supply current limit (do not raise)'
$initialGap = Read-Host 'Initial gap / light-contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode Observe -Port COM5 -MaximumSeconds 1 `
  -ConfirmMotorPowerDisconnected -ConfirmedFirmwareSha256 $hexHash `
  -CurrentLimitSetting $currentLimit -InitialGap $initialGap `
  -FieldNotes 'A: motor power disconnected; idle/output-off check' `
  -OutputCsv "captures/commissioning-A-$stamp.csv"
```

**B. Low-output check; C. STOP check.** After A passes, enable the retained limited
supply under supervision. Run this **once** with target40 and a3-second PC
observation limit. The script sends one START, observes the existing control
path and sends STOP on completion/failure. Confirm nonzero output and correct
direction at low command, then measure physical OFF on the script's STOP and
verify `StopVerified=True`, IDLE/OFF and no subsequent restart. Record STOP-to-OFF
timing; CSV/software readback alone does not qualify hardware stopping. Use the
external E-stop immediately for wrong direction, limit/droop, abnormal heating,
motion or failure to stop. Absent/unmeasurable active output cannot pass B/C.

```powershell
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -Target 40 -MaximumSeconds 3 `
  -ConfirmSupervisedMotion -ConfirmedFirmwareSha256 $hexHash `
  -CurrentLimitSetting $currentLimit -InitialGap $initialGap `
  -FieldNotes 'B/C: low-output check and STOP; record electrical timing' `
  -OutputCsv "captures/commissioning-BC-$stamp.csv"
```

**D. Feedback-loss automatic stop.** Only after B/C pass, use the same command
once with a new timestamp/filename `commissioning-D-$stamp.csv`, target40 and
`-MaximumSeconds 5`. While nonzero output is confirmed, interrupt only the
pressure-feedback receive path using a prepared electrically safe test break;
keep MCU/sensor power correct. Measure shutdown from the last valid received
sample, before the later PC STOP. Check fault/OFF without fresh feedback within
the nominal50 ms lease budget, and no automatic restart after restoring feedback.
Distinguish this autonomous stop from the script's final STOP; otherwise D is
inconclusive. Record the independent timing measurement. Do not bypass freshness
checks or increase timing budgets if D fails. Stop this session if any A-D result
fails or is inconclusive.

**E. One low-target ForceServo step.** Only if A-D pass, disconnect motor power,
restore feedback, explicitly reset the fault/MCU using the established procedure,
and recheck IDLE/OFF and initial raw20..30. After supervision permits power, run
the same SingleStart command **once** with target60, `-MaximumSeconds 5`, a new
timestamp/filename `commissioning-E-$stamp.csv`, and updated field notes. Observe
the trajectory, control direction, limits and HOLD if reached; the script ends
with STOP. No extra manual START, parameter tuning, current increase or automatic
repeat. Return all CSV/report/metadata files and one brief field note, including
actual current limit, initial gap, A-D measurements, any supply/temperature or
mechanical abnormalities and E outcome. Missing instrumentation is NOT_MEASURED.

## Software verification and reproduction

[Actual tests and scope](Docs/CommissioningUnlock1/TEST_RESULTS.md),
[current parameter defaults](Docs/CommissioningUnlock1/default_parameters.json),
[current schema and bounds](Docs/CommissioningUnlock1/protocol_schema.json), and
[current firmware manifest](Firmware/ForceServo1.SHA256SUMS.txt).

```powershell
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 `
  -ForceServo -CommissioningUnlock1 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION `
  -BuildDir "output/CommissioningUnlock1/build-$stamp"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 `
  -OutputDirectory "output/CommissioningUnlock1/general-$stamp"
python tools/run_force_servo_tests.py --output "output/CommissioningUnlock1/default-$stamp"
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check `
  --output "output/CommissioningUnlock1/check-$stamp"
```

Field verification uses strict Python standard-library ELF/HEX address/load,
checksum, type/bounds, identity, configuration, compiled commissioning gate and
real hash checks. It needs no ARM executable. Capture needs Python3.9+ and
PowerShell. Development builds need ARM GCC; host tests need host GCC. Only the
optional developer `--objcopy-cross-check` needs ARM objcopy. Tests use synthetic
transport/HAL and do not open a serial port or measure real hardware.

After validated changes, update tracked firmware/manifests and documentation,
commit and push `origin/main` normally. Do not generate archives, sidecars,
extracted delivery copies or a package-verification prerequisite. Preserve field
evidence and failure logs. Remove regenerable objects/caches after recording tests.
The former locked ForceServo1 and ConvergenceMeasure1 firmware/evidence remain
historical; their field instructions do not apply to this candidate.
