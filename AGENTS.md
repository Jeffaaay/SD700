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

The current candidate is StaticForceRuntimeCharacterization2. The user explicitly
provided the supervised1/2/4 ms assist envelope (4 ms reserved per deliberate
session),5000 ms administrative inter-run lockout,30/2/25 N guards,
and USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT (scale1 offset0, unit2).
The current user removed overall session/build/capture deadlines. Zero force START
uses normal continuous approach; first valid contact at20 N latches one assist
eligibility. Below3000 N: active HOLD until manual STOP or fault;3000 N: first
valid reach immediately OFF, no HOLD. Keep receive/assist and conditional stall
protections; zero deadline fields apply only to this runtime unit2 profile.
Only TargetForceN1..3000, AssistPercent0..40 and ContinuousPercent0..10 are runtime
field inputs. All gains, timing, RELEASE100, reference/slew and protection remain
firmware-owned. The lockout is NOT validated thermal cooling time. No motor,
thermal, continuous output or3000 N qualification is claimed.
Preserve atomic plan/readback/version/digest and single-consumption START authority,
post-assist pending checks, STOP, freshness, receive-anchored lease, independent
cutoffs, direction interlock, hardware guard and fault latch. No auto-retrigger,
escalation, restart or field PI tuning. Plain ForceServo remains locked.
No Codex serial connection, flashing or motion. Preserve original field evidence;
software PASS is not physical validation. Use current README/verifier hashes.
