# Target250Continuous2 - actual software verification

Baseline `c73e55ba505c4f7f43dff481c32155d0558075b8`, initially clean main equal to
fetched origin/main. Implementation preserves the existing controller, working
START, trajectory and safety timing. **POWERED_TEST_READY=NO. physical test NOT RUN.**
No serial connection, flash, powered motion, motor current, waveform, temperature,
physical STOP, Target250/HOLD or rotating-load validation was performed.

The software result is complete within the specified no-higher-rating fallback.
Retained100/100 limits and Kp1 are not presented as a performance solution.
Higher-range proof is SYNTHETIC host code execution, not hardware approval.
See [evidence/output trace](EVIDENCE_AND_PATH.md) for the precise missing ratings.

## Actual commands

Commands were run on this Windows workspace. Native subprocess return codes,
compiler commands and log hashes are recorded in [verification.json](verification.json).
[Captured test/build output](software_test_output.txt) is retained as ordinary
tracked documentation; raw local logs remain under `output/Target250Continuous2`.
No ZIP or package-verification flow was created.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -Target250Continuous2 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/Target250Continuous2/build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 -OutputDirectory output/Target250Continuous2/general_host
python tools/run_force_servo_tests.py --output output/Target250Continuous2/default_host
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/Target250Continuous2/final
```

Use a new evidence output directory when reproducing; never overwrite field data.
The last runner actually executes production PowerShell framing/Observe tests,
three capture self-tests, explicit-profile O0/O2/Os host tests, data/schema tests,
strict firmware rejection tests, offline verification, historical AutoTarget
verification, higher-range fixture O0/O2/Os, compile-policy gate tests and objcopy.
The actual individual arguments and exit codes are in verification.json.

| Check | Actual result |
| --- | --- |
| Full general host regression |33 executables +22 compile-policy cases PASS |
| Default ForceServo O0/O2/Os |15 groups each PASS |
| Target250Continuous2 explicit profile O0/O2/Os |34 groups each PASS |
| Host-only ceilings600/200 O0/O2/Os |34 groups each PASS, SYNTHETIC, defaults still100/100 |
| Compile/arming policy |9 cases PASS; locked, explicit gate, invalid combinations and higher non-host overrides |
| Production framing and full capture |31 PowerShell cases PASS |
| ForceServo / pressure / AutoTarget script SelfTest |PASS |
| Data/schema self-test |11 checks PASS |
| Target250/Continuous2 data tests |8 tests PASS |
| Strict firmware rejection tests |18 tests PASS, including empty-PATH offline CLI |
| Current firmware strict verification |PASS;30312 addressized load bytes |
| Developer objcopy cross-check |PASS |
| Historical AutoTarget verifier |PASS |
| ARM RealBench Release |PASS; text30212, data100, bss3880 bytes |

## Production-path evidence

Six new groups augment the original28 explicit-profile groups. They run the
actual Machine, ForceServo, real executor, TIM5, Modbus parameter transaction and
PWM hardware code with host register shims, not an isolated alternative controller.

A. Settled250/pressure22/Kp1/cap100 reproduces P/raw228, integer request100 and
the unchanged5000 ms saturation fault. Output OFF, fault latched, no later START
or fresh sample restarts automatically. This supports the software reading of the
reported5001 ms failure; it does not diagnose physical motor or sensor behavior.

B. Full48-word parameter staging/commit/readback, distinct PRESS/RELEASE maxima,
zero/out-of-range rejection with prior config intact. Current ceilings100/100 and
host-only ceilings600/200 both execute. The larger fixture admits actual600 PRESS,
599 same-direction update and200 RELEASE with matching host CCR; ceiling+1 in
each direction fails closed and cannot reuse the old session. Sweep operating400
is below test ceiling600; this distinguishes operating cap from permitted profile.
No larger hardware command was authorized by these arbitrary fixtures.

C. Fixed reference250 sweep at22,100,180,220,240,245,249,250,260,324. Current
cap100/Kp1 and synthetic cap400/Kp4 both have e_taper100. Tests verify raw P,
post-limit float, truncated request, committed command, amplitude flag, HOLD and
limited negative RELEASE after interlock. No forced minimum command is introduced.

D. Original stale-at-START OFF wait, duplicate START without renewal, pending/active
STOP, feedback loss, invalid/stale/order faults, receive-anchored independent lease,
raw overpressure, direction interlock, hardware guard, deadline and no-restart
regressions still pass. Feedback age20/gap125/lease130, build30000/session45000,
saturation5000/tracking10000 are unchanged. Default physical compile gate remains
LOCKED; the explicit image retains its old armed gate at100, without new unlock.

E. At5 ms control intervals, PRESS cap drops immediately to Kp at error1; increasing
magnitude resumes at5 command units per update. From0 with settled reference and
sufficient demand,100 is reached in100 ms and the synthetic400 cap in400 ms.
RELEASE -100 drops to -2 immediately, then increases to -7 after5 ms; reversing
still first commits OFF and waits deadtime. STOP during ramp and overpressure
force OFF. These are command-rate tests, not mechanical acceleration measurements.

F. Both directions cover commands1,4,5,6,99,100,profile ceiling-1 and ceiling;
zero continuous request physically disables the register shim; zero nonzero-plan
request rejects. Counts equal ceil(command*4799/24000), including rounding at5/6.
Above-ceiling and INT32_MIN requests reject. Old100 -> compare20 remains exact.

G. Original PI anti-windup against committed output, quantization exception,
small Ki0.2 effects and BUILD/HOLD integral continuity remain. The existing small
synthetic plant still returns P-only211.444/no HOLD versus Ki0.8/250.000/841 HOLD
samples of1000. These are SYNTHETIC model results only; shipped Ki=Kd=0. They do
not demonstrate that this real machine reaches250 or identify its delay/dead zone.

H. Verifier rejects malformed/checksum/length/overlap HEX, malformed/type/bounds ELF,
wrong defaults/limits, schema/build/contract/readiness/rate-policy mutation, altered
arming, historical predecessors and changed true hashes/manifest. It still runs
with PATH empty. No hash/configuration/safety assertion was removed for PASS.

The new per-session peak is tested on a valid sample between control updates,
duplicates, stale samples, STOP retention, next-session reset and raw-abort inclusion.
It avoids depending on faster PC polling; it is still sampled sensor evidence.
PowerShell verifies128-word frozen decoding, including nonzero peak267 and
post-limit98.5 test fields, full48-word FC03 config readback and ZERO START Observe.
All replies traverse production Read-LengthAwareResponse/Get-ModbusResponseLength;
wrong CRC/station/function/overlength, incomplete frames and exceptions still reject.
Simulated SingleStart sends once and never retries on fault/lost echo. Public CLI
readinessNO refuses before a serial connection. Active configuration above either
ceiling rejects. Reports separately label PC sampled peak and MCU recorded maximum,
saturation duration/first observed exit, polling gaps and unknown physical motion.

## Failures retained and resolved

`range_initial.log`: an older executor regression still assumed101 was always
illegal, so the600-ceiling fixture correctly exposed the stale test expectation.
The assertions now use direction profile ceiling+1. Actual rejection, OFF and
no-restart assertions remain, and the default100 build still rejects101.

`capture_initial.log`: the new negative public-CLI test correctly emitted
POWERED_TEST_READY=NO, but Windows PowerShell promoted native stderr to a terminating
NativeCommandError before the test could assert it. Only the test's child-process
capture temporarily uses Continue, restores Stop immediately, and checks nonzero
exit plus the exact readiness refusal. Production script error handling is unchanged.

The initial all-pass integration is retained under `verified`; final integration
under `final` additionally tests explicit command ramp time and new frozen fields.
No production C changed after the successful ARM build. HEX/ELF were copied as
bytes into the canonical current paths; preceding firmware bytes were preserved.

## Delivery

HEX SHA256 `2B99E36C3694BDC8A9E9A0145161CBA537432607AA3A2268176CD62A18D0CA83`.
ELF SHA256 `CE5CF188C03AA229288B57DF766615BF5819B0FD9E9494BDB9BF3967C62E35AA`.
F103 /46530104;24 defaults;16 contract words;47 u32 +17 float diagnostics.

Current retained caps100/100, Kp1 -> e_taper100; new pressure authority is NOT
established. Missing fitted-motor rating and proposed continuous motor-side
current/waveform/temperature evidence prevents a higher powered ceiling.
**POWERED_TEST_READY=NO; PHYSICAL_PERFORMANCE=NOT_VALIDATED; physical test NOT RUN.**
Hardware STOP, static250/HOLD, thermal duty and rotating load remain unvalidated.
