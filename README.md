# SD700 - StaticForceAuthority2

**physical test NOT RUN.** GitHub main is the delivery; no ZIP. Built from actual
main `07fe6077c095f4229f68fbbb9168b9aaf55271b2` (StaticForce3000_Boost1), preserving
its independent cutoff and verified lower-output handoff.

The existing ForceServo now supports an explicit bounded assist rise, independent
normal PRESS cap, response/taper exit and continuous P/PI BUILD/HOLD. The requested
20/30/40% assists and 10% normal cap pass synthetic production-path tests, but
**those profiles are disabled in this HEX**. The named field files and a reviewed
output/on-time/cumulative/OFF-cooling envelope were unavailable. No new physical
rating or N calibration is inferred from the observations described by the user.

**Actually enabled: profile4, PRESS720 / RELEASE100, no assist, maximum5 s.**
This is the inherited short low-output envelope, not a force-building improvement
at3%. Do not repeat the reported no-motion3% test expecting this build to supply
10/20/30/40%. Plain `-ForceServo` remains compiled LOCKED; no runtime unlock.

[Current HEX](output/StaticForceAuthority2/firmware/SD700_ForceServo1_StaticForceAuthority2_RealBench_Release.hex)
and [ELF](output/StaticForceAuthority2/firmware/SD700_ForceServo1_StaticForceAuthority2_RealBench_Release.elf).

HEX SHA256: `D895A3895FDC08ABDCA7231F5D71140610FD5C192D606BB45E6E909AB52F0BC6`

ELF SHA256: `5BFE60DED2B6F2392F7EB47FA38794298A05CFF1F4CD60EE02874DD366D43B6F`

| Setting | Actual state |
| --- | --- |
| Profile / protocol / build |4 /F106 /46530108 |
| Target range / raw trip |1..275 legacy control units /325; not N |
| Default Kp / Ki / Kd |10 /0 /0; existing PI configurable |
| Live PRESS / RELEASE |720 /100; nominal3% /0.417%, CCR144 /20 of4800 |
| Disabled candidates |ID20:4800/CCR960; ID30:7200/CCR1440; ID40:9600/CCR1920 |
| Candidates' separate normal cap |Up to2400/CCR480 (10%), disabled pending reviewed limits |
| New assist timing and cooling |Unreviewed: zero sentinels, **not permission for a zero-time burst** |
| Live build / energized / session / capture maximum |5000 ms, unchanged |
| Sample age / feedback gap / receive lease |20 /125 /130 ms, unchanged |
| Reference rate / acceleration / ordinary slew |200 /1000 /1000, unchanged |
| PWM / field PSU setting |20 kHz /0.5 A unchanged; not measured winding current |

[Handoff and evidence limits](Docs/StaticForceAuthority2/HANDOFF.md),
[actual software tests](Docs/StaticForceAuthority2/TEST_RESULTS.md),
[execution record](Docs/StaticForceAuthority2/verification.json),
[defaults](Docs/StaticForceAuthority2/default_parameters.json),
[wire schema](Docs/StaticForceAuthority2/protocol_schema.json).
Historical evidence, failed logs and firmware are preserved. The previous pair's
manifest is `Firmware/StaticForce3000_Boost1.SHA256SUMS.txt`.

## Field checkout and one supervised capture

No new high-assist hardware test is enabled by this delivery. First supply the
reviewed time/cooling limits identified in the handoff; no additional observe-only
round is requested to invent them. The command below is the **existing low-output
profile's safety capture**, for an independently authorized supervised checkout;
it is not a recommendation to repeat the known3% no-motion performance trial.

Verify firmware/IDLE/output OFF and live pressure before motion; use a fixed
workpiece/light-contact setup and accessible physical stop, record the initial
gap, keep0.5 A unchanged. The script sends exactly one START, observes at most5 s
and sends STOP before readback. Use console S/Escape or the physical stop to abort
abnormal motion/noise/current/heat. An already planned active-STOP or feedback-loss
termination check ends this invocation; do not restart automatically to test the
other. Record which check actually ran. If output never becomes active, do not
claim an active-output shutdown was validated. Return CSV/report/metadata and a
short field account. No250 N/500/3000 N trial or live profile escalation.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -ProfileId 4 -Target 250 -MaximumSeconds 5 `
  -ConfirmSupervisedMotion -ConfirmedFirmwareSha256 D895A3895FDC08ABDCA7231F5D71140610FD5C192D606BB45E6E909AB52F0BC6 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'One inherited low-output safety capture; NO ASSIST; record actual termination check' `
  -OutputCsv "captures/static-force-authority2-$stamp.csv"
```

Parameters, config/profile/catalog readback and strict firmware verification run
before START. Field verification is pure Python; ARM tools are only needed for
development. A local file hash plus operator flash attestation is not a device
flash hash measurement. Capture stops between bounded transactions and discards
partial snapshots; true communication errors and unknown START echo remain errors.
A target crossing is not a sustained HOLD result. Register OFF is not physical
stop qualification. **HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain
unvalidated.**

## Reproduce software checks

```powershell
.\tools\run_host_tests.ps1 -OutputDirectory output/StaticForceAuthority2/recheck-legacy
python tools/run_force_servo_tests.py --output output/StaticForceAuthority2/recheck-plain
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForceAuthority2/recheck-all
.\tools\build_gcc.ps1 -ForceServo -StaticForceAuthority2 -MotorMode RealBench -Configuration Release `
  -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/StaticForceAuthority2/rebuild
python tools/verify_force_servo_firmware.py --objcopy-cross-check
```

Use new output directories to preserve evidence. The verifier checks the tracked
current pair; the optional objcopy check independently reconstructs its HEX.
