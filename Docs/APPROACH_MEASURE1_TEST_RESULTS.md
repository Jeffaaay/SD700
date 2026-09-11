# ApproachMeasure1 - actual verification

Base: `ca68ba765d8f8221042f4c82fae57b8393791933` on Jeffaaay/SD700 main.
Candidate: `output/AutoTarget/firmware/SD700_AutoTarget_ApproachMeasure1_RealBench_Release.hex`

HEX SHA256: `8F537F2EF2FFA622BB92F1A6ED2C38972B57627337EC6B905C8283734826D56F`

ELF SHA256: `D49382BB519261E40A6453ADF18C660A75383B287F833741995D773FDCC8D546`

All pressure curves are **SYNTHETIC_INPUT**. They verify software event ordering,
not physical gap/contact response, pressure calibration or tuning. Powered
static contact/Target250/HOLD, mechanical safety and OFF-fall validation: **NOT RUN**.
No device connection, flashing or hardware START was performed.

## Deterministic change evidence

Before changing MCU code, the new long-zero-pressure test failed against the
current PressBoost1 baseline (O0 exit 1, expected): the existing search fault
violated `fault == FAULT_NONE`. The local `red-test.log` retains this result.
The updated tests then passed at O0 and O2, through ApplicationRuntime,
Modbus START/STOP admission, real MotorExecutor and fake TIM5/HW.

- Zero pressure with new valid frames beyond BOTH 3000 and 8000 ms, through
  12000 ms: repeated bounded 10000 mV / 20 ms / 50 ms backstop pulses, OFF and
  settled new feedback; no total search fault and no per-pulse budget reset.
- Fresh contact stops coarse output immediately and enters existing PressBoost1
  (Pressure30 -> requested PRESS3000 mV/10 ms), with the convergence clock
  anchored to MCU receive time. Independent pulse bounds remain unchanged.
- Prior ContactDeadline receive/dispatch order, wrap, duplicate/cache/future/
  invalid/stale samples, completion/contact races and safety/STOP priority.
  Fresh contact after the former 3000 ms boundary is now intentionally accepted.
- No-response convergence faults at exactly 8000 ms AFTER first contact; an
  already-contacted START retains its original clock. Recontact pulses remain
  10000 mV/10 ms/backstop40 and cannot restart that clock.
- Long initial approach stopped during pulse/settle delay/sample wait; overpressure,
  overcurrent, stale/invalid sensor, BACKSTOP and executor error stop safely.
  Late samples/completions cannot restart; tick-wrap and retained measurement
  lower bounds are checked. First-contact diagnostics freeze independently of STOP.
- Existing segmented PRESS, bounded two-response boost, effective-response
  retraction, near-target caps, normal completion and STOP/fault reset regressions.

## Actual results

| Check | Result |
| --- | --- |
| Original 23 C host variants | PASS |
| ForcePi / ordinary-real-mode AUTO+PI refusal, O0/O2 (10 variants) | PASS |
| V5 completion races O0/O2 (6 variants) | PASS |
| AutoTarget / PressBoost1 / ApproachMeasure1 O0/O2 (2 variants) | PASS |
| Total C host variants | 41/41 PASS |
| Existing C acknowledgement/policy cases | 22/22 PASS |
| AutoTarget opt-in compile policy | 11/11 PASS |
| Build-script invalid AutoTarget mode/ack cases | 4/4 expected rejection PASS |
| AutoTarget capture framing / preflight | 13 / 20 groups PASS |
| Full static / ApproachOnly / one START / lost echo STOP / report tests | PASS |
| Original pressure capture self-test | PASS |
| Physical-output-lock and direct-command ownership source checks | PASS |
| ARM Locked, RealCompileCheck, ScopeTest, RealBench, AutoTarget; Debug+Release | 10/10 PASS |

New Release: text25668 / data20 / BSS2960 bytes. Exactly one strong
`TIM5_IRQHandler` was checked in every real build. The matching Release ELF
contains `g_sd700_approach_diagnostics` (208 bytes); use symbol names, not old offsets.

Host compiler: `clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)`

ARM compiler: `arm-none-eabi-gcc (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119`

## Commands and local evidence

Run from repository root; all commands below actually ran successfully:

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/ApproachMeasure1/host
.\Tests\Host\run_completion_race_regressions.ps1 -OutputDirectory output/ApproachMeasure1/races
.\tools\run_auto_target_tests.ps1 -OutputDirectory output/ApproachMeasure1/tests
.\tools\capture_auto_target_static.ps1 -SelfTest
.\tools\capture_pressure_response.ps1 -SelfTest
.\Tests\Host\check_physical_output_lock.ps1
.\Tests\Host\check_direct_command_ownership.ps1
.\tools\verify_auto_target.ps1 -SkipHost
```

The final command ran the four invalid build options and all ten ARM builds;
its SkipHost message applies only to that invocation. Host suites ran separately
above. The AutoTarget Release invocation inside that matrix was:

```powershell
.\tools\build_gcc.ps1 -MotorMode RealBench -AutoTarget -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/AutoTarget/build
```

The retained HEX/ELF were copied byte-for-byte from that actual Release build,
not renamed from the old field candidate. Old local firmware remains untouched
and ignored; main tracks only this new pair. Generated logs/builds are not in
Git; `output/ApproachMeasure1/test_execution.json` records command exit codes.
The capture log was emitted by a separate successful SelfTest invocation.

Raw log SHA256 values (under local `output/ApproachMeasure1/`):

| Log | SHA256 |
| --- | --- |
| host_suite.log | `9374E0F034A1F3D338DE201FD14A4ACD1EA464594E45613E131ECDA4EBF3870A` |
| v5_completion_races.log | `2B20BDFEA79A2611CAB008A6CF6775722326AE02BF98B87A53495CA8DE6A4C8C` |
| auto_target_pressboost.log | `4A89C02DD8BB4D0243EDFB8382C53C8910C57B94155F3FB1384464C79EC06762` |
| pressure_capture.log | `7C83ECAFDF28C085C781F57A262B13D4DAB2EB54DD755B8A60695B6A847CD2A6` |
| physical_output_lock.log | `DFDF117A6E4BB4E3C12A10F82AAD14CF5DD88FDA6F28B16EF27FEF60F33D46E3` |
| direct_command_ownership.log | `D51B2B2A08768F00B922F25C1884C1D967573535944AE46510D48E94E83C9D32` |
| arm_matrix.log | `02D72791DEA0895E8C8CE6326D6587D5BCF65C89F6EC418D60640C9D7600273B` |
| capture_tests.log | `6175FB924D928258D1FB5E73902147E64B2EE81F2CDA137BA6D5435328106334` |

Individual ARM logs are under local `output/AutoTarget/build_*.log`. The repository
SHA256SUMS.txt covers every tracked file except itself; the firmware hash list
is also kept separately under Firmware/. No CI/CD or hardware automation was added.
