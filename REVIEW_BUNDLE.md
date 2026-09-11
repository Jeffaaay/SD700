# PressBoostRetain1 - current field repository index

The only control change from ApproachMeasure1 (`c5af4ed`) retains existing boost
within the current cap after a fully qualified effective PRESS rise in the same
far band (error>20), while clearing the low-response count. Base mapping,
low-response growth, fine/near and transition resets, search, timing, Ki0,
RELEASE/HOLD, STOP/Fault and execution/safety layers are preserved.

This is a strategy candidate; it does not establish all physical causes.
**physical test NOT RUN.** All host pressure input is SYNTHETIC_INPUT.

| Review item | Location |
| --- | --- |
| Current candidate, SHA256 and Git update workflow | [README.md](README.md) |
| COM5 / Target250 / maximum60 s / ONE START and RAM measurements | [Static test](Docs/AUTO_TARGET_STATIC_TEST.md) |
| Actual test/build results and field-evidence limits | [Test results](Docs/PRESS_BOOST_RETAIN1_TEST_RESULTS.md) |
| Current Release HEX | [SD700_AutoTarget_PressBoostRetain1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.hex) |
| Matching Release ELF | [SD700_AutoTarget_PressBoostRetain1_RealBench_Release.elf](output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.elf) |
| Firmware hashes | [Firmware/SHA256SUMS.txt](Firmware/SHA256SUMS.txt) |
| All tracked-file hashes except itself | [SHA256SUMS.txt](SHA256SUMS.txt) |
| Minimal control patch | [machine.c](Application/machine.c) |
| Runtime / real executor / fake TIM5/HW tests | [test_auto_target.c](Tests/Host/test_auto_target.c) |
| Existing capture path, updated candidate identity/report | [capture_auto_target_static.ps1](tools/capture_auto_target_static.ps1) |

HEX_SHA256=348F4ED990742346F4C56EED9CDB9C9311CA48AEAC9B29639A660B00ECEB584F

Main tracks one current HEX/ELF pair. Old firmware and original engineering
evidence remain locally and in history; no history was rewritten. User-supplied
field observations are recorded with their limits; original CSV/report were not
found in this workspace and were not synthesized or overwritten.

Local execution commands/logs: `output/PressBoostRetain1/test_execution.json`.
Source audit and firmware identity: `source_audit.json` / `firmware_identity.json`
in the same local directory. The tracked test document records actual outcomes
and log hashes. Generated logs are ignored and are not included in a clone/ZIP.

Delivery ZIP: `output/SD700_PressBoostRetain1_SourceOfTruth.zip`, created with
`git archive --format=zip --prefix=SD700/ HEAD` from the pushed delivery commit.
Its Git ZIP comment identifies the commit; root SHA256SUMS verifies its files.
The adjacent `.zip.sha256` records the ZIP hash. Both are separately delivered,
ignored artifacts. No hardware connection, flashing or START was performed.
