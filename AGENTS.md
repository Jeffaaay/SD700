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

The current candidate is StaticForce3000_1. The explicit armed build permits
only profile1: legacy control counts, target1..275, PRESS<=720/RELEASE<=100,
unchanged0.5 A supply setting and <=5 s absolute energized/session/capture scope.
This remains an unvalidated short experiment, not a continuous thermal rating.
No live Newton calibration or high-output boost qualification exists. Peak
hardware activation remains disabled; high-range/high-PWM fixtures are synthetic.
Plain ForceServo remains LOCKED with no runtime unlock. Preserve STOP, freshness,
receive-anchored lease, fixed boost/session deadlines, independent raw and
calibrated overforce, direction interlock, hardware guard, fault latch and no
restart. Latest user instructions allow one combined supervised static session
within the confirmed short profile; no Codex hardware motion, current increase,
automatic repeated START or unqualified3000 N trial. Use the current README.
