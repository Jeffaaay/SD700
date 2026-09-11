# Current firmware identity

Current field candidate: **ApproachMeasure1**, with existing PressBoost1 control
after contact. The only Git-tracked HEX is:

`../output/AutoTarget/firmware/SD700_AutoTarget_ApproachMeasure1_RealBench_Release.hex`

SHA256: `8F537F2EF2FFA622BB92F1A6ED2C38972B57627337EC6B905C8283734826D56F`

Matching ELF: `../output/AutoTarget/firmware/SD700_AutoTarget_ApproachMeasure1_RealBench_Release.elf`

ELF SHA256: `D49382BB519261E40A6453ADF18C660A75383B287F833741995D773FDCC8D546`

Both files were produced by this task's actual RealBench AutoTarget Release
build. [SHA256SUMS.txt](SHA256SUMS.txt) paths are relative to this directory.
Prior PressBoost1/FieldReady1 firmware remains in Git history and may exist
locally as ignored evidence. It is not this candidate.

See [the full static test](../Docs/AUTO_TARGET_STATIC_TEST.md). Contact timing,
Target 250 / AUTO_HOLD and mechanical safety remain unvalidated in hardware.
