# SD700 - Target250MVP1

Current milestone: **Target250MVP1**, from clean main/origin-main
`3e4208df808fa7c78731ed589e0cf13a63da81a9`. GitHub main is the source of truth.
Delivery is the pushed commit. **No ZIP.**

**Target = 250 sensor control units, not Newton. FIELD_STATUS=NOT_RUN.**
This candidate has not been connected, flashed or physically tested by this work.

[Current RealBench Release HEX](output/Target250MVP1/firmware/SD700_ForceServo1_Target250MVP1_RealBench_Release.hex)

HEX SHA256: `4277556949A4AD6A2D5D24E49F8A1DEA97C7D8906CA48AA9878D68B5DF924055`

[Matching ELF](output/Target250MVP1/firmware/SD700_ForceServo1_Target250MVP1_RealBench_Release.elf)

ELF SHA256: `06B8281B8D089D97D5639C6176A94F962AB5297FE04EEAE9CC32AF5B5DB31B01`

## START and Target250 control

The previous field failure occurred before motion. The old START code rejected
pressure older than20 ms, and the Modbus mapping returned generic0x04 for
NOT_READY and other failures. The reported preflight age24 ms makes freshness
the leading explanation, not proof of the exact START-time condition or of a
motor hardware fault. See [reported evidence](Docs/Target250MVP1/FIELD_EVIDENCE.md).

START now latches one request in IDLE, with physical output OFF. The next new,
valid pressure frame, received since START and delivered within20 ms, initializes
the existing ForceServo continuous session from that measurement. It does not
use the old10000 mV approach. No new machine state or controller framework was
added. A duplicate START cannot extend the wait; STOP cancels it immediately.
The no-output wait expires after250 ms and faults safely. Invalid feedback,
fault, timeout or STOP cannot cause a later automatic restart.

The former initial-pressure20..30 / target<=60 PC gate is removed. The capture
command uses Target250 only. Firmware boots with target250 selected and retains
the existing maximum275/raw-abort325 protections. Initial nonnegative pressure
below contact20 can start continuous control. Once actual contact has been
observed, the existing contact-loss shutdown remains. Pressure freshness, STOP
priority, hardware output matching guard, receive-anchored TIM5 lease, fault
latching and at least2 ms direction-change OFF interlock remain active.

The existing measurement -> reference -> PI/PID -> continuous executor -> active
HOLD path is used. The first field run is P-first:

| Parameter | Current value |
| --- | --- |
| Kp / Ki / D | 1 / 0 / 0 |
| PRESS / RELEASE cap | 100 /100 mV command magnitude; RAM-adjustable1..100 |
| Reference rate / acceleration | 200 units/s /1000 units/s^2 |
| Output slew | 1000 mV/s, unchanged |
| Minimum control interval | 5 ms |
| Maximum newly delivered sample age | 20 ms, unchanged |
| Active feedback gap / TIM5 lease | 125 /130 ms |
| Pending START wait | 250 ms, output OFF |
| Build / total session deadline | 30000 /45000 ms defaults, unchanged |
| Continuous saturation / large-error timeout | 5000 /10000 ms defaults, unchanged |

Reference rate/acceleration use the existing allowed upper bounds. From pressure23,
the existing cubic reference reaches250 in about1.70 seconds instead of17.0;
this is reference timing, **not predicted physical rise time**. P decreases as
pressure approaches250; limited RELEASE is available above target. HOLD continues
the same controller without resetting its integral. Ki remains configurable for
the next measured tuning step. A local anti-windup correction prevents pure
integer-mV truncation from cancelling small unsaturated I increments; actual
saturation, output slew and direction-interlock tracking remain in force.

## Authority and feedback timing

**Do not increase the physical0.5 A supply current limit.** No evidence in the
repository establishes a higher qualified continuous command ceiling. Older
5000 mV /10 ms PRESS and10000 mV approach commands are pulse evidence; the driver
mapping and a0.5 A limit do not establish continuous motor/driver thermal ratings.
The existing100 mV cap is retained as a conservative numerical ceiling, not a
measured safe continuous rating. It maps to compare20 using the unchanged24000 mV
reference /4799 timer period. It is not measured motor-terminal voltage.

If100 mV cannot move or reach250, this run must show that outcome and saturation;
no automatic current or command-authority increase is provided. A saturated
failure suggests reviewing authority/hardware; an unsaturated failure suggests
reviewing controller tuning. Neither is a complete physical root-cause diagnosis.

Observed latest-sample ages reached74 ms and receive/delivery extrema about101
ms. The extrema are cumulative, but the repeated non-startup ages cannot be
proven to be startup-only artifacts. The old40/50 ms active budgets cannot support
those observed gaps. Keep new-sample age20 ms;101+20=121 ms rounds up to125 ms on
the existing5 ms control grid. Lease130 adds one5 ms slot and remains independently
receive-anchored. TIM5 retains its1 ms early comparison (129 ms in host tests).
Only the profile's accepted lease upper bound changes; IRQ/renewal/STOP mechanics
are unchanged. Larger gaps or stale delivered samples still stop output. These
budgets accommodate the reported samples; they are not a worst-case sensor or
physical shutdown qualification. Default locked ForceServo retains40/50 ms.

## One supervised field session

Keep the existing0.5 A supply limit, record actual initial gap/contact and permit
only the supervised load/travel/thermal exposure with an external E-stop ready.
Use a normal interactive PowerShell console on COM5. Do not use ISE or redirect
keyboard input for the operator-key STOP. No separate diagnosis-only round.

A. With motor power disconnected, update/verify the repository and flash this
HEX using the established method. With MCU/sensor correctly powered, confirm
IDLE, fault0, no pending START, output OFF electrically and no spontaneous motion
before admitting the retained limited supply.

B-D. Prepare the existing mechanical setup, run the command below **once**, and
observe continuous motion, pressure, saturation, peak/overshoot and final response.
The script itself sends the only START. It saves PREFLIGHT and TARGET_READBACK,
then RUN samples. Do not additionally press manual START.

E/F. If stable near250, allow a brief active HOLD before the final STOP. Then
press **S** (or Escape) in the capture console while output/control is active.
This sends the existing STOP, collects STOP_READBACK and preserves the report.
Confirm physical output OFF/no motion and `StopVerified=True`; software readback
alone does not certify electrical STOP. If it stalls saturated, record that and
STOP before the unchanged5-second saturation fault. Use the external E-stop
immediately for abnormal motion, supply limiting/droop, heating or failed STOP.
The script also STOPs on timeout/error; never restart automatically. HOLD belongs
before that final STOP, so this remains a single START session.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -Target 250 -MaximumSeconds 40 `
  -ConfirmSupervisedMotion `
  -ConfirmedFirmwareSha256 4277556949A4AD6A2D5D24E49F8A1DEA97C7D8906CA48AA9878D68B5DF924055 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'One supervised Target250 session; S/Escape for final STOP; record motion and supply behavior' `
  -OutputCsv "captures/target250-mvp1-$stamp.csv"
```

Do not test500 or3500 N, repeat START, raise current or automatically tune gains.
Return CSV, report, metadata and a short field note: motion, initial gap, retained
current limit, supply/temperature behavior, whether250/HOLD occurred and actual
STOP result. Report unmeasured current/temperature as NOT_MEASURED. The hash is
an operator flash attestation; the capture does not hash MCU flash.

## What the data says

Frozen samples contain raw/control pressure, target/reference/error, P/I/D terms,
raw controller output, requested integer command, committed/clamped command,
amplitude saturation, direction, feedback age, lease, state/fault/detail and
START pending/time. Applied command means the executor's committed command,
not measured terminal voltage/current. Metadata records START echo acceptance
(true/false/unknown on lost echo), stop reason and exact parameter readback.

The report gives time to control/nonzero command (not measured motion), initial
pressure with its pre-start source,90%/first250 time, peak/overshoot, final pressure
and error, saturation percentage and observed time, HOLD presence/active status,
contiguous HOLD/settling estimate and software STOP verification. When250 is not
reached, maximum pressure and saturation are explicit. Gaps and duplicate polls
are disclosed; no interpolation across gaps or full-bandwidth performance claim.

## Reproduce and deliver

[Actual test record](Docs/Target250MVP1/TEST_RESULTS.md),
[defaults](Docs/Target250MVP1/default_parameters.json),
[schema/bounds](Docs/Target250MVP1/protocol_schema.json),
[current firmware manifest](Firmware/ForceServo1.SHA256SUMS.txt).

```powershell
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 `
  -ForceServo -Target250MVP1 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir "output/Target250MVP1/build-$stamp"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 `
  -OutputDirectory "output/Target250MVP1/general-$stamp"
python tools/run_force_servo_tests.py --output "output/Target250MVP1/default-$stamp"
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check `
  --output "output/Target250MVP1/check-$stamp"
```

The internal `SD700_FORCE_SERVO_COMMISSIONING` flag/host switch reuses the existing
explicit output gate; `-Target250MVP1` is the current builder option. Plain
`-ForceServo` remains physically locked with previous control/timing defaults.
Field verification needs only Python's standard library and strictly checks true
hashes, ELF/HEX addressed load bytes, checksums, bounds, identity, configuration
and arming. Optional objcopy is developer-only. Capture requires Python and
PowerShell; host tests use a GCC-compatible compiler; ARM builds use ARM GCC.

Preserve original evidence and historical firmware. After checks pass, update the
source/firmware manifests, commit and normally push origin/main. No archive or
package-verification prerequisite. **Current physical output/STOP/HOLD250 and
rotating-load performance are NOT_VALIDATED; FIELD_STATUS=NOT_RUN.**
