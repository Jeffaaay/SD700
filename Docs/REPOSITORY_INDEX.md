# Repository entry points and file roles

GitHub main is the source of truth. This index separates current use from historical evidence and regression fixtures; a retained historical image is not a current field recommendation.

| Role | Entry point | Meaning |
|---|---|---|
| Current candidate | [README](../README.md), [SourcePort1 handoff](BuildToTarget3_SourcePort1/HANDOFF.md) | BuildToTarget3_SourcePort1; F10D/46530113/profile7; PHYSICAL_STATUS=NOT_RUN |
| Current firmware authority | [ForceServo1 SHA256 manifest](../Firmware/ForceServo1.SHA256SUMS.txt), [strict verifier](../tools/verify_force_servo_firmware.py) | Exactly one current HEX/ELF pair; verifier checks the actual image, configuration, output guard and hashes |
| Current software evidence | [SourcePort1 focused tests](BuildToTarget3_SourcePort1/HANDOFF.md) | Real host/build results; synthetic force inputs do not prove physical performance |
| Current ignored-output cleanup | [GeneratedArtifactsCleanup1](GeneratedArtifactsCleanup1.md), [script](../tools/clean_generated_artifacts.py) | Default dry-run; exact ignored-only apply, capture/fixture protection and unchanged firmware |
| Historical conservative cleanup | [RepositoryCleanup1 record](RepositoryCleanup1/README.md) | Earlier separate commit after A; original inventory and results remain historical |
| Historical engineering/evidence | Prior candidate folders under Docs, [legacy AUTO index](../REVIEW_BUNDLE.md), [legacy control map](../OLD_TO_NEW_CONTROL_MAP.md) | Their identities, measurements, failures and limits describe their own versions |
| Hardware/old source references | [Reference](../Reference), [INA240](../Reference/Hardware/ina240.pdf), [old-source provenance](BuildToTarget2_Interpulse1/SOURCE_REFERENCE.json) | Preserved originals; reference code is not an executor bypass or field control API |
| Test fixtures and regression tools | [Tests/Host](../Tests/Host), [tools](../tools), prior images below | Retain all dependencies. Older firmware is intentionally used for identity/configuration rejection and equality tests; synthetic CSV and fake serial frames are not field evidence |
| Local generated outputs | output outside retained firmware/evidence | Ignored ARM/host builds, raw software logs, final/repro/focused runs and synthetic captures can be regenerated. Every tracked file, physical/uncertain capture and explicit test/tool input remains protected |

All34 pre-existing tracked HEX/ELF files keep their paths and hashes; [prior preservation inventory](RepositoryCleanup1/commit_a_firmware.json) pins those34. Pulse10msFix1 and RiseDiagnostic1 added four files. SourcePort1 adds the current pair; all38 predecessors remain unchanged, total40. Historical firmware entries:

| Historical candidate | Original paths retained |
|---|---|
| AutoTarget | [Preserved HEX/ELF](../output/AutoTarget/firmware) |
| BuildToTarget1 | [Preserved HEX/ELF](../output/BuildToTarget1/firmware) |
| BuildToTarget2 | [Preserved HEX/ELF](../output/BuildToTarget2/firmware) |
| BuildToTarget2_RiseDiagnostic1 | [Preserved HEX/ELF](../output/BuildToTarget2_RiseDiagnostic1/firmware) |
| BuildToTarget2_Pulse10msFix1 | [Preserved HEX/ELF](../output/BuildToTarget2_Pulse10msFix1/firmware) |
| BuildToTarget2_CoolingAnchorFix1 | [Preserved HEX/ELF](../output/BuildToTarget2_CoolingAnchorFix1/firmware) |
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

Historical manifests may name raw ignored software logs that were later removed by GeneratedArtifactsCleanup1. Those records describe the original runs; tracked reports, hashes and physical evidence are retained. The earlier policy retaining all final/repro output is superseded for ignored software artifacts only.
