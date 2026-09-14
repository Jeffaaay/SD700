# StaticForce3000_Boost1 software test record

Candidate parent: `ed3c213b689929524dba2af016d85724097c7850`.
Initial HEAD=origin/main, clean worktree. No rollback, force push, PR or ZIP.
Execution timestamps are actual UTC values in [verification.json](verification.json)
(2026-09-14 UTC, 2026-09-13 local Pacific). **physical test NOT RUN.**

## Changes and constraints

One exact count-domain experimental profile: ID3, Target250 boost admission,
Kp10/Ki0/Kd0, continuous PRESS720, RELEASE100, peak6000, boost10 ms/total10 ms.
The72-unit taper margin follows720/Kp10. Existing target range1..275/raw trip325,
20 kHz PWM, control5 ms, fresh sample20 ms, feedback gap125 ms, receive lease130 ms,
START wait250 ms, reverse OFF2 ms and absolute5 s limits remain. PSU stays0.5 A.

A cap-only excursion with1000 command/s could not produce6000 in10 ms. The
admitted boost explicitly requests6000 and reports limit bit16, while retaining
normal PID calculations and their <=720 output as the reduction handoff. It is
not an old pulse loop. The actual-output commit uses the boosted authority only
while boost is admitted, with Ki0. PID Init/Prepare/Commit source is byte-for-byte
unchanged. START admission source is unchanged. The12 protected source files
listed in verification.json, including TIM5, PWM hardware, main/runtime, arming
gate, pressure conversion and old capture transport, are byte-for-byte unchanged.

The main-loop reduction runs at deadline-2 ms (nominal8 ms); TIM5's existing
1 ms reserve still stops at approximately9 ms if the handoff does not succeed.
The10 ms configured deadline is an upper budget, not a promised measured pulse
width. The transfer verifies the lower compare before restoring the original
receive-anchored lease, without new pressure, integration, or lease extension
from now. A pending expiry, late servicing or guard failure forces OFF.
Existing early target/reference taper still ends boost. Reserved10 ms is never
refunded by STOP/fault/reset/config/new START in a boot; max one peak per boot
is stronger than max one per session. No automatic restart or retry was added.

The immutable profile and executor retain continuous720 despite the distinct
6000 command ceiling. Writable cap721 is rejected. No Newton calibration or
qualification bit was invented. Historical synthetic profile/calibration and
large-range tests explicitly disable the new live boost unless testing it.

## Actually executed

All final commands returned exit0. Exact arguments, start times, durations,
log SHA256 and compiler commands are recorded in verification.json. The
[execution transcript](software_test_output.txt) contains actual output.

| Check | Result |
| --- | --- |
| `python tools/run_force_servo_tests.py --commissioning` |44 groups at each O0/O2/Os PASS |
| Same with `--range-fixture` |46 groups at each O0/O2/Os PASS; synthetic envelope only |
| Plain ForceServo host |15 groups at each O0/O2/Os PASS |
| `tools/run_host_tests.ps1` |33 host cases +22 compile-policy cases PASS |
| `Tests/Host/test_commissioning_gate.py` |9 lock/arming/invalid-build cases PASS |
| `Tests/Host/test_force_servo_capture.ps1` |37 cases PASS |
| `tools/capture_force_servo.ps1 -SelfTest` |PASS; production length parser, no serial |
| Pressure / AUTO capture `-SelfTest` |Both PASS |
| `tools/force_servo_data.py --self-test` |11 synthetic schema/data checks PASS |
| `Tests/Host/test_target250_data.py` |13 checks PASS |
| `Tests/Host/test_force_servo_verifier.py` |22 acceptance/rejection checks PASS |
| Current strict firmware verifier |PASS, pure Python; optional ARM objcopy equality also PASS |
| Historical AUTO firmware verifier |PASS |
| ARM RealBench Release |PASS; final source rebuild gave identical HEX and ELF |

The final regression driver was:

```powershell
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForce3000_Boost1/final-checks
```

Its historical filename remains; it ran current candidate tests and current
firmware verification. A small report correction also preserves failed reserved
boost attempts with no recorded successful peak as `peak_command=null`. After
that correction, the11 schema,13 data and37 production capture tests were
actually rerun under `output/StaticForce3000_Boost1/telemetry-checks`, all PASS.

The ARM command was:

```powershell
.\tools\build_gcc.ps1 -ForceServo -StaticForce3000Boost1 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForce3000_Boost1/build
```

ARM text38132/data100/bss4272 bytes; strict ELF/HEX comparison covers38232
addressed load bytes, including RAM initialization at its flash load address.
SchemaF105/build46530107;50 config words,52 immutable profile words,188 frozen
diagnostic words. Verifier pins exact identities, config, profile, arming gate,
symbols and hashes; malformed/modified HEX/ELF, profile/config mutations and all
prior candidates are rejected. Empty PATH and optimized Python checks remain.

HEX SHA256: `4EA0CDB953C06B4DB32F77746B16677B4421C752D828AC9F423F24BBC4DFF7E0`

ELF SHA256: `A301D51A61F312BE680D259832B83C34DAB2EF6A0D4D1527C310BF1E83A58D82`

## New bounded-boost coverage

- Live default profile with101 ms pressure cadence requests6000 and actual
  register shim reaches TIM3=1200, TIM2=0, guard valid, full10 ms reserved.
- At7 ms the boost remains active. At8 ms it hands off to the saved positive
  normal command <=720/CCR144. Control sequence and accepted pressure sequence
  do not change; logical deadline is original received_ms+130, not now+130.
- Next real synthetic frame at101 ms supplies the recorded after pressure;
  absent feedback is not represented as a measured zero. Peak and handoff
  records survive slow polling and STOP through actual framed capture/CSV/report.
- No main/sensor/PC service: TIM5 alone removes output at9 ms, then the machine
  faults. It cannot resume after the deadline or with an old token.
- STOP during peak, overpressure, invalid/out-of-order feedback, hardware
  mismatch, loss of new pressure after handoff and receive-lease expiry stop
  output. Reset and later samples remain OFF until an explicit START.
- Pending compare before transfer and injected compare after the lower hardware
  write both fail closed. Expiry is never cleared to permit handoff.
- Fresh target taper at5 ms lowers/exits; duplicate feedback cannot renew a
  compare; repeat arm and handoff above720 are rejected.
- Wrong peak/duration/cumulative profile, cap721, direct unbudgeted721 and boost
  duration11 ms are rejected in the live executor. Other valid tuning and low
  targets do not admit the peak. STOP/new START cannot refund spent budget.
- Existing same-direction updates, OFF transition on reversal, HOLD, trajectory,
  PI, finite session, saturation/progress, guard, timer and STOP regressions pass.

One initial O0 attempt failed because an older continuous PWM boundary fixture
used the now6000 peak ceiling for a normal continuous request. The assertion
correctly rejected it. The fixture now uses the distinct continuous ceiling;
separate peak tests prove6000 is reachable only under boost, and the original
invalid peak ceiling assertion remains. The failed log is retained and indexed;
no safety or firmware assertion was deleted to obtain PASS.

## Delivery limits and evidence preservation

The70 existing tracked evidence/document files checked against the parent
commit remain byte-identical, including prior HEX/ELF and raw field records.
The predecessor manifest is preserved as `Firmware/StaticForce3000_1.SHA256SUMS.txt`.
Current tracked-source hashes are maintained in the root SHA256SUMS.txt.
Regenerable build objects/executables and duplicate build outputs are removed
only after inspection; useful logs and synthetic capture results remain locally,
with the consolidated software evidence tracked here. No package is generated.

NOT RUN: serial connection, flashing, physical output, scope/current/thermal or
mechanical measurement, hardware STOP validation, static Target250 force
performance and rotating-load validation. User-reported prior3% field evidence
is recorded separately; no new physical pass is inferred from host/ARM checks.
This candidate is one short supervised breakaway experiment, **not a continuous
25% rating, a3000 N qualification, or permission to increase the0.5 A supply limit**.
