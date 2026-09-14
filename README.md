# SD700 ? StaticForceRuntimeCharacterization1

**physical test NOT RUN.** One supervised characterization HEX supports all runtime
combinations below, without recompiling or reflashing between deliberate sessions.
GitHub `main` is the delivery. No ZIP. Baseline `80fe5767f702be26284a9d6b7f97cd445766e027`
matched fetched `origin/main` and the clean worktree before implementation.

[Current HEX](output/StaticForceRuntimeCharacterization1/firmware/SD700_ForceServo1_StaticForceRuntimeCharacterization1_RealBench_Release.hex)
? [ELF](output/StaticForceRuntimeCharacterization1/firmware/SD700_ForceServo1_StaticForceRuntimeCharacterization1_RealBench_Release.elf)

HEX SHA256: `35A72464F8DBD5E3CFE1C8C72FDED97D7771BF8F2BD956603E77FC7EBF21D99E`

ELF SHA256: `60F5AB4CFF7A8793DB8C6C3BC3C7C082565C56D4ADC0F2CD7A4EA76B843C8A14`

The three physical-test inputs are arbitrary integer **TargetForceN 1..3000**,
**AssistPercent 0..40**, and **ContinuousPercent 0..10**. Percentage maps to command
`round(percent * 240)` with nonnegative half-up rounding after binary32 encoding.
Examples: assist20/25/30/35/40% ?4800/6000/7200/8400/9600; continuous3/5/7.5/8.5/10%
?720/1200/1800/2040/2400. Zero assist disables it; zero continuous cap allows no
ordinary PRESS. RELEASE remains capped at100. Continuous percentage is a ceiling:
trajectory ? P/PID ? executor ? monitored continuous output ? active HOLD remains
in use. It is not fixed PWM and percentage is not torque.

The installed sensor interface is **USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT**:
one reported unit =1 N, scale1, offset0. The received16-bit `raw_pressure_counts`
remains separately recorded; unit2 `measured`/`force_N` means measured_force_N.
This is the user's sensor-unit confirmation, not a new calibration. Qualification
bits remain0; no >3000 N mechanical qualification has been invented.

Firmware fixes Kp10/Ki0/Kd0, measurement_filter_s0, reference200 N/s and1000 N/s?,
output slew1000 command/s,20 kHz PWM, RELEASE100, sample age20 ms, feedback gap125 ms,
receive-anchored lease130 ms and reversal OFF deadtime2 ms. Generic Parameters and
legacy target/profile writes cannot tune or arm this build. Plain ForceServo still
builds locked; this dedicated build requires the existing commissioning/RealBench
acknowledgement and physical-output guard.

**SHORTEST_SOFTWARE_VALID_SUPERVISED_CHARACTERIZATION_ENVELOPE**:
assist rise1 ms, normal handoff by2 ms, independent hard cutoff4 ms, reservation4 ms,
maximum one assist per deliberate START. Timing begins on safe assist admission,
not at the PC command timestamp. It retains the initial-contact/positive-demand
and30 N taper guards. Measured rise?2 N ends assist early; rise?25 N aborts. The
first fresh post-assist response is checked before any ordinary increase and
?25 N stops with Fault5/Detail20 (`FAULT_DETAIL_POST_ASSIST_EXCESSIVE_RISE`).
Active-assist excessive rise remains Detail19. The receive lease is not extended
by the assist scheduler or handoff. TIM5 retains its conservative one-ms reserve;
missing the tiny service window fails OFF rather than extending assist.

The absolute energized/session limit stays5000 ms. High targets may make a bounded
attempt even if the unchanged trajectory cannot finish in five seconds. MCU expiry
stops with Fault5/Detail15 and `TARGET_NOT_REACHED_WITHIN_SESSION` unless reached.
Targets below3000 may use existing active HOLD within that same budget. Exactly3000
is a boundary probe: the first fresh valid measurement?3000 immediately turns OFF,
terminates with `BOUNDARY_TARGET_REACHED`, and does **not** HOLD. Other runs still
trip at?3000. Low targets remain subject to unchanged contact-loss protection.

After an active session ends,5000 ms is an **administrative supervised-test re-arm
lockout; NOT a validated thermal cooling time**. STOP/fault does not refund an
assist reservation. A new complete plan, readback/acknowledgement and deliberate
START after lockout starts a new per-session4 ms budget. No automatic repeat,
escalation or restart; reboot is not evidence of safe thermal conditions.

**NOT_A_MOTOR_RATING ? NOT_A_THERMAL_RATING ? NOT_A_CONTINUOUS_RATING.** No3000 N
validation, continuous40%/10% rating, PI tuning or thermal rating is claimed.

## One manual field command

Verify idle/output OFF, sensor supply/readback, safe permitted load/travel and an
accessible physical STOP. Keep the actual PSU setting **0.5 A unchanged**. Pull and
verify the current pair, flash this one HEX once, then choose **one** command:

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 250 -AssistPercent 20 -ContinuousPercent 5
```

The wrapper verifies the repository HEX/ELF with pure Python (no ARM toolchain),
asks for flash/supervision/unchanged0.5 A confirmation and actual gap, stages and
reads back the whole plan, prints its commands and fixed envelope, then requires
ENTER immediately before its **one START**. Do not add a manual START. It observes
at most five PC seconds, sends STOP, verifies register OFF, and creates unique
CSV/report/metadata paths. S/Escape or physical STOP must abort abnormal motion,
noise, current indication, heat or mechanical behavior immediately. Physical STOP
remains available while serial calls wait. Return those three files and one brief
field account; no automatic repeat.

Other permitted manually selected examples (each is a separate deliberate choice,
never an automated matrix):

```powershell
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 500 -AssistPercent 30 -ContinuousPercent 7.5
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 1000 -AssistPercent 30 -ContinuousPercent 10
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 3000 -AssistPercent 40 -ContinuousPercent 10
```

250 or500 N may also be paired manually with20%/5%,30%/7.5% or40%/10%.
Intermediate values such as650 N,25% or35% assist,6% or8.5% continuous are supported.
These are software bounds, not evidence that any combination reaches its target.
The PC stop reserve may end observation before the MCU's five-second expiry;
reports distinguish PC termination from a recorded MCU session timeout.

[Protocol and wire schema](Docs/StaticForceRuntimeCharacterization1/PROTOCOL.md),
[field handoff](Docs/StaticForceRuntimeCharacterization1/HANDOFF.md),
[actual software results](Docs/StaticForceRuntimeCharacterization1/TEST_RESULTS.md),
[execution records](Docs/StaticForceRuntimeCharacterization1/verification.json).
Original field evidence and historical firmware remain intact. The old3% no-motion
evidence does not validate the new1/2/4 ms envelope. Register OFF and host PASS do
not validate hardware stopping. **HARDWARE_STOP_VALIDATION / STATIC_250 /
ROTATING_LOAD remain unvalidated.**

## Reproduce software checks

Use fresh output directories; previous logs and synthetic captures are not replaced.

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/StaticForceRuntimeCharacterization1/recheck-legacy
python tools/run_force_servo_tests.py --output output/StaticForceRuntimeCharacterization1/recheck-locked
python tools/verify_capturefix1.py --commissioning --characterization --objcopy-cross-check --output output/StaticForceRuntimeCharacterization1/recheck-all
.\tools\build_gcc.ps1 -ForceServo -StaticForceRuntimeCharacterization1 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForceRuntimeCharacterization1/rebuild
python tools/verify_force_servo_firmware.py --objcopy-cross-check
```

Only the optional developer cross-check/build needs ARM executables. The default
verifier checks real hashes, ELF type/bounds, addressed flash load bytes including
RAM initializers, HEX checksums, identity, output guard and every pinned profile,
fixed-config and runtime-contract field. It cannot attest what is flashed in a
connected MCU; the operator confirms that separately.
