# SD700 — ForceServo1 CaptureFix1 (physical output LOCKED)

Current candidate: **ForceServo1 CaptureFix1**, a capture/verifier tool repair on
reviewed ForceServo1 `29d0a9091b8333c690bcb41f5474dffc3909ef61`. HEAD matched and
the worktree was clean. Production firmware source and both firmware files are
unchanged; no ARM rebuild was performed. Previous evidence and ZIPs are retained.

Shared serial framing now supports FC03 parameter replies. Tests exercise the
production length reader and the complete no-hardware Observe flow, including
failure cleanup. The field verifier checks exact hashes, addressed ELF/HEX load
bytes, identity, configuration and the compiled lock using Python's standard
library; **no ARM toolchain is required for field verification or Observe**.

**physical test NOT RUN. COMMISSIONING_NOT_TUNED.** This firmware cannot enable
motor output, including approach. Continuous-duty ratings, sensor/control timing,
reversal deadtime and the new hardware stop path still require validation.
Do not use the former AutoTarget static250 instructions for this candidate.

[RealBench locked Release HEX](output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.hex)

HEX SHA256: `C76059E0FAA126D5E52A6D640599A007E71D669080C022FEEE6180D3AB414B41`

[Matching ELF](output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.elf)

ELF SHA256: `B6AEFBCAD93DE82A3514C134518A249A6D6851D3E8C4E8DF3EE7C41DC0984EFF`

ForceServo1 implements bounded approach, force reference trajectory, one PID
through BUILD/active HOLD, supervised continuously updated PWM commands and a
receive-anchored TIM5 lease. It adds atomic RAM parameter groups and frozen
versioned Modbus diagnostics. Old base+boost cannot own output in this build.
Target max275, contact20 and raw abort325 remain; units are sensor control units,
**NOT calibrated N**. There is no rotating-axis interface or target500 test.
No safe continuous output or gain is inferred from the old 10 ms pulse behavior.

## One next field step

With motor power **physically disconnected** and MCU/pressure sensor correctly
powered, confirm the identified locked firmware and observe pressure
receive/telemetry timing on COM5 for at most 60 seconds.
**ZERO START** in this step. Do not add manual START or use ApproachOnly.
Record the retained actual current limit and initial gap in the two text fields
below. Do not change the current limit or try pressure 250/500. Keep originals
and return CSV, report, metadata and one brief field note; no automatic repeat.
Observation does not validate continuous-mode hardware stopping or authorize
unlocking the output.

```powershell
python tools/verify_force_servo_firmware.py
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode Observe -Port COM5 -MaximumSeconds 60 `
  -ConfirmMotorPowerDisconnected `
  -ConfirmedFirmwareSha256 C76059E0FAA126D5E52A6D640599A007E71D669080C022FEEE6180D3AB414B41 `
  -CurrentLimitSetting '填写原有限流实值；保持不变' -InitialGap '填写初始间隙' `
  -FieldNotes '电机动力已物理断开；记录现场情况' `
  -OutputCsv "captures/force-servo1-capturefix1-observe-$stamp.csv"
```

Operator hash confirmation attests which image was flashed; it is not an MCU
flash readback verification. No current/temperature instrumentation means
NOT_MEASURED. A later powered trial requires approved load/travel/cumulative
motion/thermal exposure, retained current limit, supervision and external E-stop.
The output lock has no tuning-register bypass in this release.

## Review and reproduce

- [Design, timing, safety coverage and known blockers](Docs/ForceServo1/DESIGN.md)
- [RAM parameters and versioned protocol](Docs/ForceServo1/PARAMETERS_AND_PROTOCOL.md)
- [One field step, hardware qualification list and tuning order](Docs/ForceServo1/TUNING_AND_FIELD.md)
- [CaptureFix1 actual verification and limitations](Docs/ForceServo1_CaptureFix1/TEST_RESULTS.md)
- [Historical ForceServo1 verification](Docs/ForceServo1/TEST_RESULTS.md)
- [Default parameter JSON — numerical seeds only](Docs/ForceServo1/default_parameters.json)
- [Current firmware manifest](Firmware/ForceServo1.SHA256SUMS.txt)

```powershell
# Software-only checks; use a new output path to retain prior evidence.
$checkStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
python tools/verify_capturefix1.py --output "output/ForceServo1_CaptureFix1/check-$checkStamp"
python tools/verify_force_servo_firmware.py
# Optional developer cross-check, requires ARM objcopy:
# python tools/verify_force_servo_firmware.py --objcopy-cross-check
```

PowerShell and Python 3.9+ are required for capture; GCC is additionally required
for the host regression runner. No serial port is opened by these software
checks. CaptureFix1 does not change ForceServo PID, trajectory, executor, TIM5,
HOLD, limits, deadlines or output lock. Existing safety assertions remain.

Current delivery: `output/SD700_ForceServo1_CaptureFix1_SourceOfTruth.zip`, with
an adjacent `.zip.sha256`. Its `SOURCE_OF_TRUTH.json` identifies the committed
source; `ForceServo1_CaptureFix1_PACKAGE_SHA256SUMS.txt` hashes every entry.
After extracting, run `python tools/verify_force_servo_capturefix1_package.py .`
from the extracted `SD700` directory before the field command. This offline
package check also requires only Python and does not open a serial port.

Historical entry point: [ConvergenceMeasure1 README archive](Docs/ForceServo1/ConvergenceMeasure1_README_ARCHIVE.md).
Its links/commands refer to the repository root and its **previous** candidate.
The original AutoTarget HEX/ELF and `Firmware/SHA256SUMS.txt` are preserved;
`verify_current_auto_target_firmware.py` still strictly verifies that old pair.
New verification uses the ForceServo-specific manifest and verifier.

Software PASS does not mean static250, continuous output, STOP hardware,
rotating-load performance or target500 has passed. **physical test NOT RUN**.
