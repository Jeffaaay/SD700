# SD700 ? BuildToTarget1

Current candidate: **BuildToTarget1**, based on main `9de33246b67603e797ff31cc8472f54b9bd0520d`.
One firmware supports integer TargetForceN **1..3000 N**. Sensor unit is the user's confirmed installed sensor output, scale1/offset0; this is not a new calibration or force qualification.
**PHYSICAL_STATUS=NOT_RUN.** No serial connection, flashing or machine motion was performed for this delivery. Synthetic target attainment is not physical attainment or stable holding.

The build restores `APPROACH ? BUILD ? TAPER ? TARGET_REACHED_OFF` using the existing state machine, single executor, atomic plan/one START, fresh feedback, receive-anchored lease, output guard and TIM5 cutoff. PID/trajectory remain available and diagnostic (10/0/0, Ki locked); repeated bounded segments supply build authority. No old hardware driver is copied, no direct PWM bypass, no active HOLD or preload in this candidate.

| Phase / limit | Command and independent time contract |
|---|---|
| APPROACH from0 N, error>50 N and not contacted | 5000 command (~20.83% PWM mapping); normal OFF99 ms / hard100 ms; cumulative approach reservation8000 ms |
| BUILD, contact latched at10 N, error>50 N | base3000 + boost0..4000; maximum7000 (~29.17% mapping). After two valid post-pulse rises<1 N, +300; a positive response retains earned boost. Hard duration=floor(30000/command) ms,4..10 ms; normal OFF hard?1 ms. At7000: normal3 / hard4 ms |
| TAPER, remaining error3..50 N | no boost; scaled800..3000 command; normal9 / hard10 ms |
| Fine, remaining error(0,3] N | no boost;400..1000 command; normal4 / hard5 ms. Energy decreases at the fine transition even where amplitude steps upward |
| Every segment | bridge truly OFF for>=30 ms AND an ordered valid sample received at/after end+30 ms, age<=20 ms; during-pulse samples never authorize the next segment |
| All segments combined | reserve full hard duration before arming; maximum12000 ms per thermal epoch. Early STOP/fault gives no refund; no PWM-duty discount |
| Budget reset / boot |108000 ms uninterrupted verified bridge OFF, also after boot. START/STOP/fault, pulse OFF and phase changes never reset the budget. Output stays OFF after cooling; explicit new plan/START required |
| True no force response |5000 ms accumulated active BUILD/TAPER time including pulse OFF, reset only by valid post-pulse net new-high progress>=2 N or full thermal epoch reset. No500 ms tracking fault; tracking time remains diagnostic |
| Reach any target, including3000 | immediately disable bridge, enter state16, monitor only; no BRAKE, small voltage, automatic repress, RELEASE or HOLD |

Low targets take precedence over contact/APPROACH: they enter TAPER directly. An already attained target produces no segment. Raw>=3000 N remains protected: initial IDLE at the trip is rejected/faulted; target3000 on the first valid post-START sample turns OFF before any output. Continued exactly3000 monitoring is OFF; >3000 or raw3000 for a lower target faults.

There is **no normal overall session/build/capture deadline**. Finite exposure, no-response, sensor-invalid, feedback gap125 ms, sample age20 ms, original receive lease130 ms, per-segment hard cutoff, excessive post-pulse rise25 N, STOP/E-stop, overforce, fault latch and direction guards remain. A new BUILD contract does not repeatedly trigger ONE Assist; continuous/Assist output ownership is unavailable in this build. Historical Assist4 ms code/protection remains for historical profiles.

The exposure/cooling values are conservative experimental software constraints, **not a motor/thermal/continuous rating**. The user-reported3.6 A rated,7 A stalled/~30 s and10% work/rest (2 min/18 min) are not bench measurements.12 s ON reservation +108 s OFF adopts a shorter1:9 work/rest envelope; PWM duty is a different quantity. PSU remains0.5 A; command, CCR and bridge-enabled elapsed estimates are not winding-current or temperature measurements. See [source basis](Docs/BuildToTarget1/SOURCE_BASIS.md).

Firmware: [HEX](output/BuildToTarget1/firmware/SD700_ForceServo1_BuildToTarget1_RealBench_Release.hex), [ELF](output/BuildToTarget1/firmware/SD700_ForceServo1_BuildToTarget1_RealBench_Release.elf).
Exact SHA256 values are in [the current pair manifest](Firmware/ForceServo1.SHA256SUMS.txt) and [handoff](Docs/BuildToTarget1/HANDOFF.md). F10A /4653010C / profile6. Strict verifier checks actual ELF identity, all configuration bytes, output guard, addressed load bytes, HEX checksums and pinned hashes using Python alone.

For a supervised field session, pull current main and verify before connecting:

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python .\tools\verify_force_servo_firmware.py
```

After flashing the verified pair locally and keeping the permitted travel/load, E-stop and existing0.5 A setting, leave output OFF at least108 seconds after boot. This wait is in the same session; it is not an additional test round. Record actual initial gap and supply setting.0 N is allowed. Use the actual local COM port (COM6 shown):

```powershell
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 250
```

Only Target is a field control; no AssistPercent, ContinuousPercent, Kp/Ki/Kd, pulse duration or cooling override. The wrapper shows the real stage commands/budgets and asks for one explicit START confirmation. It checks capability,50-word FC03 config,66-word operating profile, disabled catalog,52-word immutable Build profile and284-word frozen diagnostics, all chunked through the production length-aware parser. The legacy atomic plan output slots must both be zero; these are reserved, not an indication of zero Build authority. Any readback/configuration mismatch means ZERO START.

The script sends exactly one START. It streams CSV throughout build and OFF monitoring, continuing after target reach to record pressure decay until S/Escape, external STOP or a real fault. Use independent STOP/E-stop immediately for abnormal motion/noise/current/heat; PC sampling cannot certify the physical stop. Return CSV, metadata, report and brief field conditions. Do not auto-repeat. Target reach and sampled OFF retention are separate observations; neither authorizes rotating-load PID tuning or proves3000 N performance.

[Tests and commands](Docs/BuildToTarget1/TEST_RESULTS.md) ? [Protocol](Docs/BuildToTarget1/PROTOCOL.md) ? [Repository policy](AGENTS.md).
GitHub main is the source of truth; no ZIP/package delivery. Historical firmware, logs and field evidence are retained.
