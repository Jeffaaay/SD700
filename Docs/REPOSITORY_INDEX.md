# Repository entry points and file roles

GitHub main is the source of truth. This index separates current use from historical evidence and regression fixtures; a retained historical image is not a current field recommendation.

| Role | Entry point | Meaning |
|---|---|---|
| Current candidate | [README](../README.md), [CoolingAnchorFix1 handoff](BuildToTarget2_CoolingAnchorFix1/HANDOFF.md) | BuildToTarget2_CoolingAnchorFix1; F10C/46530110/profile7; PHYSICAL_STATUS=NOT_RUN |
| Current firmware authority | [ForceServo1 SHA256 manifest](../Firmware/ForceServo1.SHA256SUMS.txt), [strict verifier](../tools/verify_force_servo_firmware.py) | Exactly one current HEX/ELF pair; verifier checks the actual image, configuration, output guard and hashes |
| Current software evidence | [CoolingAnchorFix1 tests](BuildToTarget2_CoolingAnchorFix1/TEST_RESULTS.md) | Real host/build results; synthetic force inputs do not prove physical performance |
| File-only cleanup | [RepositoryCleanup1 record](RepositoryCleanup1/README.md) | Separate commit after A; exact dry-run/apply lists, preservation proof and byte-identical rebuild |
| Historical engineering/evidence | Prior candidate folders under Docs, [legacy AUTO index](../REVIEW_BUNDLE.md), [legacy control map](../OLD_TO_NEW_CONTROL_MAP.md) | Their identities, measurements, failures and limits describe their own versions |
| Hardware/old source references | [Reference](../Reference), [INA240](../Reference/Hardware/ina240.pdf), [old-source provenance](BuildToTarget2_Interpulse1/SOURCE_REFERENCE.json) | Preserved originals; reference code is not an executor bypass or field control API |
| Test fixtures and regression tools | [Tests/Host](../Tests/Host), [tools](../tools), prior images below | Retain all dependencies. Older firmware is intentionally used for identity/configuration rejection and equality tests; synthetic CSV and fake serial frames are not field evidence |
| Local generated outputs | output outside retained firmware/evidence | Rebuildable objects/caches/test executables may be removed only by the reviewed cleanup policy. Logs, CSV/metadata/report, unique firmware, final runs and failed-run evidence are preserved |

The original32 tracked HEX/ELF files keep all original paths and hashes. Together with the current pair there are34 files; [commit A preservation inventory](RepositoryCleanup1/commit_a_firmware.json) pins all34. Historical firmware entries:

| Historical candidate | Original paths retained |
|---|---|
| AutoTarget | [Preserved HEX/ELF](../output/AutoTarget/firmware) |
| BuildToTarget1 | [Preserved HEX/ELF](../output/BuildToTarget1/firmware) |
| BuildToTarget2 | [Preserved HEX/ELF](../output/BuildToTarget2/firmware) |
| BuildToTarget2_Interpulse1 | [Preserved HEX/ELF](../output/BuildToTarget2_Interpulse1/firmware) |
| BuildToTarget2_StartAnchorFix1 | [Preserved HEX/ELF](../output/BuildToTarget2_StartAnchorFix1/firmware) |
| CommissioningUnlock1 | [Preserved HEX/ELF](../output/CommissioningUnlock1/firmware) |
| ForceServo1 | [Preserved HEX/ELF](../output/ForceServo1/firmware) |
| StaticForce3000_1 | [Preserved HEX/ELF](../output/StaticForce3000_1/firmware) |
| StaticForce3000_Boost1 | [Preserved HEX/ELF](../output/StaticForce3000_Boost1/firmware) |
| StaticForceAuthority2 | [Preserved HEX/ELF](../output/StaticForceAuthority2/firmware) |
| StaticForceAuthority2_ReviewFix | [Preserved HEX/ELF](../output/StaticForceAuthority2_ReviewFix/firmware) |
| StaticForceRuntimeCharacterization1 | [Preserved HEX/ELF](../output/StaticForceRuntimeCharacterization1/firmware) |
| StaticForceRuntimeCharacterization2 | [Preserved HEX/ELF](../output/StaticForceRuntimeCharacterization2/firmware) |
| Target250Authority1 | [Preserved HEX/ELF](../output/Target250Authority1/firmware) |
| Target250Continuous2 | [Preserved HEX/ELF](../output/Target250Continuous2/firmware) |
| Target250MVP1 | [Preserved HEX/ELF](../output/Target250MVP1/firmware) |

The root SHA256SUMS.txt covers every tracked file except itself. Firmware/SHA256SUMS.txt continues to pin the historical AUTO candidate; Firmware/ForceServo1.SHA256SUMS.txt selects the current ForceServo candidate. Historical raw evidence is never rewritten to match current schema or firmware. Existing field CSV/metadata/report remain in their original locations. No ZIP delivery, serial connection or physical test is part of this cleanup.
