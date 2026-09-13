# ConvergenceMeasure1 - actual verification

Historical engineering record. Current delivery uses GitHub main; the old
archive workflow and generated build copies have been retired. Follow the
[current README](../README.md), not historical delivery instructions below.

Baseline and initial HEAD: `c36a1da1f9892b583fafbbc3b27feaaf78d48ff7`
(PressBoostRetain1). Initial worktree was clean; fetched origin/main matched.
No rollback, history rewrite or overwrite of existing evidence.

Candidate HEX: `output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex`

HEX SHA256: `BB5486FB8318045416CE0C65668118CAA556F675AB7C6F403D1ED7F8192B780B`

Matching ELF: `output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.elf`

ELF SHA256: `7E09AB5463A92158459739682B8DE1DE2E257A955347A1EC5DCDC5CEACEC2D2D`

**physical test NOT RUN.** No device connection, flashing or hardware START was
performed. Software pressure inputs are **SYNTHETIC_INPUT**, not physical data.

## Field evidence and scope

User-supplied file names: `press-boost-retain1-250-20260912-095632-272.csv`
and the same-stem `.report.txt`. Neither was found in the workspace. The supplied
observations are recorded as such; no files, RAM values or current measurements
were fabricated. Qualified AUTO maximum59 and valid stopped readback60 are
separate observations. Target250/HOLD were not observed; Fault5/Detail8,
StopVerified=True. Late request readbacks stayed5000 mV/10 ms. Coherent
request94/pressure49 and request106/pressure59 were1224 ms apart in PC time.
This supports continued rise before shutdown, not a physical ceiling of60 or
a guarantee that extending the budget will reach250.

The only production change is `AUTO_TARGET_CONVERGENCE_TIMEOUT_MS` in
`Application/auto_target_config.h`:8000U ->30000U. `machine.c` and all other
production/configuration files are byte-identical to the baseline. First valid
contact receive time anchors the active convergence budget; initial search
consumes none. Already-contacted START anchors it to that START. Pulses, rise,
boost changes and recontact during convergence cannot renew it. Expiry remains
Fault5/Detail8 with output disabled and the fault latched. Existing HOLD entry
clears the clock, HOLD is exempt, and HOLD exit starts its existing new cycle.

PressBoostRetain1, base amplitude mapping,5000 mV maximum,10 ms pulse,40 ms
backstop,50 ms settle,Ki0,contact20,HOLD/RELEASE,freshness,overpressure,TIM5,
STOP and fault latching are unchanged. No automatic restart or unlimited PRESS.
Capture changes are candidate identity and report text only. Sampling, preflight,
single START, STOP verification, output no-overwrite and return logic are intact.

## Actual deterministic results

Updated deadline tests failed against the unmodified8000 ms baseline at O0 as
expected: the no-fault assertion before30000 ms failed at its original timeout.
`red-test.log` retains that result. While developing the new supplemental tests,
HOLD and recontact state expectations were corrected to match the existing
implementation; no original safety assertion or production logic was relaxed.
Final O0/O2 runs cover:

- Continuous valid below-target feedback: alive at8000 and29999 ms, then
  Fault5/Detail8 and output OFF at exactly30000 ms after an already-contacted
  START. Late normal completion/fresh pressure cannot restart the latched fault.
- First contact at MCU3000 ms: healthy through32999, deadline at33000. Thus
  initial search consumes no budget. Existing receive/dispatch ordering checks
  remain. A separate initial search continues35000 ms with bounded first-profile
  pulses, then contact initializes the full budget and existing PRESS mapping.
- Loss of contact after3000 ms, regained at20000: active cycle anchor unchanged
  on every tick; still expires at30000. Recontact pulses remain10 ms/backstop40.
- A rising synthetic trace and boost growth/retention through9000 ms, across
  uint32 tick wrap, never renew the anchor. STOP, raw overpressure, invalid frame,
  stale feedback, per-pulse backstop and overcurrent still stop earlier than30 s.
  Late events cannot restart. These are software fault injections, not current
  measurements or thermal validation.
- HOLD reached in that extended window stays output OFF through31000 ms with
  fresh in-band feedback; its clock remains0. Leaving HOLD preserves its existing
  new correction-cycle behavior. Existing RELEASE, PressBoostRetain1 cap/reset,
  feedback gate, executor race, approach and capture tests all pass.

| Check | Actual result |
| --- | --- |
| Original C host suite including ForcePi O0/O2 | 33/33 PASS |
| Completion races O0/O2 | 6/6 PASS |
| AutoTarget / ConvergenceMeasure1 / PressBoostRetain1 O0/O2 | 2/2 PASS |
| Total C host variants | 41/41 PASS |
| Existing acknowledgement/policy cases | 22/22 PASS |
| AutoTarget compile-policy cases | 11/11 PASS |
| Invalid AutoTarget mode/ack build options | 4/4 expected rejection PASS |
| Capture framing / preflight groups | 13/13 and 20/20 PASS |
| Full static / ApproachOnly / single START / lost echo STOP / reports | PASS |
| Original pressure capture / output lock / direct ownership | PASS |
| Locked, RealCompileCheck, ScopeTest, RealBench, AutoTarget; Debug+Release | 10/10 ARM builds PASS |
| Shipped pair hashes and exact ELF configuration | PASS; timeout30000 ms |

Release size: text25724 / data20 / BSS2960 bytes. Every real ARM
build has exactly one strong `TIM5_IRQHandler`. Diagnostic RAM remains208 bytes.
The actual old ELF timeout is8000, new ELF30000; all other MachineConfig bytes
and all ForcePi configuration bytes match the old ELF. The current verifier
checks the complete17-word MachineConfig tuple, both release/overpressure enable
flags, raw abort325, full PI tuple including Ki0, RAM layout and both firmware
hashes. Historical `tools/package_auto_target.py` FieldReady1 hash/configuration
assertions remain unchanged. They are not used as current-candidate expectations.

## Reproduction and delivery

The same existing matrix runs with isolated outputs under
`output/ConvergenceMeasure1/`, leaving previous builds/logs/firmware/ZIPs intact.
Exact commands, script hashes, exits and log hashes are in local
`test_execution.json`. Host commands:

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/ConvergenceMeasure1/host
.\Tests\Host\run_completion_race_regressions.ps1 -OutputDirectory output/ConvergenceMeasure1/races
.\tools\run_auto_target_tests.ps1 -OutputDirectory output/ConvergenceMeasure1/tests
.\tools\capture_auto_target_static.ps1 -SelfTest
.\tools\capture_pressure_response.ps1 -SelfTest
.\Tests\Host\check_physical_output_lock.ps1
.\Tests\Host\check_direct_command_ownership.ps1
```

The ARM matrix runs `tools/build_gcc.ps1` for the five listed modes/configurations
with existing acknowledgement arguments and `-BuildDir output/ConvergenceMeasure1/build`.
The four invalid AutoTarget mode/ack combinations are required to fail. Current
Release and shipped-ELF verification commands:

```powershell
.\tools\build_gcc.ps1 -MotorMode RealBench -AutoTarget -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/ConvergenceMeasure1/build
python tools/verify_current_auto_target_firmware.py
```

The published pair is copied byte-for-byte from that Release build. Main tracks
only the new pair; old firmware remains on disk and in history. Historical test
records and original evidence are untouched. Root SHA256SUMS covers every tracked
file except itself. The source-of-truth ZIP is a `git archive` of the pushed
commit, containing source, HEX/ELF, manifests, current instructions and this
actual test record. Raw generated logs remain local and are not in Git/ZIP.

See [field instructions](AUTO_TARGET_STATIC_TEST.md). Before any hardware use,
confirm permitted longer cumulative action time and thermal load; preserve the
existing current limit, record its actual setting and initial gap. COM5,
Target250, PC maximum60 s, one script START, no additional manual START and
no ApproachOnly. Record current limiting/voltage sag, abnormal heating or
mechanical behavior and stop promptly. Return CSV, report and brief notes only;
no automatic repeat. The field capture command was not executed here.

## Actual log SHA256

Paths relative to local `output/ConvergenceMeasure1/`:

| Log | SHA256 |
| --- | --- |
| host_suite.log | `9374E0F034A1F3D338DE201FD14A4ACD1EA464594E45613E131ECDA4EBF3870A` |
| v5_completion_races.log | `2B20BDFEA79A2611CAB008A6CF6775722326AE02BF98B87A53495CA8DE6A4C8C` |
| auto_target_pressboost.log | `88C4F7012B13772BE12589A1216FFD29EF8AF75E208002A06196B40F79370D0A` |
| capture_tests.log | `BF4AC5C9FA908F35E25666738F8E02CCAE1FA76A3184292D509A05489B2530DE` |
| pressure_capture.log | `7C83ECAFDF28C085C781F57A262B13D4DAB2EB54DD755B8A60695B6A847CD2A6` |
| physical_output_lock.log | `DFDF117A6E4BB4E3C12A10F82AAD14CF5DD88FDA6F28B16EF27FEF60F33D46E3` |
| direct_command_ownership.log | `D51B2B2A08768F00B922F25C1884C1D967573535944AE46510D48E94E83C9D32` |
| build_reject_Locked.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_RealCompileCheck.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_ScopeTest.log | `40FA0A91E89AC37E9293B6D89E69A7268F4AC5644ABBD004E765AA11174A8574` |
| build_reject_RealBench.log | `97518CE482477A6966D8C4DBDDB5E2E46ABD08537F5DB3A1F0E170D8C9FE14A0` |
| build_Locked_Debug.log | `A31C07393BDA7C82F10F82B56E9E257A19BF4F573268A63C9AE5F1CB0A1B01D9` |
| build_Locked_Release.log | `B21C48D8B2CB0F0A92956F7A4DC76D6B457C36F03B2BE6A0BE48C96E958C7D09` |
| build_RealCompileCheck_Debug.log | `0A6E6F6C6015A9D79FB36D3EEF45F318C9FD4B20848B898F1376BA8434D373B2` |
| build_RealCompileCheck_Release.log | `7CD40A311A48F1143F88B6BFB4D35E3144BAE0B2D57ECB795561CAA5C8F7139C` |
| build_ScopeTest_Debug.log | `03CAD0381FDCAC313F482CD045A1DAD03EA2C042117E806B17C94BAA1E7B16D1` |
| build_ScopeTest_Release.log | `F2EAE9869D7267DA25998872A9543D854302AAD7429D7FC2A9260DAB1025186F` |
| build_RealBench_Debug.log | `7A84525356CACB0DE816711ACEE7985CBE6669541B6DCE8015CF1BC13ABD9B9B` |
| build_RealBench_Release.log | `A73FA6EB7E353DE386B09E5F16C1609A194D24F497567504B2A2EA2DAFE51488` |
| build_AutoTarget_Debug.log | `FA50F3E855AD13F9BD1D82219CD77A52A7149F98EF8E9AB7387D53C9E559EFCC` |
| build_AutoTarget_Release.log | `451849E85A1D61DE9598B3385DEBDF03E552D53FB2F8EC403A4A13801CA35B50` |
| firmware_verification.log | `0FFB377F77B0FEC8EE0677A4652C9D7AD983266A2328291DFF2C464830CD4E8A` |
