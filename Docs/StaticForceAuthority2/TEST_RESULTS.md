# StaticForceAuthority2 software verification

**physical test NOT RUN.** Executed2026-09-14 UTC (2026-09-13 local).
Base07fe6077c095f4229f68fbbb9168b9aaf55271b2; no unrelated work was present.
Exact commands, UTC start times, exit codes and original log SHA256 values are
in [verification.json](verification.json). [software_test_output.txt](software_test_output.txt)
contains actual outputs, with only newline/trailing-whitespace normalization.
No package, serial access, flashing or physical measurement was performed.

| Executed checks | Actual result |
| --- | --- |
| Existing host regressions |33 PASS;22 build-policy compile checks PASS |
| Plain ForceServo |15 groups each at O0/O2/Os PASS |
| Current live profile / production gate |42 groups each at O0/O2/Os PASS;9 gate cases PASS |
| Synthetic higher-output production path |49 groups each at O0/O2/Os PASS |
| Final PowerShell framing/capture suite |45 cases PASS, including exact README command |
| Force/pressure/AUTO capture SelfTest |PASS |
| Schema/data self-test / report tests |11 /14 PASS |
| Strict firmware rejection tests |24 PASS, including every active/catalog field mutation |
| Current offline verifier / optional objcopy / historical AUTO verifier |PASS |
| ARM RealBench ForceServo Release |PASS; text39868, data100, bss4408;39968 addressed load bytes |
| Source/evidence audit |9 protected source files,78 historical files and PID Init/Prepare/Commit unchanged |

The final full runner was:
`python -B tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForceAuthority2/final-checks`.
It passed all13 components. A recorded final-supplement reran the Force capture
SelfTest (including the newly added exact README command), report tests and
schema checks after those last test/report additions. The production image did
not change after the ARM build. Legacy/plain checks ran in extra-checks. Use the
commands in verification.json to reproduce individual invocations with new
output directories; compiler invocations are included in the host result records.

## What the synthetic tests prove

Real machine/PID/executor/HW/TIM5 code with simulated registers and pressure
commits4800/7200/9600 with CCR960/1440/1920, and normal2400 with CCR480. Fixtures
explicitly enable their own test envelope:3 ms rise,9 ms planned end,12 ms hard
budget/total,1000 ms OFF. These values have no hardware approval. Historical
800 ms calibrated synthetic fixtures remain regression inputs, not live profiles.

Checks cover Target137 as well as250; no-contact/zero-demand/near-target exclusion;
next frame101 ms later; no stale integration/lease renewal; response/taper and
excessive-rise exits; ordinary negative demand through OFF interlock; actual-output
anti-windup, PI influence and integral preservation into HOLD; normal deadline,
missed rise, delayed main hard cutoff, pending expiry before/after lower commit;
STOP/fault/invalid/stale/duplicate frames; no restart; cumulative no-refund and
OFF/config/selection/wraparound. Existing progress/noise/drop, HOLD and finite
session policies remain covered. Synthetic plant lag/deadband/leakage variants
and scripted traces are test assumptions, not predictions of this actuator.

The final capture tests pass every response through production length parsing,
including FC03 config/catalog chunks and complete frozen diagnostic flows. They
cover a2 ms remaining-budget tail without transmitting a2 ms-timeout request,
partial snapshot discard, operator STOP as the next transaction, genuine tail
communication failure retained as CAPTURE_ERROR, and unknown lost START echo with
exactly one START attempt. Observe/invalid catalog/unreviewed profiles send ZERO
START. The README command is parsed, its literal hash/profile/budget/current are
checked, and its run arguments are exercised with simulated transport only.

Pure Python verifier checks exact live identity, current profile, all three
disabled candidates, compiled ceilings/zero peak budget, arming function, hash
manifest, ELF bounds/types and HEX checksums/addressed load equality. Rejections
include altered/unlocked config, each of33 active and99 catalog floats, contract
ceilings/authorization, malformed/truncated/overlapping ELF/HEX and prior images.
It passes with empty PATH and retains checks under optimized Python. Objcopy was
also run as a developer cross-check, not a field dependency.

## Earlier failures and corrections

Two initial O0 synthetic attempts failed; their actual stderr/output is retained
in the transcript and raw local logs. First, the taper test's245 sample also
crossed its deliberately tight excessive-rise threshold10. The test now isolates
taper with a separate synthetic threshold while retaining an explicit10-unit
excessive-rise fault test. Second, a skipped rise left actual output below the
saved normal request. Handoff correctly rejected an increase; the service now
reduces to min(saved normal, current actual), and tests assert that condition.

The first capture run failed an old assertion expecting one completed RUN row
when operator STOP interrupted its first snapshot. The corrected expectation is
zero RUN rows, one discarded partial snapshot and STOP as the next transaction.
Its raw synthetic CSV/report/metadata remain locally; no complete standalone
stdout log was captured for that first run, and none is fabricated. Final45-case
capture suite passed. Later safety-order tests additionally prove an unsafe new
pressure frame cannot cause a transient ramp write before its stop/fault.

## Release facts and limitations

HEX SHA256: `D895A3895FDC08ABDCA7231F5D71140610FD5C192D606BB45E6E909AB52F0BC6`

ELF SHA256: `5BFE60DED2B6F2392F7EB47FA38794298A05CFF1F4CD60EE02874DD366D43B6F`

Live profile4 is720/100 with no assist and5 s maximum.20/30/40% assist and10%
normal profiles are explicitly disabled, not silently clamped demonstrations.
The named handoff/field files, old flashed identity and reviewed output/time/
cooling limits were unavailable. See HANDOFF.md for exact missing inputs and
user-described evidence. No thermal/current rating, N calibration, continuous
20-40% rating, sustained physical HOLD or3000 N qualification is claimed.
**HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD: NOT_RUN for this build.**
