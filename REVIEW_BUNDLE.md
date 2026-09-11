# ApproachMeasure1 - current field repository index

This main candidate measures the production starting gap with the existing
bounded coarse pulses. It removes the fixed 3000 ms search fault and starts
the existing 8000 ms convergence budget at first valid contact. PressBoost1
mapping/boost, Ki=0, RELEASE/HOLD and the execution/safety layers are preserved.

| Review item | Location |
| --- | --- |
| Current candidate, SHA256 and clone/update workflow | [README.md](README.md) |
| ONE START, static full AutoTarget command and measurement/RAM map | [Static test](Docs/AUTO_TARGET_STATIC_TEST.md) |
| Actual deterministic tests and ARM matrix | [Test results](Docs/APPROACH_MEASURE1_TEST_RESULTS.md) |
| New Release HEX | [SD700_AutoTarget_ApproachMeasure1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_ApproachMeasure1_RealBench_Release.hex) |
| Matching Release ELF | [SD700_AutoTarget_ApproachMeasure1_RealBench_Release.elf](output/AutoTarget/firmware/SD700_AutoTarget_ApproachMeasure1_RealBench_Release.elf) |
| Firmware hashes | [Firmware/SHA256SUMS.txt](Firmware/SHA256SUMS.txt) |
| All tracked file hashes except itself | [SHA256SUMS.txt](SHA256SUMS.txt) |
| Timing patch and RAM fields | [machine.c](Application/machine.c), [auto_target_config.h](Application/auto_target_config.h) |
| Runtime + real executor + fake TIM5/HW regressions | [test_auto_target.c](Tests/Host/test_auto_target.c) |
| Existing capture path and report | [capture_auto_target_static.ps1](tools/capture_auto_target_static.ps1) |

HEX_SHA256=8F537F2EF2FFA622BB92F1A6ED2C38972B57627337EC6B905C8283734826D56F

Local execution logs, pre-edit hashes and build metadata are indexed by
`output/ApproachMeasure1/test_execution.json`; capture tests are in
`output/ApproachMeasure1/capture_tests.log`. Individual ARM configuration logs
are `output/AutoTarget/build_*.log`. Generated files are ignored by Git; the
tracked test-results document records their actual outcomes and log hashes.
Prior work remains in previous commits/tags and the original local SD700
workspace. No history was rewritten; previous local firmware was not deleted.

Main has one current HEX/ELF pair. Software pressure inputs are SYNTHETIC_INPUT.
Hardware contact time/count, Target250/HOLD and mechanical safety are not yet
validated for this candidate. Save starting gap notes and the post-STOP RAM
dump; serial polling cannot provide exact approach measurements. No hardware
connection, flash or START was performed.
