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

The current candidate is StaticForceAuthority2 ReviewFix. Requested20/30/40% assist and
10% normal profiles are represented and synthetic-tested, but disabled pending
reviewed output/on-time/cumulative/OFF-cooling limits. The only enabled live
profile4 is inherited short PRESS720/RELEASE100, no assist, absolute5 s. Do not
advertise new high-output availability or invent time/current ratings. Plain
ForceServo remains LOCKED with no runtime unlock. Keep0.5 A unchanged.
Preserve STOP, freshness, receive-anchored lease, independent peak/session
cutoffs, overpressure, direction interlock, hardware guard and fault latch.
Assist reservation and cooling survive STOP/fault/reset/config/new START within
a boot; reboot is not permission for more exposure. No auto-retrigger/restart.
Post-assist pending response checks must precede normal output updates, without
lease extension; keep logical active peaks distinct from post-assist samples.
No Codex serial connection, flashing or motion. Preserve original field evidence;
software PASS is not physical validation. Use current README/verifier hashes.
