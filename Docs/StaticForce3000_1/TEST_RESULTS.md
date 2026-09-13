# StaticForce3000_1 software verification

Baseline and fetched main were both `6bfb5f1e669234b0e56027b76c5853f94cea3e2a`.
The worktree was clean. No later changes were reset or overwritten.
**physical test NOT RUN.** No hardware connection, flashing, START or supply
adjustment was executed by Codex. No Authority1 physical result was supplied.

## Actual final executions

Sixteen recorded command executions returned0. See
[verification.json](verification.json) for exact argument arrays, UTC timestamps,
elapsed times, compiler commands and SHA256 of original local stdout/stderr logs.
[software_test_output.txt](software_test_output.txt) is the checked-in transcript
with line endings and trailing whitespace normalized; original logs remain in
`output/StaticForce3000_1/`. Synthetic captures are explicitly labeled and are
not field evidence. No package or ZIP check is required or generated.

| Check | Actual result |
| --- | --- |
| Existing `run_host_tests.ps1` suite | PASS,33 host cases and22 build-policy cases |
| Production ForceServo + real arming gate | PASS,41 groups at O0/O2/Os |
| Synthetic calibrated-range/peak fixture | PASS,43 groups at O0/O2/Os |
| Plain ForceServo logical host regressions | PASS,15 groups at O0/O2/Os; separate production gate test proves LOCKED |
| Production compile-time gate matrix | PASS,9 cases; unauthorized ceilings/flags rejected |
| PowerShell framing + complete capture | PASS,36 cases through production length parser; Observe ZERO START |
| ForceServo/pressure/AutoTarget capture self-tests | PASS; no serial hardware |
| Python schema/statistics | PASS,11 checks |
| Python report tests | PASS,12 tests, including calibrated N versus raw counts and unavailable measurement |
| Strict HEX/ELF rejection tests | PASS,21 tests; all26 profile-field mutations rejected |
| Pure-Python verifier and optional objcopy cross-check | PASS,35904 addressed load bytes equal |
| Historical AutoTarget firmware verifier | PASS; earlier pair preserved |
| ARM Release | PASS; text35804, data100, bss4168 bytes |

Commands used (full expanded command records are in verification.json):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 -OutputDirectory output/StaticForce3000_1/regression_delivery
python tools/run_force_servo_tests.py --output output/StaticForce3000_1/locked_host_delivery
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -StaticForce3000 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForce3000_1/build
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForce3000_1/delivery_checks
```

The last command executes the production host O0/O2/Os, synthetic6000/200
representability fixture, gate tests, all capture self-tests, Python data/schema,
strict rejection tests, current and historical verifiers. Its historical filename
is retained as the established regression entry point; it verifies the current
StaticForce3000_1 candidate. No old verification assertion or hash gate was removed.

## Production-path coverage and changed expectations

- The mathematical controller accepts3000 while the live profile rejects unqualified
  `target_N` before START. Synthetic raw1000 ->2010 N with scale2 and offset10
  proves the machine/protocol path does not secretly clamp at275/325 or relabel
  counts as N. Independent raw-trip and calibrated overforce both force OFF.
- A fictional3200 N fixture boundary supports software target3000/trip3100. The
  real drawing's3000 N boundary rejects that ordering. Synthetic2900/2950/3000
  checks margin ordering only; none of these values is a proposed live limit.
- Cubic22 ->3000 reference is22.335 s, reference402.908 at5 s with the unchanged
  numerical rate/acceleration. Slow reference or excessive requested HOLD is
  rejected before START if it cannot fit the selected profile budget.
- Clipped output with sufficient net progress continues beyond5 s in a separately
  qualified **synthetic** long-duration fixture. No-response, bounded noise and
  slow creep stop finitely. The live5 s profile reaches independent TIM5 OFF at
  4999 ms even with continuing valid progress; a5000 ms main safety check latches
  the session timeout. Fresh samples cannot extend that absolute budget.
- Optional peak tests install real production executor PWM plans up to6000,
  CCR1200 (nominal25%), with a synthetic Kp100/slew10000 and800 ms boost duration.
  These are host fixtures, not actuator ratings. Current hardware peak/duration/
  cumulative fields are0; PRESS720/RELEASE100 and initial Ki/Kd0 remain live limits.
- Peak expiry without another main/sample/PC call, a compare expiry racing early
  transfer, invalid feedback, STOP in pending/BUILD/boost/HOLD, direction OFF and
  no restart are exercised. Lower-output transfer is verified before releasing
  the short compare. Full duration reservations survive STOP, fault reset and
  subsequent manual START; the cumulative1600 ms fixture refuses a third boost.
- Profile word reads/digests, wrong profile selection, changed profile bytes,
  wrong config digest, conservative parameter group and target60 admission,
  target_N qualification rejection, lost START echo and device faults are checked.
  Each simulated reply traverses Read-LengthAwareResponse/Get-ModbusResponseLength
  and the unchanged station/function/CRC/exact-length/timeout checks.
- Raw and N statistics remain distinct. An unknown target stays unknown; a
  measured_valid=0 numeric sentinel is not counted as a zero-force measurement.
  Raw MCU peak includes trip samples; measured peak only includes valid in-range
  samples, so it cannot bound an overforce event.

Historical long-running plant/regression cases now explicitly select a host-only
long count-profile envelope. They do not loosen the live5 s profile. The P-only
loaded synthetic plant now stops after no net response rather than running to
20 s indefinitely; the existing PI fixture still reaches250 and active HOLD.
Tracking timeout tests now wait for reference completion. Saturation-timeout
expectations test the finite no-response clock, not elapsed clipping alone.

Development failures were fixed before final executions: old tracking/P-only
expectations, a numeric invalid-cap test that lay inside the expanded host-only
range, an unselected host profile ceiling and an out-of-HOLD synthetic N sample.
Their original O0 assertion logs/hashes are preserved in verification.json and
the transcript. The initial PowerShell profile-mutation fixture also failed its
assertion because it wrote an already-zero halfword; the corrected mutation and
all final36 cases passed. Interim ARM logs are retained locally; only the final
matched firmware pair is the current candidate.

## Final firmware and limits

HEX: `output/StaticForce3000_1/firmware/SD700_ForceServo1_StaticForce3000_1_RealBench_Release.hex`

HEX SHA256: `39967AF2116B38A5FDB65C2D44AE83C04D5BA7EA3559932B09C64D2FCBE56AD9`

ELF SHA256: `95B4C99A75929D10E26D2DBE4DA8A708089B1285D6C1B50032E99BB326460323`

F104 /46530106. Profile1 is an uncalibrated count-domain SHORT experiment, not a
qualified continuous rating. Live target1..275 counts, raw trip325, PRESS<=720,
RELEASE<=100,0.5 A supply unchanged, <=5 s output eligibility/session/capture.
New-sample age20 ms, active gap125 ms and receive-anchored lease130 ms remain.
Plain firmware gate is LOCKED; the explicit candidate retains its existing
compile-time acknowledgement/arming path and hardware guard. No runtime unlock.

Missing higher-force qualifications: sensor full-scale and multi-point N
calibration with coverage/uncertainty; weakest assembled mechanical limit and
stopping margin; motor/driver continuous, peak and stall current; peak/on/HOLD
and cumulative exposure duration, minimum OFF/cooling time, and the time base
of the drawing's10% duty. The supplied24 V/3000 N/5 mm/s/60 mm/internal-limit-switch
facts do not fill those gaps.0.5 A supply setting is not a measured winding limit.

**NOT RUN:** physical STOP, physical feedback-loss/lease validation, force rise/
taper/HOLD at any target, full-range calibration, peak/continuous current or
thermal duty, and rotating load. Software PASS is not physical qualification.
Use the one combined short supervised session in the current README, with no
automatic retries or autonomous increase toward3000 N.
