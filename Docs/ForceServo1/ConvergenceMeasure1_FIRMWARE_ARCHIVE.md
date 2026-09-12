# Current firmware identity

Current field candidate: **ConvergenceMeasure1**, with convergence timeout
30000 ms. PressBoostRetain1 retention and ApproachMeasure1 search are preserved. The only Git-tracked HEX is:

`../output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.hex`

SHA256: `BB5486FB8318045416CE0C65668118CAA556F675AB7C6F403D1ED7F8192B780B`

Matching ELF: `../output/AutoTarget/firmware/SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release.elf`

ELF SHA256: `7E09AB5463A92158459739682B8DE1DE2E257A955347A1EC5DCDC5CEACEC2D2D`

Both files were produced by this task's actual RealBench AutoTarget Release
build. [SHA256SUMS.txt](SHA256SUMS.txt) paths are relative to this directory.
Prior PressBoostRetain1/ApproachMeasure1/PressBoost1/FieldReady1 firmware remains in Git history and may exist
locally as ignored evidence. It is not this candidate.

See [the full static test](../Docs/AUTO_TARGET_STATIC_TEST.md). Contact timing,
Target 250 / AUTO_HOLD and mechanical safety remain unvalidated in hardware.
Verify hashes and all actual ELF configuration values from the repository root:

```powershell
python tools/verify_current_auto_target_firmware.py
```

The verifier explicitly requires30000 ms, Ki0, and the unchanged timing, amplitude
and pressure-safety settings. Historical FieldReady1 packaging assertions remain
unchanged; they are not the current candidate verifier.

**physical test NOT RUN.**
