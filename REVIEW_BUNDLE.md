# ConvergenceMeasure1 - historical engineering index

This is a historical record. The current firmware, field instructions and
GitHub-main delivery policy are in [README.md](README.md). Former archive
delivery has been retired; this index does not authorize the old field test.

The only production change from PressBoostRetain1 (`c36a1da`) increases the
convergence timeout from8000 to30000 ms. Timing anchors and HOLD semantics,
PressBoostRetain1, base mapping, limits, pulses, feedback gates and all execution/
safety behavior are preserved. This remains a bounded measurement candidate.
Continued rise before the prior timeout does not prove60 is a physical ceiling
or that more time will achieve250. **physical test NOT RUN.**

| Review item | Location |
| --- | --- |
| Current candidate and Git workflow | [README.md](README.md) |
| Longer action/thermal confirmation; COM5 / Target250 / maximum60 s / one script START | [Static test](Docs/AUTO_TARGET_STATIC_TEST.md) |
| Actual tests/builds and field-evidence limits | [Test results](Docs/CONVERGENCE_MEASURE1_TEST_RESULTS.md) |
| Historical ConvergenceMeasure1 Release HEX | [SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex) |
| Historical matching Release ELF | [SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.elf](output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.elf) |
| Historical AUTO firmware SHA256 | [Firmware/SHA256SUMS.txt](Firmware/SHA256SUMS.txt) |
| All tracked-file hashes except itself | [SHA256SUMS.txt](SHA256SUMS.txt) |
| Single macro change | [auto_target_config.h](Application/auto_target_config.h) |
| Host timing and safety regressions | [test_auto_target.c](Tests/Host/test_auto_target.c) |
| Historical AUTO hash/ELF regression check | [verify_current_auto_target_firmware.py](tools/verify_current_auto_target_firmware.py) |
| Existing capture path and updated report | [capture_auto_target_static.ps1](tools/capture_auto_target_static.ps1) |

HEX_SHA256=BB5486FB8318045416CE0C65668118CAA556F675AB7C6F403D1ED7F8192B780B
ELF_SHA256=7E09AB5463A92158459739682B8DE1DE2E257A955347A1EC5DCDC5CEACEC2D2D

At this release, main tracked one current pair. Historical firmware, tests and
engineering records remain available. Generated archives have since been removed.
The named field CSV/report were not found in the workspace;
the supplied observations are recorded without inventing files, RAM/current values.
No history was rewritten and no hardware connection, flashing or START occurred.

Local verification index: `output/ConvergenceMeasure1/test_execution.json`,
`source_audit.json`, `firmware_identity.json` and `firmware_verification.log`.
The tracked test record contains actual outcomes and log hashes. Raw generated
logs remain local, not in Git/ZIP.

This historical release used a source ZIP. The current workflow delivers only
the pushed Git commit. All host pressure input is SYNTHETIC_INPUT.

Role: historical AUTO engineering evidence and regression fixture. It does not select the current field image. Current and historical/test-fixture entry points are separated in [the repository index](Docs/REPOSITORY_INDEX.md).
