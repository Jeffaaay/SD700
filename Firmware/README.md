# Current firmware: Target250Authority1

SHORT supervised experiment only; **FIELD_STATUS=NOT_RUN**. Not a production-safe
or continuous rated profile. See [current README](../README.md) for the single
COM5/Target250/unchanged0.5 A session, maximum5 seconds, one scripted START.

[HEX](../output/Target250Authority1/firmware/SD700_ForceServo1_Target250Authority1_RealBench_Release.hex)

SHA256: `322537355B059AA080B5C2356452D43C1C685E30889640D87E01CF5213037A48`

[ELF](../output/Target250Authority1/firmware/SD700_ForceServo1_Target250Authority1_RealBench_Release.elf)

SHA256: `DB6D3364EF0BFC1DF05A6F0E1065EF052684E0DBE63EE8B117C54015614D1E06`

Verify with `python tools/verify_force_servo_firmware.py`. Pure Python checks
exact hashes, ELF/HEX structure and30432 addressed load bytes, F103/build46530105,
24 exact defaults and16 contract words. Kp10/Ki0/Kd0, PRESS720/RELEASE100, selected
ceilings720/100 and experimental-readiness1 are required, with the existing output
arming gate. Reference200/accel1000/slew1000, age20/gap125/lease130, max275/raw325,
build30000/session45000/saturation5000 are retained. Default ForceServo stays LOCKED.
No verifier check was skipped. Optional objcopy is developer-only.

The [current manifest](ForceServo1.SHA256SUMS.txt) names this pair. The previous
[Continuous2 manifest](Target250Continuous2.SHA256SUMS.txt),
[MVP1 manifest](Target250MVP1.SHA256SUMS.txt),
[CommissioningUnlock1 manifest](CommissioningUnlock1.SHA256SUMS.txt),
[locked manifest](ForceServo1_Locked.SHA256SUMS.txt) and [AutoTarget manifest](SHA256SUMS.txt)
retain their original bytes. GitHub main is source of truth; no ZIP.
Physical STOP, force response, current, temperature and continuous rating are not validated.
