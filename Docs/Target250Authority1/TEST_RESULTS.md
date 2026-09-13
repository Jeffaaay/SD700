# Target250Authority1 - actual software verification

Baseline/local HEAD/fetched origin-main:
`a4c48457b723b715bc83c0ca5f2166df3735b737`. Worktree was clean. No reset,
force push, archive or historical evidence replacement. The user explicitly
selected720 as a SHORT supervised experimental ceiling, superseding the earlier
Continuous2 restriction. No new continuous motor/board rating was established.
**FIELD_STATUS=NOT_RUN. physical test NOT RUN.** No serial port, flashing, motor
operation, current/waveform/temperature measurement or physical STOP test occurred.

## Scope and resulting profile

Production firmware changes are limited to `force_servo_output_profile.h` and
the build ID in `force_servo.h`. Explicit profile Kp10/Ki0/Kd0, PRESS operating and
profile ceiling720, RELEASE100, experimental-readiness1. Default locked profile
retains Kp1/caps100/readiness0. Non-host overrides remain rejected; no runtime unlock.

Controller, trajectory, machine/START, protocol transaction, executor, PWM/TIM5,
HOLD, STOP, feedback and protection implementation files are unchanged. Existing
shared macros already carry the new limit through parameter validation and
executor planning/update; no hidden100 PRESS cap remains. SchemaF103 and its
128-word diagnostics are unchanged; identity advances to build46530105. Config
is48 words. Capture uses new identity/defaults, validates exact initial parameters
before SingleStart, defaults to5 seconds and rejects longer SingleStart before
serial connection. Existing Observe/Parameters mode limits remain up to60 seconds.

Reference200/accel1000/output slew1000, age20/gap125/lease130 ms, pending OFF wait250,
control minimum5, reversal OFF2, target maximum275/raw325/contact20 and finite
build30000/session45000/saturation5000/tracking10000 ms defaults are unchanged.
Increases use the existing slew; magnitude decreases remain immediate at the next
accepted control update. PI/quantization anti-windup and BUILD/HOLD continuity stay.

The selected taper is720/10=72 sensor units. At reference250 the user-specified
pressure22/100/178/200/220/240/245/249/250 sweep produces720/720/720/500/300/100/50/10/0.
Above target, RELEASE is limited to100 magnitude. These are verified software
calculations, not physical force results or validated braking distances.

PWM mapping is unchanged: ceil(720*4799/24000)=144. At20 kHz and ARR4799, nominal
duty144/4800=3%, nominal input high time1.5 us. Planned CCR is not externally
measured waveform or terminal voltage. Nominal command ramp0 ->720 is0.720 s at
1000 units/s with sufficient controller demand; reference movement and sample-grid
alignment can affect when that demand occurs. It is not a mechanical acceleration
claim. Keep the physical supply setting0.5 A; it is not a winding-current rating.

## Commands actually run

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -Target250Authority1 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/Target250Authority1/build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 -OutputDirectory output/Target250Authority1/general_host
python tools/run_force_servo_tests.py --output output/Target250Authority1/default_host
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/Target250Authority1/verified
```

The last command runs the production PowerShell framing/full capture suite, all
three capture self-tests, current real-output host tests, data/schema tests,
strict verifier rejection tests, current/historical firmware checks, host-only
range fixtures and compile gate tests. Exact subprocess/compiler arguments,
exit codes and original log hashes are in [verification.json](verification.json).
[Captured output](software_test_output.txt) is tracked with trailing whitespace
normalized; original local logs remain under `output/Target250Authority1`.
Use a new output directory to reproduce; do not overwrite evidence.

| Check | Actual result |
| --- | --- |
| General host regression |33 cases +22 compile-policy cases PASS |
| Default ForceServo O0/O2/Os |15 groups each PASS |
| Target250Authority1 O0/O2/Os |37 groups each PASS |
| Host-only ceiling900/200 fixture O0/O2/Os |37 groups each PASS; SYNTHETIC, not a field profile |
| Compile/arming policy |9 cases PASS; default locked, explicit armed, invalid combinations and ceiling overrides |
| PowerShell framing/full Observe/SingleStart |32 cases PASS |
| ForceServo/pressure/AutoTarget capture SelfTest |PASS |
| Schema/data self-test |11 checks PASS |
| Target250 data tests |8 tests PASS |
| Strict firmware rejection tests |19 tests PASS |
| Offline verifier with empty PATH |PASS; no ARM executable dependency |
| Pure Python firmware verification |PASS,30432 addressized load bytes |
| Developer objcopy cross-check |PASS |
| Historical AutoTarget firmware verifier |PASS |
| ARM RealBench Release |PASS; text30332, data100, bss3880 bytes |

## Production-path tests

Three new groups use the actual Machine -> controller -> real executor -> TIM5
and PWM hardware code with register shims. No replacement controller is involved.

- Exact initial defaults and full48-word parameter commit/readback. Settled-target
  sweep verifies raw P, float post-limit, integer request, committed output, limiter
  flags, HOLD and CCR144 at720.721 PRESS and101 RELEASE reject in the existing
  directional bound tests without accepting a misleading value.
- With reference already settled and sufficient demand,144 consecutive5 ms
  updates ramp0 ->720 in720 ms. Pressure249 immediately lowers command to10;
  returning low pressure then ramps up by5. STOP during ramp forces OFF and
  later fresh samples stay OFF. Reversal720 -> -100 commits OFF, remains OFF at
  1 ms and permits RELEASE only after the unchanged2 ms interlock.
- From actual720 committed output, overpressure, missing fresh feedback and
  independent TIM5 lease force OFF. Continued valid low-pressure samples still
  fault on the unchanged5000 ms saturation budget. Fault blocks START; new
  feedback and fault reset alone do not restart output.

All earlier stale-at-START waiting, pending STOP, active STOP, token/sequence,
lease races, feedback validation, contact, hardware guard and finite-deadline
regressions remain. MVP1 small-integral and synthetic P-vs-PI tests now explicitly
select their original Kp1/cap100 reference configuration so their original checks
remain meaningful. They do not describe Authority1 defaults. Initial Ki remains0.
Other synthetic plant regressions remain software-only, not machine identification.

PowerShell tests traverse production length parsing for capability,48-word FC03
configuration,128-word frozen diagnostics and STOP/readback. Existing CRC,
station/function, exception, exact-length and timeout checks remain. ZERO START
Observe is preserved; simulated SingleStart sends once, including lost echo or
fault. Added refusal tests cover6-second public CLI sessions and old100-cap active
configuration before START. Updated bound checks reject721 PRESS and101 RELEASE.
Reports keep the existing MCU session peak, sampled PC peak, saturation and
physical-motion/physical-stop distinctions; no new register layout was introduced.

The strict verifier requires actual new hashes, exact24 defaults and16 contract
words. Wrong Kp, former100 or above-ceiling721 PRESS, wrong RELEASE, identity,
readiness, timing, contract, arming, damaged ELF/HEX or manifest/hash mutation
reject. Continuous2 joins the rejected predecessor identities. No assertion was
removed or skipped to obtain PASS.

## Earlier failure retained

`output/Target250Authority1/initial_host.log` records the first run rejecting the
stale test assertion that the default PRESS cap was100. That assertion now requires
Kp10/PRESS720/RELEASE100. Target250 BUILD/HOLD expectations were updated for Kp10;
small-I/reference-plant tests explicitly retain their former test inputs. The
updated37-group suite then passed at all three optimizations. The failure log/hash
is retained rather than overwritten or represented as an earlier PASS.

## Firmware and field status

HEX SHA256 `322537355B059AA080B5C2356452D43C1C685E30889640D87E01CF5213037A48`.
ELF SHA256 `DB6D3364EF0BFC1DF05A6F0E1065EF052684E0DBE63EE8B117C54015614D1E06`.
Previous firmware bytes and manifests remain intact; the previous current manifest
is preserved as `Firmware/Target250Continuous2.SHA256SUMS.txt`.

One supervised session only: see [current README](../../README.md), COM5, Target250,
unchanged0.5 A, script's single START, observe maximum5 seconds and STOP/readback.
Stop immediately for abnormal motion/noise/current behavior; confirm physical stop
and return CSV/report/metadata. No automatic escalation or repeat. MCU saturation
and session deadlines remain separate from the PC observation window.

**FIELD_STATUS=NOT_RUN.** This does not certify720 for production or continuous duty.
Physical force buildup, Target250/HOLD, STOP, current/thermal behavior and rotating
load remain unvalidated; software PASS is not physical PASS.
