# SD700 - StaticForce3000_1

**physical test NOT RUN.** Static workpiece software milestone from reviewed
main `6bfb5f1e669234b0e56027b76c5853f94cea3e2a` (Target250Authority1).
GitHub main is the delivery; no ZIP. No serial connection, flashing or motion
was performed. No Authority1 physical result was supplied.

The existing trajectory -> PID -> monitored continuous executor -> active HOLD
path remains. This candidate adds an explicit operating profile, consistent
units, finite progress-aware build policy and a bounded optional PRESS boost.
**High-force and high-output hardware activation are disabled.** The current
armed build retains only the user's short legacy count-domain experiment.
Plain `-ForceServo` remains compiled LOCKED; there is no runtime unlock.

[Current HEX](output/StaticForce3000_1/firmware/SD700_ForceServo1_StaticForce3000_1_RealBench_Release.hex)
and [ELF](output/StaticForce3000_1/firmware/SD700_ForceServo1_StaticForce3000_1_RealBench_Release.elf).

HEX SHA256: `39967AF2116B38A5FDB65C2D44AE83C04D5BA7EA3559932B09C64D2FCBE56AD9`

ELF SHA256: `95B4C99A75929D10E26D2DBE4DA8A708089B1285D6C1B50032E99BB326460323`

## Units and operating contract

`Application/force_servo_profile.h` contains one small, immutable reviewed
profile, separately read back and digested from writable PID parameters.
Profile selection confirms its ID; a register write cannot invent calibration,
raise a hardware boundary, enable peak output or unlock the build.

| Quantity | Current live experiment | Calibrated software path |
| --- | --- | --- |
| Profile ID / unit | 1 / 0: legacy control units | Fixture ID2 / unit1: N |
| Raw counts | Independent sensor value, valid below raw trip325 | Independent raw bounds and raw trip |
| Measured controller quantity | Existing raw-count identity; **not N** | raw * calibrated scale + offset; coverage checked |
| Desired target | Configurable integer1..275; default250 | `target_N` register/API; tests through3000 N |
| Operating maximum / hard trip | 275 /325 count-domain experimental limits | Operating max < hard trip < weakest hardware boundary |
| Newton qualification | None; requests rejected before START | Confirmed sensor range, calibration coverage, mechanics, current/time required |
| PRESS / RELEASE | 720 /100 command units maximum | Continuous and peak authority are separate profile fields |

The wire decoder verifies seven-byte frames and exposes raw counts. The prior
low-load display match does not establish a count-to-N calibration or full
sensor range. See [sensor evidence](Docs/PRESSURE_SENSOR_PROTOCOL.md). The live
allowed **calibrated force range is NONE**; 275 counts is not 275 N. Independent
raw protection remains active even when N conversion is unavailable.

The controller itself has no hidden275/325 clamps: it is unit-agnostic bounded
arithmetic. A calibrated profile applies N consistently to reference, error,
filtering, tolerances, tracking and overforce. Kp is command/N, Ki command/(N*s),
Kd command*s/N, reference rate N/s and acceleration N/s^2. No automatic gain or
threshold conversion assumes that one count equals one Newton.

Synthetic tests use scale2 N/count, offset10 N and a **fictional**3200 N fixture
boundary to exercise target3000 N, trip3100 N. These are not ZD3000 settings.
A separate boundary test rejects that profile at the actual drawing's3000 N
boundary and verifies a synthetic2900 target /2950 trip /3000 boundary ordering.
Those numbers demonstrate margin checks, not a proposed live rating or a
qualified stopping margin. Future values need measured uncertainty and stopping
overshoot evidence; a3300 N trip is never assumed safe above3000 N hardware.

## Time, progress and HOLD

Current defaults remain Kp10, Ki/Kd0, reference rate200 control units/s,
acceleration1000 control units/s^2, increasing output slew1000 command units/s
and immediate reduction of requested magnitude. RELEASE stays100. Cubic
reference duration is max(1.5*distance/rate, sqrt(6*distance/acceleration)).
For22 ->250 this is1.71 s; hypothetical22 ->3000 at unchanged numbers is22.335 s,
with reference402.908 at5 s. The latter calculation alone is not a Newton
calibration or permission for a longer run.

| Budget/diagnostic | Semantics |
| --- | --- |
| New sample age / active gap / receive-anchored lease |20 /125 /130 ms, unchanged; TIM5 reserves1 ms |
| Pending START / minimum control interval / reverse OFF |250 /5 /at least2 ms, unchanged |
| Requested HOLD dwell | New parameter, default500 ms; an observation goal, not an actuator HOLD rating |
| Live build / energized / session / capture ceilings |5000 /5000 /5000 /5000 ms maximum |
| Config session45000 and tracking10000 defaults | Representable tuning values; immutable5 s profile always wins |
| Planned reference + requested HOLD | Must fit profile AND selected config budget; PC also requires fit inside capture duration |
| Saturation elapsed | Continuous amplitude clipping telemetry only; not a stall diagnosis |
| Progress window / net threshold |500 ms /2 controller units; bounded diagnostic settings, not measured sensor noise calibration |
| No measured response | Fault5/Detail16 after min(config.saturation_ms, profile.no_response_ms), default5000 ms |
| Tracking elapsed | Starts only after reference completion, error over configured threshold; faults when overdue without current progress |
| Absolute expiry | Persists despite progress, HOLD, new samples or cap changes; independent TIM5 OFF and latched fault |

`SATURATING_WITH_PROGRESS` and `COMMAND_WITHOUT_MEASURED_FORCE_RESPONSE` are
separate telemetry/report states. Window anchors move only after a sufficient
net increase. Alternating noise does not repeatedly earn progress; creep too
slow to meet the threshold before the finite no-response deadline still stops.
Neither status diagnoses the motor, PWM, power supply or mechanics. Absolute
session/energized budgets are never renewed by progress.

The PC logs planned duration before START, reads target/config/profile/digests
back, then sends at most one START per invocation. Lost echo is UNKNOWN and is
never retried. Capture time begins before transmission of START. MCU pending
START remains OFF until a new valid fresh frame and a repeated plan check.
Rejected targets are not silently clamped. Parameters may be reduced/tuned
within reviewed bounds; exact-default equality and Target==250 are removed.

HOLD retains the same feedback controller and actual-output anti-windup. Initial
Ki0 may leave steady error; functional PI remains tested. Neither fixed PWM nor
successful host tracking proves physical force HOLD. Absolute output time includes
HOLD and logical-zero intervals; it is conservative elapsed energized eligibility,
not a measured integral of winding current or actual PWM on-time.

## Optional boost and hardware boundary

The software supports a distinct peak PRESS ceiling up to6000 command units,
nominal25% at the24 V mapping. Boost is a single continuous-path cap excursion,
not repeated PRESS/OFF pulses. Admission needs qualified profile flags, valid
contact, target and reference margin, fresh feedback, a healthy owner and enough
remaining absolute/cumulative budget. It keeps the selected output slew: at1000
command/s, zero ->6000 takes at least6 s with sufficient demand, not an instant
kick. The high-PWM host fixture explicitly uses synthetic Kp100/slew10000 and
800 ms/1600 ms peak/cumulative budgets to test actual CCR1200. **These settings
have no hardware approval.**

TIM5 is programmed to the earliest receive lease, fixed session deadline and
active boost deadline. A new sample cannot extend boost. Missing feedback or a
main-loop delay forces OFF independently. Early taper requests the normal
controller cap; the peak compare is retained until that lower command has been
physically installed and checked by the existing hardware guard. A pending
expiry wins; failed transfer forces OFF. STOP/FAULT do not refund the reserved
full boost duration. One admission per commanded session; cumulative reservations
survive STOP, fault reset, configuration writes and subsequent explicit STARTs
in the same boot. A reboot resets RAM accounting; it is not authorization to
repeat a thermal exposure. No live peak profile or runtime peak reset exists.

Current peak/duration/cumulative fields are all0: **BOOST_HARDWARE_ENABLED=NO**.
The720 command continuous operating cap is nominal3% PWM (CCR144/4800), not a
validated continuous thermal rating. The separately conservative RELEASE100
remains unchanged. No automatic escalation, current loop, current measurement
or I-squared-t protection has been invented.

The user's supplied ZD3000-60-170 drawing facts are24 V, maximum load3000 N,
no-load5 mm/s, stroke60 mm, use frequency10% and internal limit switches. They
establish an actuator design boundary only. Missing for a higher live profile:
confirmed sensor full-scale and multi-point N calibration/uncertainty/coverage;
weakest assembled frame/fixture/transmission limit and verified stopping margin;
motor/driver peak and continuous current, stall current, permissible peak/on/HOLD
time, cumulative exposure, minimum OFF/cooling time and the time base of10% duty.
These are not inferred from the drawing. The actual present supply setting stays
**0.5 A**, manually recorded; it is not a winding-current limit or measurement.

## One combined supervised static session

Use only profile1 and the present0.5 A supply setting, a fixed workpiece and
permitted travel. Confirm IDLE, output OFF, firmware identity, pressure feedback,
initial gap/contact and an immediately accessible physical stop. Record initial
temperature/current-limit setting. If active STOP and feedback-loss stopping are
still unvalidated, perform them in this same session with **separate deliberate
operator requests**: low target60 control units, at most1 s for each stage.
First command STOP during active output and verify physical cessation; next use
the established safe feedback-interruption method with MCU powered and verify
automatic output OFF/fault, then physical STOP. Restore feedback/reset only while
OFF; resetting must not start motion. If either fails, end the session.

Only if those prerequisites pass, take one useful static rise/taper/HOLD capture
at the selected legacy target (default250 control units), at most3 s, then verify
physical STOP. Keep total requested observation for these powered stages <=5 s;
no automatic repeats, no extensions to wait for HOLD, no autonomous force/current
increase. The plan-fit check can refuse a stage; do not bypass it. Separate stage
files belong to one combined session. Stop immediately for abnormal motion,
noise, visible limiting/dropout, temperature or mechanics. Do not test calibrated
3000 N or rotating loads. No extra observe-only day is required.

Field checkout and verifier require Git, Python and PowerShell; ARM tools are
not required for offline identity/HEX/ELF/configuration checks:

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
# One invocation = one deliberate START; choose the stage's target/time above.
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -ProfileId 1 -Target 250 -MaximumSeconds 3 `
  -ConfirmSupervisedMotion -ConfirmedFirmwareSha256 39967AF2116B38A5FDB65C2D44AE83C04D5BA7EA3559932B09C64D2FCBE56AD9 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'Combined static session; record STOP/feedback-loss results, motion, temperature and physical stop' `
  -OutputCsv "captures/static-force3000-1-$stamp.csv"
```

Use S/Escape for requested software STOP and the physical stop for emergencies.
Return CSV, report, metadata for the planned stages and one short field note.
Software `StopVerified` and planned CCR values are not external waveform or
hardware stop validation. `-TargetN` is present but fails before serial connection
on this profile with `MISSING_CONFIRMED_SENSOR_RANGE`; remaining missing
qualifications are listed above. Neither capture nor reset authorizes unlocking.

## Verification and provenance

SchemaF104/build46530106:25 float parameters (50 words),26 profile fields
(52 words, read-only), frozen60 u32 +22 floats (164 words). Reads retain the
existing11-word RTU chunk limit and production FC03/04 framing, station/function,
CRC, exact-length and timeout checks. FC03 profile starts0x180; FC06/03 profile
confirmation0x104; integer `target_N`0x103; legacy target0x0000. The legacy target
register is interpreted in the selected profile's controller unit. No writable
calibration or runtime unlock is exposed.

[Actual commands and results](Docs/StaticForce3000_1/TEST_RESULTS.md),
[parameters](Docs/StaticForce3000_1/default_parameters.json),
[schema](Docs/StaticForce3000_1/protocol_schema.json),
[current hashes](Firmware/ForceServo1.SHA256SUMS.txt).
The verifier pins actual HEX/ELF bytes, addressed load-image equivalence, all
profile/default fields, identity, arming state and protected legacy machine
configuration. Damaged files, profile mutations and old releases are rejected.
The measured/force_N numeric sentinel is0 when measured_valid=0; reports treat
that as unavailable. MCU raw peak includes raw trip samples; calibrated peak
contains only in-range samples, so it is not a bound on an overforce event.
Earlier field CSV/reports and firmware remain byte-preserved.

**HARDWARE_STOP_VALIDATION, static force/rise/HOLD, thermal duty and ROTATING_LOAD
remain NOT_VALIDATED. physical test NOT RUN.**
