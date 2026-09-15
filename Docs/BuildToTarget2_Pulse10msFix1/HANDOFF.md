# BuildToTarget2_Pulse10msFix1

Based on clean main/origin/main `b1063cad19dc980134256bacae05a0ed1aa49117`; neither requested pulse fix was present at that baseline. Firmware identity F10C /46530111 /profile7, immutable38-u32 Build config version4, digest2927201258. Schema layout is unchanged; FC03 config and frozen telemetry retain the actual command/hard time/post-pulse readback.

The user reports the new firmware stopped around12 N and the old HEX reached250 on the same machine. These are user-provided field facts, not agent measurements. **PHYSICAL_STATUS=NOT_RUN** for this candidate; restoring the reviewed command/timing does not establish the complete physical cause or prove250 N attainment.

## Reviewed binary basis

From the supplied Desktop `SD700-Servo-Press-Controller-master.zip`, member `SD700-Servo-Press-Controller-master/SD700-Servo-Press-Controller-master/Output/press_control_f411.hex`:

- HEX SHA256 `1C2E7F679FD6E1C62DF49B418576510249669CBC23BB2D485EF8E6CAED5F5820`.
- Adjacent AXF SHA256 `C6A9D169697A483A0759829062C010568DF0B0E488062DE887E90825831F5886`.
- Validated old HEX checksum/addresses and ELF32 little-endian ARM executable header/load bounds: all64176 addressed PT_LOAD bytes match, entry0x08000199. Keil AXF scatter-load bytes must be compared at program load addresses; a section-VMA objcopy export is not the comparison basis.
- AXF disassembly: MICRO passes2.0 to PulseBoost at0x0800317A/0x0800317E; FINE passes1.0 at0x08002C96/0x08002C9A. `control_task` loads10 ticks at0x08004EC4 then calls `xTaskDelayUntil` at0x08004EC8. This is the reviewed normal10 ms cadence, not a copy of the old polling stop implementation. No4 V boost or ContactPulse6V/7V variant was used.

## Focused change

| Contract | This candidate |
|---|---|
| MICRO | Base800..3000 unchanged, boost step300 unchanged, additive cap2000, total<=5000 |
| FINE | Base400..1000 and additive cap1000 unchanged, total<=2000 |
| MICRO/FINE high segment | Normal10 ms independent of boost; TIM5 normal compare hands off to preload. Independent hard cutoff11 ms, target/safety can end earlier |
| Request and executor | Both enforce10+1 ms; independently reject compensated short requests and MICRO commands above5000 |
| Preserved | APPROACH,300 initial preload/drop strictly>2 N adds100/cap600, >=30 ms cooldown with fresh post feedback, receive-anchored lease, STOP/overforce/fault, target truly OFF, no automatic repress |
| Preserved budgets | Approach8000 ms and40M command-ms; total pulse reservation plus preload12000 ms; no-response5000 ms; cooling108000 ms from actual verified OFF. START/STOP do not clear ledgers |

ELF comparison with CoolingAnchorFix1 confirms exactly five changed Build words: version3->4, boost max4000->2000, ceiling7000->5000, hard min3->11, normal min2->10. Every other Build word and all default PID/profile/catalog/characterization/legacy-machine config, hardware arming and continuous-owner denial bytes are unchanged. Gains10/0/0 and PSU0.5 A remain unchanged; command is not measured current or a motor rating.

## Actual validation

[Saved actual O2 trace and affected-group output](TRACE.txt). Trace traverses production request/executor/HW mapping and TIM5 IRQ shim without task polling. Baseline MICRO3000->7000 shortened10->4 ms; fixed3000->5000 remains10 ms, then preload300. FINE base700/+1000 retains amplitude and now stays10 ms. Invalid short contracts,30 ms/fresh gating, taper, target/STOP/fault/lease OFF, preload droop, hard deadline without main polling, noise/no-response/exposure budgets and true-OFF108000 ms cooling pass. Actual immutable FC03 config/digest readback also passes.

Commands run (all host invocations O2 only):

```powershell
python tools/run_force_servo_tests.py --commissioning --characterization --build-to-target --optimization O2 --test-argument=--pulse-before --output output/BuildToTarget2_Pulse10msFix1/before
python tools/run_force_servo_tests.py --commissioning --characterization --build-to-target --optimization O2 --test-argument=--pulse-fix --output output/BuildToTarget2_Pulse10msFix1/after
python tools/run_force_servo_tests.py --commissioning --characterization --build-to-target --optimization O2 --test-argument=--pulse-fix --output output/BuildToTarget2_Pulse10msFix1/final-host
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -BuildToTarget2 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/BuildToTarget2_Pulse10msFix1/arm
python tools/verify_force_servo_firmware.py
```

All five commands returned0. Baseline reproducer PASS; initial fix trace+11 groups PASS; final fix trace+14 groups PASS after adding persistent noise/budget, strict-droop and config-readback checks. One ARM Release build PASS (19.047 s); one current official strict verifier PASS, comparing49748 addressed HEX/ELF load bytes plus exact hashes/identity/config/guard/owner denial. No assertions were removed to obtain PASS.

NOT_RUN: O0/Os matrix, full historical regression, unrelated capture/schema/report suites, full verifier rejection-test suite and optional objcopy cross-check. Current identity expectations in those retained tests were synchronized, but are not claimed tested this turn. No cleanup, ZIP, serial connection, flash or physical machine test. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated.

## Current firmware

[HEX](../../output/BuildToTarget2_Pulse10msFix1/firmware/SD700_ForceServo1_BuildToTarget2_Pulse10msFix1_RealBench_Release.hex) SHA256 `86C6205D036C0212F1B7D2DD56BA08CF3BB8D9E20072D9DA9A64100193AFAD96`

[ELF](../../output/BuildToTarget2_Pulse10msFix1/firmware/SD700_ForceServo1_BuildToTarget2_Pulse10msFix1_RealBench_Release.elf) SHA256 `8819B57A5BAEC0091ABB171E560386F64D45AA6256E5967FBC1E824D153F0EC2`

[Current manifest](../../Firmware/ForceServo1.SHA256SUMS.txt). The prior current manifest is preserved as Firmware/BuildToTarget2_CoolingAnchorFix1.SHA256SUMS.txt. All34 existing tracked firmware files and field originals remain in place. Only Target250 is the requested follow-up field objective; no other force-range tuning or physical success is claimed.
