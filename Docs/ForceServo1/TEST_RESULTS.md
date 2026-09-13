# Actual ForceServo1 software verification

Historical engineering record. Archive delivery and its generated artifacts
have since been retired. Current delivery uses GitHub main as described in
[README](../../README.md); historical ZIP references below are not instructions.

**physical test NOT RUN.** No serial connection, flashing, motor enable or powered
trial was performed. The new continuous stop path has no inherited physical PASS.

Baseline HEAD: `f24d4a62240ec1630e026fd1eeba491f7c35b11f`; clean at entry.
Baseline tests ran before the new production implementation was edited.

| Executed check | Actual result |
| --- | --- |
| Baseline existing host executions | 33 suite + 6 completion-race + 2 AutoTarget = 41 PASS |
| Baseline ARM matrix | 10 Debug/Release builds PASS |
| Final existing host executions | 33 + 6 + 2 = 41 PASS |
| ForceServo production-chain host tests | 15 groups × O0/O2/Os = 45 group executions PASS |
| Final ARM matrix | Original 10 + ForceServo Debug/Release = 12 PASS |
| New capture statistics/schema | 9 synthetic cases PASS |
| New capture wire/decoder | CRC transport/chunks/u32/negative float decode PASS |
| Legacy captures | Both existing self-test scripts PASS |
| Physical lock / direct ownership source checks | Both existing scripts PASS |
| Rejected build combinations | Existing 4 + new 4 PowerShell rejection checks PASS |
| Exact ForceServo HEX/ELF/configuration | PASS; compiled arming function returns false |
| Original ConvergenceMeasure1 hashes/configuration | Original strict verifier PASS, unchanged |
| Existing evidence preservation | 74 original files: SHA256 and modification time unchanged |

New host tests compile the actual runtime, Machine implementation, trajectory/PID,
continuous real executor, PWM hardware code, TIM5 timer code, Modbus semantics and
RTU server. Only MCU registers/HAL, elapsed timer advancement, and physical gates
are simulated. These are not mathematical-function-only tests.

The 15 groups cover trajectory limits/targets; finite parameters/dt and D; Kp/Ki
affecting final PWM; amplitude/slew/interlock output tracking anti-windup; active
HOLD continuity and contact loss; same-direction updates/zero/reversal; lease
renewal/expiry/STOP races; old tokens, age and wrap; invalid/duplicate/reverse
feedback, raw overpressure and hardware failure; contact30s, initial approach,
backstop and total-session deadline; bounded saturation/tracking; atomic config
and frozen snapshots; RTU congestion/STOP; actual Runtime sequence delivery and
lock refusal; four SYNTHETIC plant variants with gain/lag/noise/hold disturbance.
Synthetic outcomes are software exercise evidence, not identification of SD700
mechanics or proof that250 is achievable.

The completion-race source-order check initially failed because it read raw C
text containing the new conditional early STOP pass. Its final version checks
both preprocessed owners: the legacy safety/guard/STOP order is preserved and
ForceServo additionally requires STOP before PID sample handling. Assertions were
expanded rather than removed. The original failed run and its actual log remain
in `output/ForceServo1/regression/`; final passing runs are separate.

ForceServo Release size reported by ARM tools: text30412, data100, bss3808 bytes.
The published ELF includes F101 capability, build ID46530101, max275, abort325,
30000-ms build deadline, +5000/-800 hard executor limits, locked flag0, and the
exact24-field default group. HEX regeneration from that ELF matches byte-for-byte.
The original AutoTarget config/ForcePi source, firmware pair, hashes and strict
configuration assertions were preserved.

[verification.json](verification.json) records the actual command lines,
exit codes, script/log hashes, source hashes and separate baseline/final runs.
The source ZIP includes referenced baseline/final logs, new host logs, failed
regression evidence, preserved-evidence inventory and firmware verifier output.
It excludes generated intermediate objects and duplicate build trees.

## Not run / blockers

- HARDWARE_STOP_VALIDATION: NOT_RUN / NOT_VALIDATED (continuous mode/lease new).
- STATIC_250_PERFORMANCE: NOT_RUN / NOT_VALIDATED.
- ROTATING_LOAD_PERFORMANCE: NOT_RUN / NOT_VALIDATED.
- Target500 extension: NOT_TESTED.
- Sensor acquisition delay, receive worst-case jitter, MCU/IRQ latency, electrical
  reversal deadtime, continuous duty/thermal ratings and permitted loads: NOT_VALIDATED.
- Independent watchdog is not initialized by this application; no certification
  is claimed for global interrupt disable, CPU or external hardware failure.

These blockers keep physical output compiled OFF. The sole next field activity
is the disconnected-power timing observation in the current README. It cannot
replace the hardware qualification list in [TUNING_AND_FIELD.md](TUNING_AND_FIELD.md).
