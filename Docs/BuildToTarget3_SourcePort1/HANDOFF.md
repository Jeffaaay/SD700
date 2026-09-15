# BuildToTarget3_SourcePort1

Baseline: clean main/origin/main fa6fd0036ea877e4aee7d65e14f12c094b6ca478. Identity F10D/46530113/profile7; Build config version5,45 u32 fields/90 registers, digest4163791885; diagnostic133 u32 +33 float/332 registers.

Only algorithm reference: the actual working-tree member Drivers/BSP/PRESSURE_CONTROL/pressure_control.c in the newly supplied field archive (locally Downloads/SD700-Servo-Press-Controller-master.zip). SHA256 E6E08DC4E370A853EA9A586D765D97D5B6A1E7D6DDC9760202B947F99FB4D512. No old HEAD checkout, snapshot, README algorithm or previous HEX ceiling was used. Original C/header are preserved in Tests/Host/Reference/Field182402. The host oracle compiles the complete original C; its GB18030 header is transcoded only for host compiler comments.

13 source functions were directly transplanted, including mode selection, directional PulseBoost, PrecisionForward, precision latches/escape levels and waiting rules. Platform calls emit executor decisions. Effective2..8 N far progress retains output; precision1..2 N retains its effective setting. Source large-rise reduction/glitch/settling behavior is preserved rather than replaced by the removed25 N Fault.

|250 N stage|Actual selected output|
|---|---|
|Before contact10 N|Continuous5000 command, receive-anchored lease, unchanged finite8 s approach budget|
|Far, error>50 N|Source base+directional boost, final10000 command/12 ms; default9 ms, grows with response|
|40..50 N remaining|Source near limits gradually unlock to7000/8 ms|
|Precision, <=40 N or source latch|PrecisionForward starts2500/2 ms and changes per source; ultimate250 N envelope7000/8 ms|
|After pulse|BRAKE: both drivers enabled, TIM2/TIM3 compares0; no300 forward preload|
|Next pulse|Source30 ms or final400 ms from actual TIM5 end plus fresh feedback, no second settle delay|
|First valid Target250 reach|Immediate true bridge OFF, decay monitoring; no HOLD, auto-repress or RELEASE|

The global12000 command envelope supports the copied source's other branches; it is not the250 N far limit. Only250 N was accepted in this delivery. Command equivalents/PWM are not measured voltage/current or continuous/thermal ratings. PSU0.5 A unchanged.

Safety conflict resolved explicitly: baseline still had a12 s cumulative admission limit; it is now disabled (total_on_ms=0), with exposure still diagnostic. The old5 s no-progress Fault would truncate the source's final400 ms ladder. Persistent net-new-high>=2 N no-response limit is now45000 ms:71*(12+400+125+10)+5000=43837 ms, rounded to45 s, covering the maximum70 no-rise escalation responses plus initial pulse and terminal observation. START/STOP do not clear it. Tests reach level32/escape26 and7000/8 ms before the deadline; a true platform still shuts OFF at45 s. The8 s/40M command-ms approach budget is reserved before starting approach, without refund;108 s verified OFF resets the safety epoch, never output.

Deliberate port differences: TIM5 normal and independent normal+1 ms hard cutoff replace the old FreeRTOS timer/task/IR2104 driver. Modern valid/ordered input,20 ms age,125 ms gap and130 ms receive lease remain stricter than source500 ms;400 ms stale-frame retry is not admitted. Only new valid frames enter source decisions, so repeating a frame cannot grow stall counters. BRAKE lease renewal cannot extend a high pulse. The source's post-target HOLD/repress/reverse behavior is excluded by first-valid-target OFF; no PID control or old manual/UI tasks are enabled.

Actual commands (one host optimization only; no historical suite):

```powershell
python tools/run_force_servo_tests.py --commissioning --characterization --build-to-target --source-port --optimization O2 --output output/BuildToTarget3_SourcePort1/host-accepted
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -BuildToTarget2 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/BuildToTarget3_SourcePort1/arm
python tools/verify_force_servo_firmware.py
```

All three exit0. [Actual O2 output](O2_RESULTS.txt): complete original-source versus production-chain command/BRAKE waveform compared every1 ms;0 N approach/contact, far gain retention/fall,205/215/235/245/249 N stages, full precision escalation, variable widths, fresh/cooldown gates,45 s no-response, retained START/STOP budget, bad/order/stale input, STOP in pulse/BRAKE, raw3000 overforce, independent lease/hard cutoff, approach cutoff, invalid executor contracts, actual FC03 Build readback. Earlier O2 development mismatches were fixed (extra contact wait, diagnostic ceiling, BRAKE software deadline); executed-run failure logs remain under output. One ARM Release PASS, one strict verifier PASS including59284 addressed load bytes and exact identity/config/guard/owner denial/hashes. Focused schema and PowerShell syntax checks PASS.

NOT_RUN: O0/Os matrix, historical full regression/capture/report suites, full verifier mutation suite, other-target tuning and physical tests. No cleanup, ZIP, serial connection or flashing.

HEX: output/BuildToTarget3_SourcePort1/firmware/SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release.hex
SHA256 15CD52ECAB58E15B11EE77C23E097BDDC8AD94439C7B6448B2C069504DE8C683

ELF: output/BuildToTarget3_SourcePort1/firmware/SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release.elf
SHA256 93CA20E52825A9D21BF6697E28A6D5091432BA87DD888341724D169A23D6BF42

PHYSICAL_STATUS=NOT_RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated; synthetic PASS is not physical250 N success.
