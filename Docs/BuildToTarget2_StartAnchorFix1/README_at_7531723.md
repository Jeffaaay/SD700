# SD700 - BuildToTarget2

Current candidate: **BuildToTarget2**, based on clean main/origin/main `1a4c93d57d386e25efa300a64828e2fef5a67071` (BuildToTarget1). This ports the actual old working-tree adaptive pressure strategy. [OLD_TO_NEW_CONTROL_MAP.md](OLD_TO_NEW_CONTROL_MAP.md) identifies source hashes, functions/lines, calculations, safety adaptations and tests.

**PHYSICAL_STATUS=NOT_RUN.** No serial connection, flashing or machine motion. One HEX supports integer TargetForceN **1..3000 N**; synthetic force attainment is not measured attainment or stable holding. Installed sensor units remain user-confirmed, scale1/offset0, without a new calibration claim.

The modern state machine, single motor executor, atomic Target plan, one START, fresh feedback, receive-anchored lease, STOP priority, output guard, direction interlock and TIM5 remain. PID/trajectory stay available and diagnostic with locked10/0/0. Internal adaptive bounded segments provide build authority; the field interface is Target / START / STOP.

| Stage | Actual command and timing |
|---|---|
| APPROACH: not yet contacted, force<10 N, error>3 N | Base5000 (5 V equivalent,20.83% PWM). Old COARSE boost +500 per qualified >=200 ms check with absolute movement<1 N, reset on movement>=1 N, maximum3500. Total maximum8500 (8.5 V equivalent,35.42% PWM). Normal99 / independent hard100 ms per segment |
| Contacted MICRO build / taper | Base800..3000 according to old error3..50 N formula, clipped at3000 above50 N. Add adaptive boost0..4000; total<=7000. Two post-pulse absolute movements<1 N add300; any absolute movement>=1 N resets boost and low count |
| FINE, positive error<=3 N | Old base400..1000 with smaller additive boost ceiling1000; total<=2000. Same response/reset rule. Positive residual<=1 N uses the400 minimum base to seek the exact target; zero/negative error is OFF |
| MICRO/FINE timing | Normal=max(2,floor(10*base_command/applied_command)) ms; independent hard=normal+1 ms (3..11 ms). At3000+4000: normal4 / hard5 ms; at1000+1000: normal5 / hard6 ms |
| Every segment | True bridge OFF>=30 ms plus ordered fresh post-segment feedback received at/after end+30 ms, age<=20 ms. No timeout-based repeat or feedback reuse |
| Exposure | Full hard time reserved before arming; no refund. Approach<=8000 ms, all segments<=12000 ms. Additional approach sum(command*hard_ms)<=40,000,000, derived from the preceding5000*8000 envelope; higher approach amplitude consumes it faster |
| Reset/cooling |108000 ms uninterrupted verified OFF, including boot. Pulse OFF, phase changes, STOP/fault and new START do not erase cumulative safety budgets. Cooling never restarts output |
| Persistent no response |5000 ms accumulated active BUILD/TAPER including OFF gaps without valid post-pulse net NEW HIGH progress>=2 N. Alternating noise and short500-600 ms platforms do not reset the budget or cause Detail17 |
| First valid target reach | Immediate bridge OFF, state16 monitoring only, for every target including3000. No BRAKE, preload, active HOLD, automatic repress or RELEASE |

Old MICRO/FINE formulas have a discontinuity: unboosted error4 N gives846 command, error3 N gives1000. This is disclosed, not hidden behind a claim of strict monotonic energy. Overall near-target pulse energy and the FINE boost ceiling are lower; the map gives exact vectors and adaptations. A previously latched contact never re-enters COARSE. Initial error<=3 uses FINE; already-at/exceeded target produces no segment.

There is **no normal overall session/build/capture timeout**. Detail17 remains diagnostic-only. Genuine no-response, finite exposure, feedback gap125 ms, sample age20 ms, original receive lease130 ms, sensor invalid/order loss, STOP/E-stop, overforce, independent pulse hard cutoff, excessive post-pulse rise25 N and fault latching remain. Continuous/ONE Assist ownership is unavailable in this candidate; BUILD cannot retrigger Assist. No Ki or rotating PID changes.

All voltages here are command equivalents at the existing24 V mapping. PWM/CCR are software requests, not measured motor voltage/current. The command-time cap is an additional exposure constraint, **not measured heat or a motor/thermal/continuous rating**. The existing0.5 A PSU setting is unchanged. Finite safety budgets do not guarantee reaching a target on the physical plant.

Firmware: [HEX](output/BuildToTarget2/firmware/SD700_ForceServo1_BuildToTarget2_RealBench_Release.hex), [ELF](output/BuildToTarget2/firmware/SD700_ForceServo1_BuildToTarget2_RealBench_Release.elf). [SHA256 manifest](Firmware/ForceServo1.SHA256SUMS.txt), [handoff](Docs/BuildToTarget2/HANDOFF.md). Identity F10B /4653010D / profile7; immutable Build config version2, digest1817994819. Strict Python-only verifier checks actual ELF identity/configuration/guard/owner denial, checksums, addressed load bytes and exact hashes; no field ARM executable is required.

For a single supervised field session, verify the checkout before connecting:

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python .\tools\verify_force_servo_firmware.py
```

After the field team locally flashes the verified HEX, confirm permitted travel/load, E-stop, supervision and unchanged0.5 A. Allow108 seconds true OFF after boot, record actual gap and supply setting.0 N is allowed. Use the actual local port (COM6 shown):

```powershell
.\tools\run_force_characterization.ps1 -Port COM6 -TargetForceN 250
```

The wrapper prints these stage ceilings/times before one explicit START confirmation. It reads capability,50-word active config,66-word operating profile,198-word disabled catalog,66-word immutable Build profile and300-word frozen diagnostics, with production length-aware framing/CRC/exact length checks and <=11-register chunks. The reserved Assist/Continuous slots must both be0; they do not represent the internal Build output. Configuration/identity mismatch means ZERO START.

Exactly one script START, no additional manual START and no automatic repeat. CSV streams through build and target-OFF decay monitoring until S/Escape, external STOP or a real fault. Stop immediately for abnormal motion/noise/current/heat. Return CSV, metadata, report and brief field conditions. Capture cannot certify hardware stopping; target touch and sampled OFF retention are separate from stable holding or rotating-load qualification.

[Actual tests and commands](Docs/BuildToTarget2/TEST_RESULTS.md) - [Protocol](Docs/BuildToTarget2/PROTOCOL.md) - [Repository policy](AGENTS.md). GitHub main is the source of truth. No ZIP/package. Historical firmware, evidence and failure logs remain intact.
