# Target250MVP1 - actual software verification

Baseline: clean main/origin-main `3e4208df808fa7c78731ed589e0cf13a63da81a9`.
No later main commits were present when fetched. The interrupted diagnostic-only
proposal had left no tracked changes at this task's start; no reset/force push
was used. The existing ForceServo output flag/backend is reused for Target250.

**FIELD_STATUS=NOT_RUN.** No serial connection, flashing, motor motion,
physical STOP, current/thermal measurement, Target250 response or HOLD validation
was performed by this work. Prior field Stage A was user-reported and applies
to CommissioningUnlock1; its B/C did not execute active motion.

## Implemented boundary

One `start_pending` flag and request timestamp defer an operator START to the
next new valid frame; physical output/lease stay OFF in IDLE. STOP clears the
flag. A duplicate START is BUSY and cannot extend the250 ms no-output wait.
Timeout, invalid pressure or a fault cancels the request with no restart.
The next fresh frame revalidates target/configuration/health/OFF/lock before
using the existing continuous executor. The old pulse/run entry remains rejected.
No machine-state enum or second controller was added.

Target250 is selected at boot and by the capture command. Former commissioning
raw20..30 / target<=60 PC gates and contact-at-START refusal are removed. Valid
pressure below contact can build continuously; once contact is observed, contact
loss still faults. Max target275/raw-abort325, STOP priority, output guard,
break-before-make2 ms, fault latching and explicit restart remain in force.

P-first defaults: Kp1, Ki0, D0, reference rate200 and acceleration1000 (existing
allowed upper bounds), slew1000, PRESS/RELEASE caps100 mV each. Caps remain
RAM-adjustable1..100. No higher continuous rating was inferred from pulse tests
or the0.5 A field supply limit. The physical current limit was not changed.

New-sample age20 ms remains. The125 ms gap covers the reported101 ms interval
plus20 ms delivery age, rounded up on the existing5 ms grid. Lease130 adds one
slot; independent TIM5 still compares1 ms early. Both executor and lease timer
enforce this profile's upper bound. Timer IRQ/stop/renewal mechanics are unchanged.
The default locked profile retains its previous reference/timing parameters.
Build30000 ms, total45000 ms, saturation5000 ms and tracking10000 ms defaults
are unchanged. These are finite supervised budgets, not hardware certification.

The existing I term stores the scaled integral contribution in command mV.
Pure integer-command truncation formerly entered actual-output back-calculation
even without a physical limiter. With small Ki/error this could cancel each
sub-mV increment before I affected the integer command. In the Target250 profile,
only that pure-quantization back-calculation is suppressed; amplitude, slew,
interlock and actual derating still track committed output. Integral limits and
BUILD/HOLD continuity remain. The default locked controller is unchanged.

## Executed results

Exact commands, individual O0/O2/Os compile arguments, log paths/hashes, historical
firmware hashes and actual firmware verification are in
[verification.json](verification.json). Logs live under `output/Target250MVP1`.
They are software/synthetic evidence, not field CSV or physical measurements.
The environment uses Clang22.1.8 through its `gcc` entry, Arm GNU14.2.1,
Python3.12.10 and Windows PowerShell5.1.19041.6456.

| Check | Actual result |
| --- | --- |
| Full relevant existing host suite | 33 cases +22 compile-policy cases PASS |
| Default ForceServo | O0/O2/Os,15 groups each PASS |
| Target250 real-output profile | O0/O2/Os,28 groups each PASS |
| Default locked / explicit real-output gate / invalid build combinations | 7 PASS |
| Production serial framing and complete capture | 29 PowerShell cases PASS |
| Existing ForceServo, pressure and AutoTarget capture self-tests | PASS |
| Existing Python schema/data self-test | 11 checks PASS |
| Target250 response metrics | 6 tests PASS |
| Strict firmware rejection suite | 17 tests PASS |
| Pure-Python current firmware verifier, including empty PATH | PASS |
| Developer objcopy cross-check | PASS |
| Historical AutoTarget verifier | PASS |
| ARM RealBench Release | PASS; text29968, data100, bss3856 bytes |

The seven Target250 groups add the exact24 ms stale-field request through the
actual Modbus server, zero-output waiting, duplicate/no-renewal behavior,
exactly-once initialization, pending STOP/timeout/invalid/stale/overpressure and
timestamp-wrap cases. Fresh pressure alone in IDLE or after STOP/reset cannot
start output. Tests also cover starting below contact without a pulse, PRESS
at low pressure, command reduction near250, active HOLD and limited RELEASE,
and small positive error with Ki0.2 affecting the final committed command.
At most20 ms delivery age with repeated101 ms periods stays active; a126 ms gap
faults before late feedback can hide it. With main stopped, TIM5 alone switches
OFF at129 ms for the130 ms lease. STOP, no restart, caps, anti-windup, congestion,
hardware guard corruption and both direction transitions remain tested by the
retained groups. The locked build uses the actual production arming gate test.

The small synthetic spring/load model has equilibrium `pressure=20+5*command`.
With the same target/controller, P-only finished at211.444; Ki0.8 finished at
250.000 with841 HOLD samples out of1000 at20 ms steps. The test requires the PI
result near250, active HOLD, bounded commands and no fault. It proves the I path
can remove a synthetic load's static error; it does not tune or identify the real
press. Default Ki remains0 for the requested P-first field measurement.

Capture tests preserve FC03 whole/fragmented frames, timeouts, CRC/station/function/
overlong rejections, exceptions and FC04/05/06 regressions. Every simulated reply
uses production length parsing. Complete Observe stays ZERO START. Current
SingleStart sends exactly one START, keeps TARGET_READBACK, records acceptance,
and preserves CSV/metadata/report on lost echo/fault. Lost echo is unknown, never
a resend. Target!=250 and locked firmware refuse START. Pressure0 or31 no longer
triggers the obsolete commissioning gate. Operator STOP ends observation promptly,
sends one final STOP and verifies readback. Final metrics tests cover full curve,
peak/overshoot/settling/HOLD, saturated versus unsaturated failure, equality at
the cap, pending data from an old session, gaps, and absence of motion evidence.

## Actual commands and failures retained

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -Target250MVP1 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/Target250MVP1/build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 -OutputDirectory output/Target250MVP1/general_host
python tools/run_force_servo_tests.py --output output/Target250MVP1/default_host_final
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/Target250MVP1/verified
```

After adding cap-equality reporting, the affected PowerShell capture29 cases,
integrated capture/schema self-test and Target250 metrics6 tests were rerun and
passed; command records identify their final logs. Firmware/control source did
not change after the ARM build and final O0/O2/Os runs.

Two early test assumptions failed and were corrected without weakening runtime
guards: pulse refusal while a continuous owner existed correctly returned BUSY,
so the idle-refusal test now STOPs first; HOLD exit at error exactly10 correctly
stays in HOLD, so the exit test now uses11. Both failure logs are retained. One
PowerShell redirect of Python unittest's stderr produced a shell error status
despite six `ok` results; rerunning via subprocess captured the real Python
exit0 and a clean log (`data_final_native.log`).

## Firmware and delivery

HEX SHA256: `4277556949A4AD6A2D5D24E49F8A1DEA97C7D8906CA48AA9878D68B5DF924055`

ELF SHA256: `06B8281B8D089D97D5639C6176A94F962AB5297FE04EEAE9CC32AF5B5DB31B01`

Actual ELF contract: schemaF102/build46530103, real-output1, max target275,
raw abort325, build30000, caps100/100, target250, pending250, gap125, lease130.
All24 defaults and30068 addressed load bytes are checked. Wrong identity, timing,
target, caps, gate, malformed HEX/ELF and real-hash mismatches are rejected.
The old locked and CommissioningUnlock1 images are rejected as the current
candidate while their originals remain byte-for-byte preserved.

Use only the [one-session Target250 procedure](../../README.md), not the earlier
broken commissioning run. Let stable HOLD occur before the final operator STOP;
there is no second START. GitHub main is the delivery; no ZIP or package step.
**Physical motion, Target250 performance, active hardware STOP, thermal duty and
HOLD stability of this candidate remain unvalidated. FIELD_STATUS=NOT_RUN.**
