# Current firmware: ForceServo1, physical output LOCKED

[Current HEX](../output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.hex)

SHA256: `C76059E0FAA126D5E52A6D640599A007E71D669080C022FEEE6180D3AB414B41`

[Matching ELF](../output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.elf)

ELF SHA256: `B6AEFBCAD93DE82A3514C134518A249A6D6851D3E8C4E8DF3EE7C41DC0984EFF`

Verify from the repository root with `python tools/verify_force_servo_firmware.py`.
The new [ForceServo1 manifest](ForceServo1.SHA256SUMS.txt) requires the exact pair,
ELF parameter/contract values, linked controller/executor/TIM5 code, compiled
false physical-arming function and byte-identical HEX regenerated from the ELF.

**physical test NOT RUN; COMMISSIONING_NOT_TUNED.** No target unlock exists in
this release. Follow only the [current README](../README.md) and its one next
power-disconnected timing observation. Continuous stop/enable/deadtime/timing,
static250 and rotating performance remain NOT_VALIDATED.

The historical [AutoTarget manifest](SHA256SUMS.txt), its strict verifier and
ConvergenceMeasure1 HEX/ELF remain unchanged. They are separate historical
artifacts, not an alternate field test for ForceServo.
