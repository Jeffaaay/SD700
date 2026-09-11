# SD700 - PressBoost1 / FieldReady1 static field bundle

## CURRENT FIELD VERSION

**CURRENT FIELD CANDIDATE:** PressBoost1 / FieldReady1

**HEX:** [SD700_AutoTarget_PressBoost1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.hex)

Repository path: `output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.hex`

**SHA256:** `26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2`

**STATUS:** Target 250 / AUTO_HOLD real PressBoost1 validation pending.
Target 250 uses sensor control units, not certified Newtons. No 3500 N,
40 m/min or 60 m/min validation is claimed.

## FIELD UPDATE

First time (requires access to this private repository):

```powershell
git clone https://github.com/Jeffaaay/SD700.git
cd SD700
```

Later:

```powershell
git switch main
git pull --ff-only
```

Use only the firmware identified above, then follow the
[full static test instructions](Docs/AUTO_TARGET_STATIC_TEST.md): one START,
fixed workpiece, Target 250, no ApproachOnly.

Git retains source, tools, docs and exactly this HEX plus its matching ELF.
Builds, field captures, test logs, baselines and historical firmware/ReviewBundle
ZIPs remain local engineering evidence and are ignored. They are not supplied
by a clone. This Git setup did not change MCU sources, scripts or firmware and
did not build, connect, flash or start hardware.

## FieldReady1 capture update (existing baseline)

Tonight's physical-test candidate remains PressBoost1. This update changes ONLY
host preflight retry limits, report presentation/summary and tests/packaging.
No MCU source, parameters, protocol or firmware changed; no ARM rebuild ran.

BASELINE and TARGET_READBACK each allow at most10 complete fenced snapshots,
with20 ms only between incoherent attempts. Every attempt starts all four reads
anew; fields are never combined across attempts. Safety/capability/transport
errors abort immediately. Target write and START are never retried; lost START
echo still leads to the original STOP path. AUTO sampling/qualification/exit
logic, existing stability metrics and ApproachOnly behavior are unchanged.

The generated report adds observed maximum pressure, first observed Target+/-5
time, total/longest observed AUTO_HOLD spans, final coherent STOP Fault/Detail
and StopVerified. Missing final data is UNKNOWN. Existing PRESS RAM fields are
listed individually as RAM_NOT_READ with their debugger locations; Modbus cannot
supply the exact PRESS count, base/boost or per-pulse completion/feedback tuple.
No unobserved values are filled with zero/false. See the static instructions.

Start with [REVIEW_BUNDLE.md](REVIEW_BUNDLE.md). The ONE unchanged Release pair is
`output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.*`.
HEX SHA256: `26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2`.

Next trial: a static fixed workpiece, full AutoTarget, Target250 sensor control
units, ONE START, no ApproachOnly. Success requires real evidence of contact,
pressure rising clearly above the previous approximately30 plateau, approaching
250, reaching+/-5 and entering AUTO_HOLD. This has NOT been established here.
No Newton conversion, rotating-workpiece modes or controller tuning was added.
The prior user report peak31/final30/Fault5-Detail8 remains the physical unknown.

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
Rise>=2 retracts all boost. Two units is an explicit initial quantization-scale
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
automatic restart or timeout extension. Persistent low response still faults
at the original8000 ms convergence budget (Fault5/Detail8).

## Preserved limits and evidence

APPROACH: FIRST10000 mV/20 ms/backstop50, RECONTACT10000/10/40; contact20;
3000 ms total approach. First profile repeats before first contact as before.
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

FieldReady1 actually reran41 C host variants (23 original+10 PI/refusal+6 V5
races+2 AutoTarget O0/O2),11 C policy cases, both capture-script tests and the
physical-output-lock/source check: PASS. Capture tests cover13 framing groups,
20 preflight groups,220 safety/capability attempt positions,40 transport-error
positions and report output. MCU sources/HEX/ELF remain byte-identical. ARM
build this update: NOT RUN. Prior PressBoost1's10 ARM builds remain historical
PASS evidence; its Release is unchanged:text25364/data20/BSS2928.
Historical local results: `output/AutoTarget/TEST_RESULTS.md` (not tracked by Git).
These checks were run for FieldReady1; they were not rerun for the Git setup.

All software pressure curves are SYNTHETIC_INPUT. Powered PressBoost1 test,
Target250/HOLD/mechanical response, calibration and OFF-fall diagnosis: NOT RUN.
No production approval. Use the ONE full static test in the field instructions,
without ApproachOnly; the agent did not connect, flash or start the device.

Local pre-FieldReady1 scripts, docs and hashes are retained in
`output/AutoTarget/field_ready1_baseline/`. Pre-PressBoost1 source/build evidence
remains in `output/AutoTarget/press_boost1_baseline/`. Older ContactDeadline/CaptureFix1
and legacy evidence remain history. [Legacy control evidence](Docs/LEGACY_CONTROL_EVIDENCE.md)
and [reference constants](Config/legacy_control_reference.h) are not linked
into the firmware. Root SHA256SUMS.txt covers every Git-tracked file except
itself. Historical ZIP manifests remain in the local engineering evidence.
