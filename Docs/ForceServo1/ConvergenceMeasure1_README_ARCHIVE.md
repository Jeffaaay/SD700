# SD700 - ConvergenceMeasure1 static measurement candidate

## CURRENT FIELD VERSION

**CURRENT FIELD CANDIDATE:** ConvergenceMeasure1 / PressBoostRetain1 and ApproachMeasure1 retained

**HEX:** [SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex)

Repository path: `output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex`

**SHA256:** `BB5486FB8318045416CE0C65668118CAA556F675AB7C6F403D1ED7F8192B780B`

**STATUS: physical test NOT RUN.** Target 250 / AUTO_HOLD validation pending. Target 250 is SENSOR CONTROL UNITS, not certified Newtons.
No 3500 N, 40 m/min or 60 m/min validation is claimed.

The only control change from baseline `c36a1da1f9892b583fafbbc3b27feaaf78d48ff7`
is `AUTO_TARGET_CONVERGENCE_TIMEOUT_MS`:8000U ->30000U. Timing semantics,
PressBoostRetain1 boost retention and every other control/safety setting remain
unchanged. This is a bounded measurement candidate, not proof of physical causes.

User-reported PressBoostRetain1 trial:
`press-boost-retain1-250-20260912-095632-272.csv` and the same-stem `.report.txt`.
Qualified AUTO peak59; valid stopped readback60 (separate from the AUTO peak).
Target250 was not reached and HOLD was not observed. Fault5/Detail8;
StopVerified=True. Late request readbacks stayed5000 mV/10 ms. Coherent
request94/pressure49 and request106/pressure59 were1224 ms apart in PC time.
This supports continued rise before shutdown;60 is not established as a physical
ceiling, and more time is not guaranteed to reach250. These files were not found
in the workspace; this records user-supplied evidence without fabricating files,
RAM values or current measurements.

The inherited ApproachMeasure1 change removed the fixed 3000 ms coarse-approach fault. Initial search
continues with the same bounded pulses, motor OFF, settle and new valid fresh
feedback until contact or STOP/fault. The 30000 ms convergence clock
starts at first valid MCU contact receive time, not START; an already-contacted
START begins the clock immediately. Recontact during convergence cannot reset
it. ContactDeadline's completion/error/STOP ordering remains. A per-pulse
BACKSTOP still faults (Fault 5 / Detail 6); that code no longer means a normal
3000 ms search expiration in this candidate.

Post-contact PRESS base mapping, RELEASE, HOLD, Ki=0, pulse timing,
MotorExecutor/TIM5/PWM/direction and all pressure/STOP protections are unchanged.
There is no automatic increase of approach voltage or pulse width. Initial
search has no firmware total time limit; the operator and existing capture
window terminate this supervised measurement when necessary.

The RAM record `g_sd700_approach_diagnostics` retains first-contact receive time,
pressure, coarse count and START-to-contact elapsed time. Without contact it
freezes elapsed time/count at STOP/fault as a measured lower bound. Read it
**after verified STOP, without reset/download**; the serial report identifies
these as `RAM_NOT_READ` until an operator saves the dump. PC polls cannot recover
exact contact timing/counts. Record the approximate initial gap alongside it.

## FIELD UPDATE

First time (private repository access required):

```powershell
git clone https://github.com/Jeffaaay/SD700.git
cd SD700
```

Later:

```powershell
git switch main
git pull --ff-only
```

Use only the firmware above and the [static test instructions](Docs/AUTO_TARGET_STATIC_TEST.md).
Static fixed workpiece, operator supervision, emergency stop/current limit ready,
ONE script START, full AutoTarget, **no ApproachOnly**; do not add a manual START.
First confirm that longer cumulative action time and thermal load are permitted.
Keep the existing current limit unchanged and record its actual setting and the
initial gap. Record visible current limiting/voltage sag, abnormal heating or
mechanical behavior and stop promptly. Return only CSV, report and brief field
notes; do not automatically repeat the test.
The example's 60-second PC observation window ends with STOP; it is not a new
firmware approach timeout. No physical result is claimed until field data shows
contact, rise beyond the previous qualified AUTO peak59, 245..255 and AUTO_HOLD.

Git tracks one current HEX/ELF pair. Previous firmware and generated logs/builds/
captures remain ignored local evidence; previous commits/tags are retained.
Actual tests/build results: [ConvergenceMeasure1 verification](Docs/CONVERGENCE_MEASURE1_TEST_RESULTS.md).
No hardware was connected, flashed or started by this change.

## Existing build reference (not tonight's test workflow)

| Build | AUTO / ForcePi | Direct PRESS/RELEASE | Output policy |
| --- | --- | --- | --- |
| Locked (default) | Existing dry-run AUTO; optional PI defaults off | Unsupported | Locked executor |
| RealCompileCheck | Rejected | Unsupported | Real executor unarmed |
| ScopeTest | Rejected | Existing fixed direct profile | Acknowledgement required; motor power disconnected |
| RealBench | Rejected | Existing fixed direct profile | Acknowledgement and fresh pressure required |
| RealBench **-AutoTarget** | Operator AUTO allowed; P-only configured and checked at boot | Existing direct aliases retained | RealBench acknowledgement/arming plus automatic pressure safety |

The ordinary direct profile remains 10000 mV / 100 ms / 150 ms backstop.
It is not an AutoTarget pressure-control parameter or a tuned holding profile.
AutoTarget is invalid with Locked, ScopeTest, or RealCompileCheck; it rejects
missing acknowledgement or arming, including explicit host tests.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -MotorMode RealBench -AutoTarget -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/AutoTarget/build
# Use -Configuration Debug for the other AutoTarget configuration.
```

Ordinary build commands remain:

```powershell
.\tools\build_gcc.ps1 -Configuration Debug -MotorMode Locked
.\tools\build_gcc.ps1 -Configuration Release -MotorMode Locked
.\tools\build_gcc.ps1 -Configuration Debug -MotorMode RealCompileCheck
.\tools\build_gcc.ps1 -Configuration Release -MotorMode RealCompileCheck
.\tools\build_gcc.ps1 -Configuration Debug -MotorMode ScopeTest -ScopeTestAck MOTOR_POWER_DISCONNECTED
.\tools\build_gcc.ps1 -Configuration Release -MotorMode ScopeTest -ScopeTestAck MOTOR_POWER_DISCONNECTED
.\tools\build_gcc.ps1 -Configuration Debug -MotorMode RealBench -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION
.\tools\build_gcc.ps1 -Configuration Release -MotorMode RealBench -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION
```

## Current post-contact PRESS mapping

All errors/rises below are existing sensor control units, NOT calibrated N.
For error e = target - pressure, outside HOLD and after eligible settled feedback:

| Error | Base PRESS request | Extra boost ceiling |
| --- | --- | --- |
| e <=5 and not above the target tolerance | Existing HOLD/output OFF classification | 0 |
| 5<e<=10 | 400 + floor((e-5)*600/15) mV | 0 |
| 10<e<=20 | Same fine formula; up to1000 mV | 1000 mV |
| 20<e<120 | 1000 +20*(e-20) mV | 2000 mV |
| e>=120 | 3000 mV | 2000 mV |

The near/fine bounds use the current enter tolerance5 and exit tolerance10
(fine upper = 2*exit). Micro uses 1000..3000 mV within the legacy 800..3000
reference range: starting at1000 avoids a downward step after fine's1000 ceiling.
Fine uses400..1000. These are unvalidated bench candidates, not tuned or proven
safe for the current mechanics. All fine pulses remain10 ms, backstop40 ms.

After TWO independent normally completed PRESS pulses each has settled rise
<2 control units, extra boost increases300 mV, then the pair counter resets.
A qualified rise>=2 clears the low-response count. In the same far band
(error>20), it retains existing boost capped at2000 mV; within the fine band
(10<error<=20), it still clears boost. Band changes still clear both.
Two units is an explicit initial quantization-scale
response threshold, not legacy1 N or a measured noise calibration. Each normal
completion can qualify once; old/duplicate/in-action frames, absent feedback,
BACKSTOP/error/cancel cannot raise boost. Existing50 ms OFF settle, strictly
newer post-gate receive timestamp/sequence,200 ms freshness and250 ms feedback
fault remain. Crossing bands clears history; error<=10 disables boost. HOLD,
RELEASE/direction change, loss of contact, STOP/fault and new START reset history.

The P-only ForcePi preview remains Kp8, Ki0, clamp+1200/-800 mV. AutoTarget PRESS
uses the explicit mapping AFTER the preview's safety/direction gates and BEFORE
Machine_StartPulse. It is not reclamped to1200. Its configured final ceiling is
5000 mV; MotorExecutor still validates/plans every request. RELEASE remains
min(8*positive excess,800) mV in the original direction path,10 ms, no boost.

| Target250, qualified correction sample | Zero boost | After two low responses | At extra-boost cap |
| --- | --- | --- | --- |
| Pressure30, error220 | PRESS3000 mV x10 ms | PRESS3300 x10 ms | PRESS5000 x10 ms |
| Pressure200, error50 | PRESS1600 mV x10 ms | PRESS1900 x10 ms | PRESS3600 x10 ms |
| Pressure240, error10, AUTO_SETTLE | PRESS600 mV x10 ms | PRESS600 x10 ms | PRESS600 x10 ms |

If already in HOLD at Pressure240, error10 is still inside the original exit
band: output stays OFF, with NO request. The table is requested amplitude,
not measured terminal voltage. No duration boost, integrator, unlimited ramp,
automatic restart or renewal of the running budget. Persistent low response
still faults at the configured30000 ms convergence budget (Fault5/Detail8).

## Preserved limits and evidence

APPROACH: FIRST10000 mV/20 ms/backstop50, RECONTACT10000/10/40; contact20;
No total approach deadline. First profile repeats before first contact as before.
The 30000 ms convergence budget starts at first valid contact receive time;
already-contacted START begins it at START. Pulses, rise, boost changes and
recontact cannot renew the active budget. Existing HOLD behavior remains: entry
clears the cycle clock; HOLD itself is exempt from convergence timeout, and
leaving HOLD starts the existing new correction cycle.
ContactDeadline receive/dispatch ordering and all safety priorities are unchanged.
Raw abort325 is an experiment threshold, NOT a known mechanical safety limit.
Target maximum275; HOLD enter/exit5/10. No electrical brake or energized preload.
Mechanical allowable load/current/travel and pressure calibration remain unconfirmed.

Existing Modbus request diagnostics and CaptureFix1 are retained. A small
`g_sd700_approach_diagnostics.press` RAM snapshot adds accepted request/count,
last completion reason, before/observed-peak/settled-after values and sequences.
`observed_off_fall` can flag sampled rise followed by lower settled pressure;
it is not true instantaneous peak evidence. Read it after verified STOP as in
[static instructions](Docs/AUTO_TARGET_STATIC_TEST.md). No protocol changes.

All 41 C host variants and the 10 ARM build configurations were run for
ConvergenceMeasure1, with policy and capture regressions; see the current test
record for commands, counts and hashes. All pressure inputs in software tests
are SYNTHETIC_INPUT, not physical response or tuning evidence.

Mechanical allowable load/travel and calibration remain unconfirmed. Abort325
is an experiment limit, not a certified mechanical ceiling. Powered contact,
Target250/HOLD and OFF-fall diagnosis remain NOT RUN for this candidate.

[Legacy evidence](Docs/LEGACY_CONTROL_EVIDENCE.md) and
[reference constants](Config/legacy_control_reference.h) remain historical.
Verify the shipped pair and actual ELF constants with
`python tools/verify_current_auto_target_firmware.py`; it requires30000 ms and
all existing amplitude, timing, PI and pressure-safety values.
Root SHA256SUMS.txt covers every Git-tracked file except itself. Generated
engineering evidence is local under output/ and is not supplied by a clone.
