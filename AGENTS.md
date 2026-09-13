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

The current software candidate is Target250Continuous2: POWERED_TEST_READY=NO.
No higher continuous motor/board rating is available. PRESS/RELEASE stay at the
retained 100 command ceilings; this does not solve the reported lack of pressure
rise or certify those ceilings. Keep the physical 0.5 A supply setting unchanged.
Default ForceServo remains compiled LOCKED. The existing explicit output flag is
reused by -Target250Continuous2; no runtime unlock. Do not arm higher defaults
from host-only range fixtures. Keep STOP, lease/freshness, raw overpressure,
direction interlock, hardware guard and no restart. Current README is authoritative;
no powered field procedure until the missing continuous rating evidence is resolved.
