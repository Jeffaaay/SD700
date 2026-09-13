# Current firmware: Target250MVP1

[HEX](../output/Target250MVP1/firmware/SD700_ForceServo1_Target250MVP1_RealBench_Release.hex)

SHA256: `4277556949A4AD6A2D5D24E49F8A1DEA97C7D8906CA48AA9878D68B5DF924055`

[ELF](../output/Target250MVP1/firmware/SD700_ForceServo1_Target250MVP1_RealBench_Release.elf)

SHA256: `06B8281B8D089D97D5639C6176A94F962AB5297FE04EEAE9CC32AF5B5DB31B01`

Verify from the repository root: `python tools/verify_force_servo_firmware.py`.
The exact pair is pinned by [ForceServo1.SHA256SUMS.txt](ForceServo1.SHA256SUMS.txt).
SchemaF102/build46530103, Target250, OFF pending wait250 ms, active gap125 ms,
lease130 ms, age20 ms, exact P-first defaults,100 mV caps, target max275/raw abort325,
build30000 ms and total45000 ms defaults are checked in the actual ELF.30068
addressed load bytes match the HEX. The existing hardware gate/guard and linked
continuous executor/TIM5 symbols are required. Strict checks use Python's standard
library; optional `--objcopy-cross-check` additionally requires ARM objcopy.

The previous [CommissioningUnlock1 manifest](CommissioningUnlock1.SHA256SUMS.txt),
[locked ForceServo manifest](ForceServo1_Locked.SHA256SUMS.txt), historical
[AutoTarget manifest](SHA256SUMS.txt) and their original files remain intact.
They are historical candidates, not alternate current field instructions.

GitHub main is the only source of truth; no ZIP. Use the
[current README's one supervised Target250 session](../README.md), with the
physical0.5 A current limit unchanged. The100 mV command cap is not a verified
continuous-duty rating. **FIELD_STATUS=NOT_RUN.** Software PASS cannot certify
physical output, STOP, Target250 response, thermal duty or HOLD stability.
