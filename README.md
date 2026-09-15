# SD700 - BuildToTarget2_CoolingAnchorFix1

Current candidate: **BuildToTarget2_CoolingAnchorFix1**, based on clean main/origin/main `8e2973a5a6b9869d14cfc63569989dc8cf1d9ce8` (BuildToTarget2_Interpulse1). A failed preload-to-pulse admission now anchors cooling at its verified physical OFF, after refreshing the MCU clock. Accounting may already have cleared preload_active; that no longer lets cooling start at the preceding pulse end. Repeated STOP never clears budgets or moves the anchor. All pulse/boost/preload parameters and8 s/12 s/108 s/5 s thresholds are unchanged. [Inherited source mapping](Docs/BuildToTarget2_Interpulse1/SOURCE_REFERENCE.json) and [actual regression](Docs/BuildToTarget2_CoolingAnchorFix1/TEST_RESULTS.md).

**PHYSICAL_STATUS=NOT_RUN.** No serial connection, flashing or machine motion. One HEX supports integer TargetForceN **1..3000 N**; synthetic force attainment is not measured attainment or stable holding. Installed sensor units remain user-confirmed, scale1/offset0, without a new calibration claim.

The modern state machine, single motor executor, atomic Target plan, one START, fresh feedback, receive-anchored lease, STOP priority, output guard, direction interlock and TIM5 remain. PID/trajectory stay available and diagnostic with locked10/0/0. Internal adaptive bounded segments provide build authority; the field interface is Target / START / STOP.

| Stage | Actual command and timing |
|---|---|
| APPROACH: not yet contacted, force<10 N, error>3 N | Base5000 (5 V equivalent,20.83% PWM). Old COARSE boost +500 per qualified >=200 ms check with absolute movement<1 N, reset on movement>=1 N, maximum3500. Total maximum8500 (8.5 V equivalent,35.42% PWM). Normal99 / independent hard100 ms per segment |
| Contacted MICRO build / taper | Base800..3000 according to old error3..50 N formula, clipped at3000 above50 N. Add adaptive boost0..4000; total<=7000. Two post-pulse absolute movements<1 N add300; any absolute movement>=1 N resets boost and low count |
| FINE, positive error<=3 N | Old base400..1000 with smaller additive boost ceiling1000; total<=2000. Same response/reset rule. Positive residual<=1 N uses the400 minimum base to seek the exact target; zero/negative error is OFF |
| MICRO/FINE timing | Normal=max(2,floor(10*base_command/applied_command)) ms; independent hard=normal+1 ms (3..11 ms). At3000+4000: normal4 / hard5 ms; at1000+1000: normal5 / hard6 ms |
| MICRO/FINE gap | Forward preload starts at300 command, range200..600 (0.20..0.60 V equivalent,0.833..2.5% PWM). An accepted fresh post-forward-pulse drop strictly greater than2 N adds100, capped600. Exactly2 N does not change it. Same rule for both modes; preload persists across explicit START and cooling, like old ResetAdaptive |
| Cooldown/feedback | At least30 ms after the pulse, plus ordered fresh post-pulse feedback received at/after end+30 ms, age<=20 ms. MICRO/FINE remain forward preloaded; COARSE remains OFF. No elapsed-time retry or feedback reuse. No reverse Build owner or forward preload after an accepted reverse/legacy pulse |
| Exposure | Full hard time reserved before arming; no refund. Approach<=8000 ms, pulse hard reservations plus prepaid preload time<=12000 ms. Preload counts full bridge-enabled wall time, with no PWM-duty discount. Unused prepaid time can fund later preload gaps but never reduces the reservation ledger or resets a safety budget. Additional approach sum(command*hard_ms)<=40,000,000, derived from the preceding5000*8000 envelope; higher approach amplitude consumes it faster |
| Reset/cooling |108000 ms uninterrupted verified OFF, including boot. Pulse OFF, phase changes, STOP/fault and new START do not erase cumulative safety budgets. Cooling never restarts output |
| Persistent no response |Progress anchor starts at this START's first accepted valid fresh force; START/STOP/new plan never clear the timer.5000 ms accumulated active BUILD/TAPER including pulse gaps without valid post-pulse net NEW HIGH progress>=2 N. Alternating noise and short500-600 ms platforms do not reset the budget or cause Detail17 |
| First valid target reach | Immediate bridge OFF, state16 monitoring only, for every target including3000. No BRAKE, preload, active HOLD, automatic repress or RELEASE |

The normal TIM5 compare performs a same-direction preload update through the existing executor/HW update path. Only after the bounded preload compare is verified, with no pending/expired hard event, may TIM5 retain a receive-anchored preload deadline. The counter keeps running through preload-to-pulse transitions; cutoff flags are never erased to renew output. A pending hard event, lease loss or failed handoff turns OFF and revokes ownership. Preload is limited by the original receive+130 ms lease and available prepaid exposure; new feedback cannot extend the original high pulse.

**Physical equivalence remains unproven.** The old forward interpulse behavior is now present, but the modern executor, strong feedback gate, independent cutoff and exposure budgets remain. The old400 ms retry without fresh feedback and old final preload HOLD are deliberately excluded.

As in the old code, an adapted600 preload can exceed the minimum400 FINE pulse; it is bounded separately and target reach still takes immediate OFF priority.

Old MICRO/FINE formulas have a discontinuity: unboosted error4 N gives846 command, error3 N gives1000. This is disclosed, not hidden behind a claim of strict monotonic energy. Overall near-target pulse energy and the FINE boost ceiling are lower; the map gives exact vectors and adaptations. A previously latched contact never re-enters COARSE. Initial error<=3 uses FINE; already-at/exceeded target produces no segment.

There is **no normal overall session/build/capture timeout**. Detail17 remains diagnostic-only. Genuine no-response, finite exposure, feedback gap125 ms, sample age20 ms, original receive lease130 ms, sensor invalid/order loss, STOP/E-stop, overforce, independent pulse hard cutoff, excessive post-pulse rise25 N and fault latching remain. Continuous/ONE Assist ownership is unavailable in this candidate; BUILD cannot retrigger Assist. No Ki or rotating PID changes.

All voltages here are command equivalents at the existing24 V mapping. PWM/CCR are software requests, not measured motor voltage/current. The command-time cap is an additional exposure constraint, **not measured heat or a motor/thermal/continuous rating**. The existing0.5 A PSU setting is unchanged. Finite safety budgets do not guarantee reaching a target on the physical plant.

Firmware: [HEX](output/BuildToTarget2_CoolingAnchorFix1/firmware/SD700_ForceServo1_BuildToTarget2_CoolingAnchorFix1_RealBench_Release.hex), [ELF](output/BuildToTarget2_CoolingAnchorFix1/firmware/SD700_ForceServo1_BuildToTarget2_CoolingAnchorFix1_RealBench_Release.elf). [SHA256 manifest](Firmware/ForceServo1.SHA256SUMS.txt), [handoff](Docs/BuildToTarget2_CoolingAnchorFix1/HANDOFF.md). Identity F10C /46530110 / profile7; immutable Build config version3, digest2848875769. Strict Python-only verifier checks actual ELF identity/configuration/guard/owner denial, checksums, addressed load bytes and exact hashes; no field ARM executable is required.

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

The wrapper prints these stage ceilings/times before one explicit START confirmation. It reads capability,50-word active config,66-word operating profile,198-word disabled catalog,76-word immutable Build profile and312-word frozen diagnostics, with production length-aware framing/CRC/exact length checks and <=11-register chunks. The reserved Assist/Continuous slots must both be0; they do not represent the internal Build output. CSV/metadata/report expose interpulse_active, interpulse_command, interpulse_next_command, interpulse_deadline_ms, interpulse_spent_ms and interpulse_credit_ms; current_committed and PWM identify actual software output during each gap. Configuration/identity mismatch means ZERO START.

Exactly one script START, no additional manual START and no automatic repeat. CSV streams through build and target-OFF decay monitoring until S/Escape, external STOP or a real fault. Stop immediately for abnormal motion/noise/current/heat. Return CSV, metadata, report and brief field conditions. Capture cannot certify hardware stopping; target touch and sampled OFF retention are separate from stable holding or rotating-load qualification.

[Actual tests and commands](Docs/BuildToTarget2_CoolingAnchorFix1/TEST_RESULTS.md) - [Protocol and identity](Docs/BuildToTarget2_CoolingAnchorFix1/PROTOCOL.md) - [Repository policy](AGENTS.md). GitHub main is the source of truth. No ZIP/package. Historical firmware, evidence and failure logs remain intact.
