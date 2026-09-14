# SD700 repository delivery policy

GitHub `main` is the source of truth. After a validated software change:

1. Run applicable builds/tests and strict firmware verification.
2. Keep the current field HEX/ELF tracked, with README and firmware manifests accurate.
3. Update the root tracked-source SHA256 manifest when tracked files change.
4. Commit and push `origin/main` through a normal fast-forward update.

Delivery is the pushed commit. Do not generate ZIPs, tarballs, source/review
bundles, archive hash sidecars, extracted-package copies or nested deliveries.
Do not add a package-verification prerequisite for field use. This policy
supersedes historical packaging instructions in engineering records.

Field checkouts use `git switch main`, `git pull --ff-only`, `git rev-parse HEAD`,
then `python tools/verify_force_servo_firmware.py`. Use the current repository
firmware identified by README and `Firmware/ForceServo1.SHA256SUMS.txt`.

Preserve unique field evidence and useful historical engineering records.
Inspect generated directories before removing them. Retain useful test logs;
remove regenerable objects, caches, temporary build trees and duplicate outputs.
Never use `git reset --hard`, `git clean -fdx` or force push for cleanup/delivery.

The current candidate is StaticForce3000_Boost1, explicitly authorized by the
user for one short supervised Target250 breakaway experiment. Profile3 is legacy
control counts, continuous PRESS720/RELEASE100, peak6000, boost10 ms/total10 ms.
Normal main-loop handoff is attempted at8 ms; unchanged TIM5 cutoff reserves1 ms
and forces OFF if handoff is missed. Keep existing0.5 A and absolute5 s limits.
No continuous25% rating, N calibration or3000 N qualification is established.
Cumulative reservation survives STOP/fault/reset/new START within a boot. No
retrigger, automatic repeated START or permission inferred from a reboot.
Plain ForceServo remains LOCKED with no runtime unlock. Preserve STOP, freshness,
receive-anchored lease, fixed boost/session deadlines, independent overpressure,
direction interlock, hardware guard and fault latch. No Codex serial connection,
flashing or hardware motion. Preserve original field evidence; software PASS
is not physical validation. Use the current README and exact verifier hashes.
