# Current firmware: ForceServo1 CommissioningUnlock1

[RealBench Release HEX](../output/CommissioningUnlock1/firmware/SD700_ForceServo1_CommissioningUnlock1_RealBench_Release.hex)

SHA256: `F96936FF7962ADC65737C0C6C97F5EBC49F30E8A3B33A20B75C450E3657DD46C`

[Matching ELF](../output/CommissioningUnlock1/firmware/SD700_ForceServo1_CommissioningUnlock1_RealBench_Release.elf)

SHA256: `0925C1CDAEEDB7DCBFCD08EC0BF5E37505ABAA2175B2B11A17112D747CB86D41`

GitHub main is the source of truth. Update with `git pull --ff-only`, record
`git rev-parse HEAD`, then run `python tools/verify_force_servo_firmware.py` from
the repository root. No ZIP or package-verification step is used.

[Current manifest](ForceServo1.SHA256SUMS.txt) pins this exact pair. The verifier
checks schemaF101/build46530102, explicit commissioning arming, both100 mV command
caps, full defaults, target275/raw abort325, 30000 ms build /45000 ms session
defaults, live controller/executor/TIM5/guard symbols and addressed HEX/ELF load
equality (29508 bytes). The unchanged legacy machine configuration is also
checked; its approach constants do not grant a pulse/run entry in commissioning.
Strict ELF32/ARM bounds and Intel HEX checksums use only Python's standard
library. `--objcopy-cross-check` optionally adds the ARM developer tool check.

The command cap uses the former lower release default100, not a measured safe
continuous rating. Sample age20 ms, feedback gap40 ms, lease50 ms and reversal
OFF time2 ms keep the existing defaults. Actual electrical stop timing, continuous
duty and physical behavior have not been tested. Follow only the current
[one-session A-E procedure](../README.md). Keep the existing current limit;
do not test Target250, 500 or 3500 N.

**physical test NOT RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD:
NOT_VALIDATED.**

[Historical locked ForceServo1 hashes](ForceServo1_Locked.SHA256SUMS.txt) and
the original locked HEX/ELF are preserved byte-for-byte. The historical
[AutoTarget manifest](SHA256SUMS.txt), its strict verifier and ConvergenceMeasure1
HEX/ELF remain unchanged. These are historical artifacts, not alternate current
field procedures. Plain ForceServo builds remain physically locked; the current
pair was built with the explicit `-CommissioningUnlock1` switch and RealBench
acknowledgement. Firmware checks do not read back MCU flash.
