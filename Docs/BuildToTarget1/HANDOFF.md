# BuildToTarget1 handoff

Delivery is the normal pushed main commit; no ZIP. Baseline9de33246b67603e797ff31cc8472f54b9bd0520d was clean and equal to fetched origin/main. Existing main history, earlier field firmware and raw evidence are retained.

Current candidate: BuildToTarget1, F10A /4653010C / profile6. One HEX supports integer target1..3000 N. Read exact current pair and hashes from `Firmware/ForceServo1.SHA256SUMS.txt`; strict Python verifier validates the actual binaries, not this document alone.

This delivery restores an independently bounded approach from0 N and repeated feedback-qualified build pulses. The previous P-only amplitude decline is not used as the sole build command. Existing PID/trajectory, atomic plan, communications and one executor remain. Gains10/0/0 are locked; no rotating PID or field Ki change. Only Target is field writable. The fixed higher segment amplitudes are disclosed and read back; old Assist/Continuous plan slots are reserved zero, and the continuous owner is unconditionally denied.

Commands/time budgets:

| Stage | Command | Normal OFF / independent hard cutoff |
|---|---|---|
| Approach, force<10 N and target error>50 N |5000 (~20.83% PWM mapping)|99 /100 ms;8 s cumulative approach hard reservations|
| Build, target error>50 N |3000..7000 (~12.5..29.17%)|hard floor(30000/command), normal hard?1 ms; at maximum3 /4 ms|
| Taper, target error3..50 N |800..3000|9 /10 ms|
| Fine, target error(0,3] N |400..1000|4 /5 ms|
| Target reached or negative error |0, bridge OFF|no HOLD/preload/BRAKE/repress/RELEASE; monitor only|

All stages require trueOFF>=30 ms and genuinely new post-end sensor feedback before another segment. Reserve hard duration before arming, no refunds: all stages12 s total, approach8 s. Only108 s uninterrupted verifiedOFF resets the epoch; boot also requires108 s OFF. STOP/fault/phase/pulse OFF/new START cannot wash reservations. A depleted no-response counter also denies a new START, including after fault reset. Pending/elapsed cutoff during a requested reduction is fail-closed rather than canceled as normal completion. ISR-revoked owner state is volatile and output commit remains in the existing critical section.

No normal session/build/capture5 s limit. Detail17 tracking expiry is removed from build control; tracking remains diagnostic. True no-force-response stops after5 s accumulated active BUILD/TAPER without valid post-pulse net>=2 N new-high progress;500?600 ms platforms and cumulative slow progress are accepted. A1 N oscillation is not additive progress. This does not diagnose mechanical jamming without position/current data. Freshness, receive lease, bad-sensor, STOP/E-stop, overforce, direction/output guard, independent segment cutoff and fault latch remain. At3000 first valid reach OFF; beyond3000 faults. Low targets/already reached are OFF before any floor output can persist.

The12000/108000 ms envelope is a conservative experimental design choice based on user-reported work/rest1:9 and shorter than reported stalled exposure, not a validated thermal calculation.20 kHz PWM remains.0.5 A PSU setting remains. No actual winding current or temperature measurement; command/PWM/elapsed estimates must not be represented as those measurements.

Field use is one supervised local session: pull/verify current main and flashed HEX, confirm permitted mechanics/E-stop/0.5 A unchanged and record actual gap; wait boot108 s OFF in the same session. Current `run_force_characterization.ps1` takes Port and TargetForceN, displays the actual stage profile, requires one START confirmation, records CSV continuously and waits through targetOFF decay monitoring until human/external STOP or a real fault. Use S/Escape or independent STOP/E-stop immediately on abnormal motion/noise/current/heat. Return CSV, report, metadata and brief observations; no automatic repeat.

Target touch and sampled OFF retention are separate report fields. Reaching target in synthetic feedback does not prove physical attainment or stable holding. **PHYSICAL_STATUS=NOT_RUN.** HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD, physical pulse timing, thermal/continuous rating and1000/2000/3000 N physical qualification remain unvalidated. No serial/flash/motion action was run by this implementation session.

See SOURCE_BASIS.md for exact modified old-source hashes and design decisions, PROTOCOL.md for new readback/telemetry, TEST_RESULTS.md and verification.json for actual software evidence.

Final HEX SHA256: 21258CF7E2F4D9D0ADDDA251DF482852BC7F3F087A81E475BDE548DA0EEED6F2

Final ELF SHA256: F98652424AACE83A525BBEBCD85EC1A3D81190B0CD5FC5638D0CDC53F06DC2E3
