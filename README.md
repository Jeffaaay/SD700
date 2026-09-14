# SD700 - StaticForceAuthority2 ReviewFix

**physical test NOT RUN.** GitHub main is the delivery; no ZIP. Review baseline
`b9df15185526be1ecf096a270c5e4da6b9708e3f` matched the fetched remote and clean
worktree. This is a focused software fix, not authorization for higher output.

The supplied reproduction confirmed a missed excessive-rise check after normal
assist handoff. Pressure27 to37 at the first frame101 ms after admission used to
continue BUILD at command93/CCR19. It now stops with **Fault5/Detail19 before any
normal P/PI or output update**. The9 ms handoff remains **89/CCR18**. Assist output
ends on schedule; one pending response evaluation neither extends it nor renews
the original receive lease. After a valid below-threshold first response, ordinary
BUILD is not subjected to the old assist's threshold indefinitely.

**Live profile4 is unchanged: PRESS720 / RELEASE100 /no assist /maximum5 s.**
Requested20/30/40% assist candidates and normal2400 remain disabled pending
reviewed experimental output/time/cooling limits. Their original high-output
field-delivery objective is not complete. Plain ForceServo is compiled LOCKED.
No new pulse loop, PID change, fixed10% minimum, current-limit change or runtime
unlock. Field PSU0.5 A,20 kHz PWM, Kp10/Ki0/Kd0, target1..275 legacy control units,
raw trip325, freshness/lease/STOP and absolute session budgets remain unchanged.

[Current HEX](output/StaticForceAuthority2_ReviewFix/firmware/SD700_ForceServo1_StaticForceAuthority2_ReviewFix_RealBench_Release.hex)
and [ELF](output/StaticForceAuthority2_ReviewFix/firmware/SD700_ForceServo1_StaticForceAuthority2_ReviewFix_RealBench_Release.elf).

HEX SHA256: `7346FA1A6B4D6DE045DAC1D03A13B46BC15F0F41053F1C7B146143E3FAFEF2D3`

ELF SHA256: `D0B1961A06EC5263F0C222151C3410CAFED7B69D84B5A5D6E5FD2DD34D2641A7`

[Handoff and specific missing profile limits](Docs/StaticForceAuthority2_ReviewFix/HANDOFF.md),
[actual test results](Docs/StaticForceAuthority2_ReviewFix/TEST_RESULTS.md),
[execution records](Docs/StaticForceAuthority2_ReviewFix/verification.json),
[defaults](Docs/StaticForceAuthority2_ReviewFix/default_parameters.json),
[schema](Docs/StaticForceAuthority2_ReviewFix/protocol_schema.json).
Original reproduction, failed regression and corrected results are preserved.
Historical field evidence and firmware remain unchanged; the predecessor manifest
is `Firmware/StaticForceAuthority2.SHA256SUMS.txt`.

## Response boundaries and diagnostics

F107/build46530109 carries216 frozen words (81 u32 +27 float), with unchanged
50-word config,66-word active profile and198-word disabled catalog. Pending
response evaluation requires a fresh valid ordered frame, sequence strictly after
the handoff's observed sequence, received at/after its end timestamp (wrap-safe).
It runs before the control-grid skip. The frame that triggers early handoff is
checked while assist is active and cannot also count as the after frame. A later
sequence with the same end millisecond can qualify. Pre-end timestamps cannot
consume the pending check or renew the lease. Invalid/duplicate/stale frames and
STOP/fault retain unevaluated state; inactive sessions never resume evaluation.

`assist_pressure_peak` stays the sampled peak while boost was logically active.
New `assist_response_peak` adds the first evaluated post-handoff response; neither
is a measured PWM-window peak. `assist_response_pending`, `assist_after_result`
(0 unevaluated,1 below threshold,2 excessive), after receive timestamp and sequence
make the result explicit. Late after data following STOP/fault is observational;
it cannot overwrite the original fault or authorize motion. Capture still discards
partial snapshots, preserves real errors and never retries an unknown START echo.

## Field boundary and command contract

This software fix requests **no repeated3% no-motion performance test** and enables
no new high-output run. No hardware was connected or operated. The following is
the inherited tool command contract, checked against simulated transport only;
it is **not an instruction to run a performance test now**. Before any separately
authorized supervised session, verify IDLE/output OFF, current firmware and live
pressure, fixed/light-contact setup and accessible physical stop. Keep0.5 A,
record actual gap, send at most the single script START, never a duplicate manual
START. S/Escape or physical stop aborts abnormal motion/noise/current/heat. Do not
automatically repeat or escalate. CSV/report/metadata and a short account describe
only checks actually completed; register OFF is not physical stop qualification.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -ProfileId 4 -Target 250 -MaximumSeconds 5 `
  -ConfirmSupervisedMotion -ConfirmedFirmwareSha256 7346FA1A6B4D6DE045DAC1D03A13B46BC15F0F41053F1C7B146143E3FAFEF2D3 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'Inherited low-output command contract; NO ASSIST; no performance retest requested' `
  -OutputCsv "captures/static-force-authority2-reviewfix-$stamp.csv"
```

Firmware verification remains strict pure Python; no field ARM tool dependency.
Capture budgets250 ms START wait, trajectory, HOLD dwell,2100 ms worst-case frozen
snapshot and100 ms STOP reserve within5 s. No need to finish a snapshot before
STOP; no2 ms timeout is manufactured. Target crossing alone is not sustained HOLD.
**HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated.**

## Reproduce software checks

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/StaticForceAuthority2_ReviewFix/recheck-legacy
python tools/run_force_servo_tests.py --output output/StaticForceAuthority2_ReviewFix/recheck-plain
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForceAuthority2_ReviewFix/recheck-all
.\tools\build_gcc.ps1 -ForceServo -StaticForceAuthority2 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForceAuthority2_ReviewFix/rebuild
python tools/verify_force_servo_firmware.py --objcopy-cross-check
```

Use new output directories to preserve evidence. Independent reproduction compiler
commands and before/after results are in verification.json; all are host-only.
