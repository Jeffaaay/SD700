# BuildToTarget2 handoff

PHYSICAL_STATUS=NOT_RUN. Baseline1a4c93d57d386e25efa300a64828e2fef5a67071; current commit is the checkout commit (`git rev-parse HEAD`), published and compared to live origin/main after software verification. No ZIP, serial connection or flashing by the agent.

Current firmware identity F10B /4653010D / profile7, immutable Build version2 / digest1817994819. TargetForceN1..3000 only; gains10/0/0; reserved legacy A/C=0. One script START, no overall normal capture/run deadline, first valid target reach truly OFF with continued monitoring for all targets.

HEX: `output/BuildToTarget2/firmware/SD700_ForceServo1_BuildToTarget2_RealBench_Release.hex`

SHA256: `248E4D0149AD4DEC418EBE6D7E1EF7768EEF2CB9B36F02DFCBC2BAA39958B141`

ELF: `output/BuildToTarget2/firmware/SD700_ForceServo1_BuildToTarget2_RealBench_Release.elf`

SHA256: `D4D704893E9188F16F29D43B64A423B7B5E4B4079AB49EC1414C792CF4E78740`

See [current README](../../README.md) for the single field procedure and actual stage limits; [control map](../../OLD_TO_NEW_CONTROL_MAP.md) for exact old source and safety adaptations; [test results](TEST_RESULTS.md) for actual commands/results. PSU0.5 A unchanged; finite exposures are experimental software constraints, not thermal/continuous ratings. Hardware stop, static force and rotating load remain unvalidated. Historical field pairs and evidence are unchanged.
