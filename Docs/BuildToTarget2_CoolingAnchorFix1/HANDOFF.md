# BuildToTarget2_CoolingAnchorFix1 - commit A handoff

PHYSICAL_STATUS=NOT_RUN. Based on main8e2973a5a6b9869d14cfc63569989dc8cf1d9ce8. Only failed-admission cooling accounting and firmware identity change. A preload-to-pulse transition can consume prepaid time and clear preload_active before budget/timer/HW rejection. Its failure path now disables output, refreshes the clock, and anchors cooling only on confirmed OFF register readback, independently of the accounting flags. Failed hardware-disable readback cannot start cooling. Repeated STOP does not clear budgets or restart the cooldown.

Reproduction:3000 command, normal10/hard11 ms,300 preload,30-ms cooldown then valid post feedback, repeated to reserved12000. Old rest_remaining=107970 at true OFF; fixed=108000. Timer/HW failure and STOP during transition are also covered, including an injected3-ms delay before shutdown.

Identity F10C /46530110 /profile7. Schema/config layout unchanged:38-u32 version3, digest2848875769. All pulse/boost/preload parameters,8 s/12 s/108 s/5 s thresholds, original START/lease/STOP/sensor/overforce protections and target reach OFF are unchanged. [Protocol inherited from Interpulse1](../BuildToTarget2_Interpulse1/PROTOCOL.md); only current build ID/hash differ. [Current schema](schema.json).

HEX: `output/BuildToTarget2_CoolingAnchorFix1/firmware/SD700_ForceServo1_BuildToTarget2_CoolingAnchorFix1_RealBench_Release.hex`
SHA256: `11D2525F2FE0F206453D09CEF4DD483C29CBD1812E7097BA53B6854FB48F25FF`

ELF: `output/BuildToTarget2_CoolingAnchorFix1/firmware/SD700_ForceServo1_BuildToTarget2_CoolingAnchorFix1_RealBench_Release.elf`
SHA256: `6082D8EB0DE290040961EED0F0E388EE76536D35834BCAF62827D940F41B8507`

[Actual tests](TEST_RESULTS.md), [README](../../README.md), [firmware manifest](../../Firmware/ForceServo1.SHA256SUMS.txt). Original32 tracked HEX/ELF paths/hashes are pinned in [preserved_baseline_firmware.json](preserved_baseline_firmware.json) and remain intact. No ZIP, serial, flashing or physical output. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated. File-only cleanup is a subsequent separate commit and must preserve this exact field pair.
