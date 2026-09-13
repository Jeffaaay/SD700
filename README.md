# SD700 - Target250Authority1

**SHORT supervised experimental profile. FIELD_STATUS=NOT_RUN.**
720 command units are not a validated continuous or production-safe rating.
Target250 is sensor control units, not calibrated Newtons. Keep the physical
supply setting at0.5 A. No serial connection, flashing or physical test was
performed by this software work.

From clean main/origin/main `a4c48457b723b715bc83c0ca5f2166df3735b737`.
GitHub main is the source of truth; normal commit/push delivery, **no ZIP**.
The user's explicit720 experimental profile supersedes the previous candidate's
no-higher-output restriction; it does not provide a new hardware rating.

[Current RealBench Release HEX](output/Target250Authority1/firmware/SD700_ForceServo1_Target250Authority1_RealBench_Release.hex)

HEX SHA256: `322537355B059AA080B5C2356452D43C1C685E30889640D87E01CF5213037A48`

[Matching ELF](output/Target250Authority1/firmware/SD700_ForceServo1_Target250Authority1_RealBench_Release.elf)

ELF SHA256: `DB6D3364EF0BFC1DF05A6F0E1065EF052684E0DBE63EE8B117C54015614D1E06`

## Selected profile

The existing trajectory -> PID -> continuous monitored executor -> active HOLD
implementation is unchanged. Only the explicit first supervised profile changes
from Kp1/PRESS100 to Kp10/PRESS720. Ki/Kd remain0; RELEASE stays100. No old pulse
control, additional controller, gain schedule or START/safety architecture change.

| Setting | Current value |
| --- | --- |
| Target / Kp / Ki / Kd |250 sensor units /10 /0 /0 |
| PRESS operating cap / profile ceiling |720 /720 command units |
| RELEASE operating cap / profile ceiling |100 /100 command units |
| Reference rate / acceleration |200 sensor units/s /1000 sensor units/s^2 |
| Output slew |1000 command units/s increasing magnitude; immediate magnitude reduction, unchanged |
| Nominal command ramp0 ->720 |0.720 s with sufficient demand; not mechanical acceleration |
| New sample age / active feedback gap / lease |20 /125 /130 ms, unchanged |
| Pending START OFF wait / control minimum / direction OFF |250 /5 /at least2 ms, unchanged |
| Saturation / tracking / build / session timeout |5000 /10000 /30000 /45000 ms, unchanged |
| Target maximum / raw abort / contact threshold |275 /325 /20, unchanged |

At settled reference250, u_raw=10*(250-pressure), clamped to[-100,+720].
The taper threshold is720/10=72 sensor units. At pressure178 output equals720;
above178 it decreases proportionally, reaching0 at250. This arithmetic is not a
physically validated braking distance or a promise of Target250 attainment.
P-only may retain steady-state error under load. BUILD and HOLD use the same
controller; existing PI/anti-windup remains available, with initial Ki0.

| Pressure | Calculated raw P | Calculated settled command |
| --- | --- | --- |
|22 |2280 |720 |
|100 |1500 |720 |
|178 |720 |720 |
|200 |500 |500 |
|220 |300 |300 |
|240 |100 |100 |
|245 |50 |50 |
|249 |10 |10 |
|250 |0 |0 |
|260 |-100 |-100 |
|270 |-200 |-100 |

Values above are software calculations, not field measurements. Existing PWM
mapping ceil(command*4799/24000) gives CCR144 at720. ARR4799 and20 kHz mean
nominal duty144/4800=3%, nominal input high time1.5 us. Neither720 nor planned
CCR144 is an externally measured voltage/current/waveform. PWM frequency, polarity,
SD enable sequence and register/output guard are unchanged. The0.5 A supply setting
is not a motor-winding current measurement or a continuous thermal qualification.

`Application/force_servo_output_profile.h` supplies parameter defaults and ceilings
for controller validation, protocol writes/readback and executor admission/update.
There is no downstream100 PRESS clamp.721 PRESS and101 RELEASE are rejected.
The operating bounds remain writable within the profile, but SingleStart requires
the exact initial parameter group, including720/100, Kp10, Ki/Kd0 and unchanged
safety timings. Plain `-ForceServo` retains its locked gate, Kp1/caps100 defaults
and prior reference/timing profile. Non-host overrides of the selected ceiling
still fail compilation. Host-only900/200 test fixtures are not field settings.

STOP priority, stale/invalid feedback shutdown, receive-anchored TIM5 lease,
overpressure, direction break-before-make, finite deadlines, hardware guard,
fault latch and no automatic restart remain intact. The five-second saturation
fault is retained, not extended. It counts continuous amplitude clipping under
the existing semantics and does not prove mechanical stall. The PC's five-second
observation limit is separate and is not a new MCU five-second session deadline.

## One short field test

Use the new verified HEX and the established supervised setup; confirm IDLE/OFF,
fixed workpiece and initial light contact. Keep0.5 A unchanged. On COM5 run the
capture once below: the script sends the only START, Target250, observes at most
5 seconds and issues STOP/readback. STOP immediately for abnormal motion, noise
or current behavior (S/Escape in the interactive console; use the physical E-stop
for an immediate emergency). Confirm physical stop. Return CSV/report/metadata;
no manual extra START, automatic retry, current increase or repeated trial.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
$gap = Read-Host 'Actual initial gap/contact setup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
.\tools\capture_force_servo.ps1 -Mode SingleStart -Port COM5 -Target 250 -MaximumSeconds 5 `
  -ConfirmSupervisedMotion `
  -ConfirmedFirmwareSha256 322537355B059AA080B5C2356452D43C1C685E30889640D87E01CF5213037A48 `
  -CurrentLimitSetting '0.5 A; manually confirmed unchanged' -InitialGap $gap `
  -FieldNotes 'One short supervised experiment; record motion/noise/current behavior and physical stop' `
  -OutputCsv "captures/target250-authority1-$stamp.csv"
```

Run in a normal interactive PowerShell console. Do not extend observation to
wait for HOLD. Any brief HOLD must fit inside the same five-second window.
The script's error/timeout path preserves available data and sends STOP without
retrying START. Software StopVerified does not substitute for physical shutdown.
Do not test500 or3500 N or use remote/unattended motor operation.

## Identity, capture and software checks

SchemaF103 is unchanged; build ID is46530105. Frozen diagnostics remain128 words,
active parameters48 words, read in the existing11-word chunks. Telemetry retains
pressure/reference/error, P/I, raw/post-limit/requested/committed output, limiter
flags, planned PWM, freshness/lease, fault, MCU session peak and stop reason.
The report separates PC sampled peaks from MCU sampled maxima and never infers
motion from a nonzero command. Metadata labels the experiment as unvalidated.

Strict verification pins the actual new HEX/ELF hashes,24 defaults and16-word
contract, including720/100 limits, experimental readiness1 and the existing
arming gate. Readiness1 means only this user-authorized short experiment, not
continuous/production qualification. Python's standard library suffices offline;
objcopy remains an optional developer cross-check. Hash attestation does not read
MCU flash. All earlier firmware and evidence remain historical and byte-preserved.

[Actual test commands/results](Docs/Target250Authority1/TEST_RESULTS.md),
[defaults](Docs/Target250Authority1/default_parameters.json),
[schema/bounds](Docs/Target250Authority1/protocol_schema.json),
[current firmware manifest](Firmware/ForceServo1.SHA256SUMS.txt).

**physical test NOT RUN.** Hardware STOP, Target250/HOLD response, electrical output,
thermal duty and rotating-load performance remain physically unvalidated.
