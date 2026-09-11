# PressBoostRetain1 - actual verification

Base and initial HEAD: `c5af4edcb9bbcebaf647e85cd89af4139dbdfbf9` (ApproachMeasure1).
Initial worktree was clean; fetched origin/main had no newer commits. No rollback.

Candidate: `output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.hex`

HEX SHA256: `348F4ED990742346F4C56EED9CDB9C9311CA48AEAC9B29639A660B00ECEB584F`

ELF SHA256: `E3C5A08788D955881E691600209A4E083A83FB564BC7EFC2C59328F38EF4B022`

**physical test NOT RUN.** No hardware connection, flashing or START was performed.
All software pressure inputs are **SYNTHETIC_INPUT**. Host/build PASS verifies
software behavior; it does not establish physical contact, Target250/HOLD,
pressure calibration or the complete physical root cause.

## Scope and prior field evidence

The user supplied: one START, approximately 10 mm starting gap, approximately
4 s to contact (field estimate), observed maximum58, Target250 not reached,
HOLD not observed, Fault5/Detail8 and StopVerified=True. The supplied CSV summary
reports multiple post-contact PRESS requests at5000 mV/10 ms, including coherent
request92 at pressure42/5000 mV and request94 at pressure44/3000 mV. These sampled
requests motivate a strategy change; they do not identify each pulse's settled
response or establish all physical causes. Original field CSV/report were not
present in this workspace and were not recreated, overwritten or relabeled.

Only production change: in `Machine_ConsumePressFeedback`, after every existing
normal-completion, request-match, fresh/new sample, motor-OFF and settle gate,
a rise>=2 in the same far band (`Machine_PressBand == 3`, error>20) resets the
low-response count and retains existing boost within the2000 mV band cap.
Existing two-low-response +300 mV growth, all band transitions, fine/near
retraction and STOP/Fault/contact-loss/RELEASE/HOLD/START resets are unchanged.

Source audit: every other tracked production/configuration file is byte-identical
to the baseline. Ki0, base mapping,5000 mV maximum,10 ms pulse,40 ms backstop,
50 ms settle,8000 ms convergence, contact threshold, ApproachMeasure1 search,
TIM5, STOP and overpressure protection are unchanged. No automatic restart.
The capture script changes only four candidate identity/report-text lines;
protocol, preflight, capture, single START, STOP and no-overwrite logic are unchanged.

## Tests and results

Before the MCU edit, the updated host test failed on the baseline at O0:
after pressure30->31->32->33->35, the expected3300 mV request was instead3000.
`red-test.log` retains the expected failure. After the edit, O0/O2 pass:

- Same far band: rise exactly2 retains300 mV boost and clears an outstanding
  low-response count of1; two subsequent low responses are required to grow again.
  Rise>2 at the2000 mV boost cap retains5000 mV. With a smaller base in the same
  band, pressure200 requests3600 mV and pressure229 requests3020 mV.
- Repeated low responses still grow by300 mV per pair and saturate at2000 mV
  (far) /1000 mV (fine). Exact error20 crossing clears boost/count; effective
  rise within fine still clears them. Error<=10 remains unboosted.
- Retained nonzero boost and an outstanding low-response count clear on contact
  loss, HOLD, RELEASE, STOP in pulse/delay/sample-wait, BACKSTOP, output failure,
  overpressure, invalid/stale feedback and feedback timeout. Late events do not
  restart output; a later explicitly admitted START begins with zero boost.
- Apparent rise in-action, before OFF settle, at the gate timestamp, cached before
  the gate, or with duplicate sequence/completion cannot clear the count or consume
  feedback. A new qualified rise then clears the count and retains boost once.
  Mismatched request feedback cannot set `feedback_valid` or retain boost; invalid
  frame/control-unit and future samples fault without qualifying the rise.
- Existing approach/contact ordering, convergence deadlines, tick wrap, executor
  completion races, safety priority and capture regressions remain passing.

| Check | Actual result |
| --- | --- |
| Original host suite, including ForcePi O0/O2 | 33/33 PASS |
| Completion races O0/O2 | 6/6 PASS |
| AutoTarget / PressBoostRetain1 / ApproachMeasure1 O0/O2 | 2/2 PASS |
| Total C host variants | 41/41 PASS |
| Existing acknowledgement/policy cases | 22/22 PASS |
| AutoTarget compile-policy cases | 11/11 PASS |
| Invalid AutoTarget mode/ack build arguments | 4/4 expected rejection PASS |
| Capture framing / preflight groups | 13/13 and 20/20 PASS |
| Full static / ApproachOnly / single START / lost echo STOP / reports | PASS |
| Original pressure capture self-test | PASS |
| Physical output lock / direct-command ownership checks | PASS |
| Locked, RealCompileCheck, ScopeTest, RealBench, AutoTarget; Debug+Release | 10/10 ARM builds PASS |

Every real ARM build has exactly one strong `TIM5_IRQHandler`. Release ELF
configuration bytes match the ApproachMeasure1 ELF exactly. Diagnostic RAM
layout remains208 bytes; use symbols with the matching ELF, not old addresses.

Release size: text25724 / data20 / BSS2960 bytes.

Host compiler: `clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)`

ARM compiler: `arm-none-eabi-gcc (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119`

## Reproduction and retained engineering evidence

All new logs/builds are isolated under `output/PressBoostRetain1/` so previous
engineering output and firmware remain untouched. Exact commands, script hashes,
exit codes and log hashes are in local `test_execution.json`. This follows the
existing verification matrix with a separate BuildDir, avoiding the fixed old
output paths in `verify_auto_target.ps1`:

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/PressBoostRetain1/host
.\Tests\Host\run_completion_race_regressions.ps1 -OutputDirectory output/PressBoostRetain1/races
.\tools\run_auto_target_tests.ps1 -OutputDirectory output/PressBoostRetain1/tests
.\tools\capture_auto_target_static.ps1 -SelfTest
.\tools\capture_pressure_response.ps1 -SelfTest
.\Tests\Host\check_physical_output_lock.ps1
.\Tests\Host\check_direct_command_ownership.ps1
```

The matrix ran `tools/build_gcc.ps1` for Locked, RealCompileCheck, ScopeTest,
RealBench and RealBench -AutoTarget, each with Debug and Release, using the
existing mode acknowledgements and `-BuildDir output/PressBoostRetain1/build`.
It also checked -AutoTarget with the first three modes and unacknowledged
RealBench, requiring all four to fail. The current firmware came from:

```powershell
.\tools\build_gcc.ps1 -MotorMode RealBench -AutoTarget -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/PressBoostRetain1/build
```

The published HEX/ELF are byte-for-byte copies from that Release output. Only the
new pair is tracked; the original pair remains on disk and in Git history.
Root `SHA256SUMS.txt` covers every tracked file except itself. The source-of-truth
ZIP is a `git archive` of the delivery commit, including source, this test record,
manifest and the single current pair. Generated raw logs remain local, not in
Git or the ZIP. The ZIP commit comment and manifest bind it to the pushed commit.
See [field instructions](AUTO_TARGET_STATIC_TEST.md) for COM5, Target250,
maximum60 s observation and a single START. This command was not executed here.

## Actual log SHA256

Paths below are relative to local `output/PressBoostRetain1/`.

| Log | SHA256 |
| --- | --- |
| host_suite.log | `9374E0F034A1F3D338DE201FD14A4ACD1EA464594E45613E131ECDA4EBF3870A` |
| v5_completion_races.log | `2B20BDFEA79A2611CAB008A6CF6775722326AE02BF98B87A53495CA8DE6A4C8C` |
| auto_target_pressboost.log | `12C36FE7549DA6F586D78144FF96C4A62F5CB0C7FDFE2A0C1AB8906371B58E01` |
| capture_tests.log | `BF4AC5C9FA908F35E25666738F8E02CCAE1FA76A3184292D509A05489B2530DE` |
| pressure_capture.log | `7C83ECAFDF28C085C781F57A262B13D4DAB2EB54DD755B8A60695B6A847CD2A6` |
| physical_output_lock.log | `DFDF117A6E4BB4E3C12A10F82AAD14CF5DD88FDA6F28B16EF27FEF60F33D46E3` |
| direct_command_ownership.log | `D51B2B2A08768F00B922F25C1884C1D967573535944AE46510D48E94E83C9D32` |
| build_reject_Locked.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_RealCompileCheck.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_ScopeTest.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_RealBench.log | `97518CE482477A6966D8C4DBDDB5E2E46ABD08537F5DB3A1F0E170D8C9FE14A0` |
| build_Locked_Debug.log | `493463E4E26176DEF77D84F17E48ED30817B125CCFBDE02331D39165FE909FF3` |
| build_Locked_Release.log | `39FB345976B101F7C8535BDF2859EBCA0D80F9DD5052F31AD76C244972FDE534` |
| build_RealCompileCheck_Debug.log | `DCA9970E641D1BF563E86F2BDF4D5B2D4CE6E235D5AB56338008BF871E03CF92` |
| build_RealCompileCheck_Release.log | `D0A99A36028950C1CF5C847DA7778FC1BEDCE80F673A20ACDE2A294EE92CED70` |
| build_ScopeTest_Debug.log | `349DCA7AFCB079433B6CDDED7D6F1B0F900AD009DA5A2B4CDE3F7136B809D62B` |
| build_ScopeTest_Release.log | `8B0606A0AF92DC7169DC608C391B8CF7E90A2444CA681B8C6D388CE533BB6FD5` |
| build_RealBench_Debug.log | `2D89450F0315D6E3C505735F8F66CF3CC3AF537C98E87D99F683635D1B210666` |
| build_RealBench_Release.log | `7360204CE5F6CD68908EF42B7998430FC89B4D6844968795AE7204E2788E1414` |
| build_AutoTarget_Debug.log | `687B980E2B4B2B821A6288ACD55B1BA167CB0CC850C70908AD7F5F8A63A9DA9C` |
| build_AutoTarget_Release.log | `5C00B3B674033F48E3ACBAADA65D2183C372382C0836EED308F2D73AC5A63DC4` |
