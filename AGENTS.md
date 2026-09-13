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

The current candidate is Target250Authority1, explicitly requested by the user
as one SHORT supervised experiment: Target250, Kp10/Ki0/Kd0, PRESS720/RELEASE100.
These are command units, not measured motor voltage or a production/continuous
rating. Keep the physical 0.5 A supply setting and PC observation <=5 seconds.
The current user instruction supersedes Continuous2's prior no-powered-profile
restriction. Default ForceServo remains compiled LOCKED; the existing explicit
output flag is reused by -Target250Authority1, without runtime unlock. Preserve
STOP, freshness/lease, raw overpressure, direction interlock, hardware guard,
fault latch, finite deadlines and no restart. No automatic cap increase or
repeat START. Software PASS is not physical validation; use current README.
