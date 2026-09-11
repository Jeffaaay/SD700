# Current firmware identity

Current field candidate: **PressBoostRetain1**, retaining far-band boost after
a qualified effective rise, with ApproachMeasure1 search preserved. The only Git-tracked HEX is:

`../output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.hex`

SHA256: `348F4ED990742346F4C56EED9CDB9C9311CA48AEAC9B29639A660B00ECEB584F`

Matching ELF: `../output/AutoTarget/firmware/SD700_AutoTarget_PressBoostRetain1_RealBench_Release.elf`

ELF SHA256: `E3C5A08788D955881E691600209A4E083A83FB564BC7EFC2C59328F38EF4B022`

Both files were produced by this task's actual RealBench AutoTarget Release
build. [SHA256SUMS.txt](SHA256SUMS.txt) paths are relative to this directory.
Prior ApproachMeasure1/PressBoost1/FieldReady1 firmware remains in Git history and may exist
locally as ignored evidence. It is not this candidate.

See [the full static test](../Docs/AUTO_TARGET_STATIC_TEST.md). Contact timing,
Target 250 / AUTO_HOLD and mechanical safety remain unvalidated in hardware.
**physical test NOT RUN.**
