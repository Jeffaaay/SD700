# SD700 - Target250Continuous2

**POWERED_TEST_READY=NO. PHYSICAL_PERFORMANCE=NOT_VALIDATED. physical test NOT RUN.**

From clean main/origin/main `c73e55ba505c4f7f43dff481c32155d0558075b8`.
GitHub main is the source of truth; delivery is the normally pushed commit, no ZIP.
Target250 means sensor control units, not calibrated Newtons.

The software change is complete, but the reported no-pressure-rise performance
issue is unresolved. No motor continuous-current/duty rating or qualified
continuous board/motor electrical and thermal measurements were found. The
retained PRESS/RELEASE command ceilings remain 100/100; they are not newly
certified ratings. **Do not repeat the old 100-cap field trial as a claimed fix.**
No higher powered default or powered-test procedure is provided. The public
capture CLI refuses SingleStart before connecting a port while readiness is NO.
The explicit RealBench image retains its previous output arming gate at 100;
it is not a physically locked image. Plain `-ForceServo` builds remain LOCKED.

[Current HEX](output/Target250Continuous2/firmware/SD700_ForceServo1_Target250Continuous2_RealBench_Release.hex)

HEX SHA256: `2B99E36C3694BDC8A9E9A0145161CBA537432607AA3A2268176CD62A18D0CA83`

[Matching ELF](output/Target250Continuous2/firmware/SD700_ForceServo1_Target250Continuous2_RealBench_Release.elf)

ELF SHA256: `CE5CF188C03AA229288B57DF766615BF5819B0FD9E9494BDB9BF3967C62E35AA`

## What changed

The same trajectory -> PID -> continuous executor -> active HOLD path remains.
START still latches once, stays OFF until the next acceptable fresh sample, and
cannot restart after STOP/fault. No pulse controller, gain schedule, feedforward,
minimum positive command, new machine state, or communications framework was added.

`Application/force_servo_output_profile.h` defines independent PRESS and RELEASE
profile ceilings and operating defaults. Controller bounds, RAM parameter commit,
FC03 readback, executor planning/update, capture validation, embedded contract and
strict verifier agree. Operating limits must be positive and within their own
ceiling; invalid writes reject the whole group. The old duplicate 100 checks and
separate 5000/-800 fallback in the explicit profile no longer silently restrict a
future reviewed profile. Production builds currently reject overrides above100.
Host-only 600/200 ceiling fixtures prove scaling through actual production code;
these arbitrary numbers are not operating recommendations or hardware approval.

For the explicit profile, output magnitude increases at the existing1000 command
units/s. Reducing PRESS or RELEASE magnitude follows the lower demand immediately
on the next accepted control update. A sign change starts the opposite ramp from
zero and still passes through executor OFF/deadtime. This avoids retaining a large
PRESS command solely because a symmetric falling slew limiter was slow. STOP,
overpressure, feedback loss and hardware faults bypass normal ramping and force OFF.
Default locked-profile slew behavior is unchanged. PI actual-committed-command
anti-windup, sub-unit quantization behavior and BUILD/HOLD state continuity remain.

| Setting | Delivered value |
| --- | --- |
| PRESS profile ceiling / operating cap | 100 /100 command units (nominal mV) |
| RELEASE profile ceiling / operating cap | 100 /100, separately retained |
| Kp / Ki / Kd | 1 /0 /0 |
| e_taper = Upress/Kp | 100 sensor units; proportional taper starts below error100 |
| Reference rate / acceleration | 200 units/s /1000 units/s^2, unchanged |
| Output slew | 1000 command units/s increasing magnitude; immediate reduction |
| Command ramp 0 to cap | about0.100 s if demand is already large; sample-grid dependent |
| New-sample age / active feedback gap / lease | 20 /125 /130 ms, unchanged |
| Pending OFF wait / minimum control / reversal OFF | 250 /5 /at least2 ms, unchanged |
| Build / session / saturation / tracking timeout | 30000 /45000 /5000 /10000 ms, unchanged defaults |
| Target maximum / raw abort / contact | 275 /325 /20, unchanged |

With no justified higher ceiling, Kp1 is retained together with Upress100 rather
than presented as new validated tuning. At error228 the raw command228 still
clips to100. This e_taper leaves a broad100-unit proportional region; it is only
candidate arithmetic, not measured braking distance. Reference movement, sample
delay, inertia, command slew and hardware dead zones remain relevant. P-only may
have steady-state error under load; Ki remains available for later measured tuning,
but is zero in this candidate and is not used to defeat an output cap.

## Calculated command sweep (not field measurements)

Reference250, Ki=Kd=0, settled slew/interlock. Positive=PRESS, negative=RELEASE.

| Pressure | Error / raw P | Submitted command |
| --- | --- | --- |
| 22 | 228 | 100 |
| 100 | 150 | 100 |
| 180 | 70 | 70 |
| 220 | 30 | 30 |
| 240 | 10 | 10 |
| 245 | 5 | 5 |
| 249 | 1 | 1 |
| 250 | 0 | 0 |
| 260 | -10 | -10 |

## Electrical evidence and limits

The unchanged production mapping is `ceil(command * 4799 / 24000)` for nonzero
commands. Zero uses the executor OFF path. At100, compare20 with ARR4799 and96 MHz
timer clock means20 kHz carrier, nominal duty20/4800=0.416667%, nominal input high
time20/96 MHz=0.208333 microseconds. PRESS selects TIM3_CH3/PB0; RELEASE selects
TIM2_CH3/PB10; SD1/SD2 enable only after the existing safe apply sequence.
These are source-derived planned values. CSV `tim2/tim3` are planned counts, not
raw register telemetry or measured waveforms. The hardware guard checks actual
CCR, timer and enable registers; host tests use register shims. No external voltage,
current, gate waveform, motion or temperature measurement was made in this work.

The schematic labels U6/U7 IR2104STRPBF, Q4-Q7 DON30N10T, 22-ohm gate resistors,
12 V gate supply, bootstrap parts and U8 INA240A1DR/R35 1 milliohm. This is design
evidence, not confirmation of the fitted motor, board thermal path or current
calibration. IR2104 turn-on propagation delay is not a guaranteed minimum input
pulse width;0.208 us cannot be called definitely suppressed without measurement.
No switching-frequency/deadtime or pulse compensation changes were made.
See the [official IR2104 datasheet](https://www.infineon.com/assets/row/public/documents/24/49/infineon-ir2104-ds-en.pdf).

**Keep the physical supply setting at0.5 A.** Supply current is not motor winding
current in a PWM stage, and current limiting alone cannot qualify a duty cycle.
[maxon electrical explanation](https://support.maxongroup.com/hc/en-us/articles/360006322414-Power-conversion-in-PWM-power-stages).

Exact missing evidence: the fitted motor model with rated continuous winding
current, permissible duty/thermal limits and winding characteristics; confirmation
of fitted power-stage parts/cooling; and measured gate/terminal waveform plus
motor-side current and temperature under the proposed bounded continuous command
and fixed load, sufficient for a responsible hardware review to approve a number.
No exact revision/raw evidence identifying the user's previously successful250
build was located. Older pulse amplitudes do not supply that missing rating.
See [field evidence and output trace](Docs/Target250Continuous2/EVIDENCE_AND_PATH.md).

## Capture and verification

Reuse `tools/capture_force_servo.ps1`; no extra observe-only field round is required.
The existing no-hardware tests exercise the complete Observe and simulated
SingleStart transport flows. Actual SingleStart remains unavailable while this
candidate is not powered-test ready. No serial port, flashing or motor operation
was performed. No500/3500 test, current increase or automatic cap escalation.

SchemaF103/build46530104 adds `post_limit_output` and two MCU fields:
`session_peak_raw`, `session_peak_received_ms`. The peak is initialized at session
start and updated from valid ordered fresh samples, including accepted samples
between control updates and the raw-abort sample. STOP/fault preserve it; another
explicit session resets it. It is a sensor-sampled MCU maximum, not a guaranteed
analog-force peak. Pressure/reference/error, P/I, raw/post-limit/requested/committed
commands, limiter bits, planned PWM counts, freshness, state/fault and stop reason
remain in CSV/metadata/report. Parameters still use48 FC03 words; frozen diagnostics
use128 words in the unchanged maximum11-word chunks.

The report labels the PC sampled peak separately, records pressure rise, time to
target/tolerance, overshoot, reported saturation clock and first observed exit from
amplitude clipping, HOLD and software STOP readback. It never infers physical
motion from commands. Physical motion and physical stop remain operator-reported
in field_notes (otherwise NOT_REPORTED). Polling gaps cannot establish the exact
instant of saturation exit or continuous HOLD quality. Saturation is a software
condition, not proof of mechanical stall. The original five-second clipping fault
is retained and the field failure is reproduced in host tests.

```powershell
git switch main
git pull --ff-only
git rev-parse HEAD
python tools/verify_force_servo_firmware.py
```

The verifier is pure Python standard library, including strict hashes, identity,
configuration/readiness/arming, ELF bounds and addressed HEX checksum/load bytes.
`--objcopy-cross-check` is an optional development cross-check, not a field dependency.
This is repository-file verification, not hashing MCU flash.

[Actual software test record](Docs/Target250Continuous2/TEST_RESULTS.md),
[defaults](Docs/Target250Continuous2/default_parameters.json),
[schema and parameter bounds](Docs/Target250Continuous2/protocol_schema.json),
[current manifest](Firmware/ForceServo1.SHA256SUMS.txt).
Old firmware/evidence remains intact and historical. **Hardware STOP, Target250,
active HOLD, electrical/thermal duty and rotating-load performance remain unvalidated.**
