# SD700 - BuildToTarget3_SourcePort1

Current candidate directly ports the supplied Field182402 working-tree pressure core, SHA256 `E6E08DC4E370A853EA9A586D765D97D5B6A1E7D6DDC9760202B947F99FB4D512`. The previous standard HEX/5 V assumption is historical. [Source, differences and actual validation](Docs/BuildToTarget3_SourcePort1/HANDOFF.md).

For Target250: source continuous5 V approach before contact10 N; far-stage final10 V/12 ms; near/precision stages follow the source's progressive functions, with250 N precision ultimately7 V/8 ms. Output always passes through MotorExecutor. These are command equivalents, not electrical measurements or ratings. Gains remain10/0/0 diagnostic only; PSU0.5 A unchanged. No other target tuning was performed.

Pulses end in **BRAKE** (both drivers enabled, both PWM compares0). Source30/400 ms settling is measured from the actual TIM5 end, without a second wait. New valid frames may renew BRAKE's receive lease; they never extend a high pulse. Target's first valid reach still causes true OFF and decay monitoring, with no active HOLD, automatic repress or RELEASE.

STOP, sensor validity/order/freshness,125 ms feedback gap,20 ms sample age,130 ms receive lease, absolute overforce and independent normal+1 ms cutoff remain. The12 s cumulative admission limit is disabled; exposure is still recorded. The source's full final precision ladder needs more than5 s: the persistent no-response deadline is now45 s, with the derivation and retained START/STOP accounting in the handoff. Existing8 s approach/40M command-ms reservation and108 s verified-OFF reset remain. No normal overall capture/session deadline is restored.

Identity F10D/46530113/profile7; Build config v5/digest4163791885. Only integer Target1..3000 is writable; Assist/Continuous slots remain0. This delivery accepts only the250 N software path; other source branches are not physical qualifications.

Current [HEX](output/BuildToTarget3_SourcePort1/firmware/SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release.hex), [ELF](output/BuildToTarget3_SourcePort1/firmware/SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release.elf), [SHA256 manifest](Firmware/ForceServo1.SHA256SUMS.txt). Verify checkout before any field connection:

```powershell
git switch main
git pull --ff-only
python .\tools\verify_force_servo_firmware.py
```

The existing supervised wrapper `tools/run_force_characterization.ps1 -Port COM5 -TargetForceN 250` now prints the source profile, variable pulse timing, BRAKE and45 s no-response policy. It keeps one explicit START confirmation, full CSV/metadata/report and manual/fault STOP behavior. No current-limit change or automatic repeat. The agent did not connect serial, flash or run the machine.

CSV retains actual command/normal/hard timing and post-pulse force pairs; new fields show `brake_active`, its receive deadline, `segment_settle_ms`, source precision level/escape/band and source-filtered force. Old forward interpulse fields are0. The immutable90-word Build profile is read back and checked before START.

O2 original-source/production output comparison and affected safety tests PASS; one ARM Release and one strict verifier PASS. Historical full suites and O0/Os were NOT_RUN. [Actual test record](Docs/BuildToTarget3_SourcePort1/HANDOFF.md). **PHYSICAL_STATUS=NOT_RUN**; software PASS does not prove physical250 N success or hardware stopping.

GitHub main is the source of truth; no ZIP delivery. Historical evidence and firmware retain their paths/hashes. [Repository index](Docs/REPOSITORY_INDEX.md), [policy](AGENTS.md), [INA240 reference](Reference/Hardware/ina240.pdf). No cleanup was performed this turn.
