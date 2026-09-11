# Firmware identity

Current field candidate: **PressBoost1 / FieldReady1**.

The only Git-tracked HEX is
`../output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.hex`.
Its SHA256 is
`26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2`.
The matching ELF is retained beside it. This directory's
[SHA256SUMS.txt](SHA256SUMS.txt) identifies both files; paths are relative to
this directory. Both firmware files are unchanged by the Git setup.

Target 250 / AUTO_HOLD real PressBoost1 validation is pending. Follow
[the current static test instructions](../Docs/AUTO_TARGET_STATIC_TEST.md).

Any `SD700_GuardFixV5_RealBench_Release.*` files in the development workspace
are local history and are excluded from Git. Historical hashes remain locally in
`../output/AutoTarget/baseline/Firmware_SHA256SUMS_V5.txt`.
V5 HEX SHA256 `1D5AADF5C957B97654F7B3305813D47594B8C42C3373EA1182B6357559241C04`
does not identify the new control firmware.
