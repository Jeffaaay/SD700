> Historical ForceServo1 locked-candidate record. CommissioningUnlock1 now
> supersedes its output lock, current parameter bounds and next field step.
> Follow the [current README](../../README.md) and
> [commissioning record](../CommissioningUnlock1/TEST_RESULTS.md).
> The historical instructions below do not require a separate Observe field round.

# ForceServo1 RAM parameters and protocol

All defaults are COMMISSIONING_NOT_TUNED numerical seeds used in host tests.
Physical arming is compiled OFF independently of these parameters. These ranges
are software validation bounds, not certified mechanical/electrical ratings.

| Parameter | Unit | Default | Allowed range |
| --- | --- | --- | --- |
| `kp` | mV/unit | 1 | 0 .. 1000 |
| `ki` | mV/(unit s) | 0 | 0 .. 1000 |
| `kd` | mV s/unit | 0 | 0 .. 100 |
| `d_filter_s` | s | 0.02 | 0.001 .. 1 |
| `reference_rate` | unit/s | 20 | 0.1 .. 200 |
| `reference_acceleration` | unit/s^2 | 40 | 0.1 .. 1000 |
| `output_rate` | mV/s | 1000 | 1 .. 100000 |
| `press_cap` | mV | 250 | 1 .. 4999 |
| `release_cap` | mV magnitude | 100 | 1 .. 799 |
| `integral_min` | mV | -1000 | -5000 .. 0 |
| `integral_max` | mV | 1000 | 0 .. 5000 |
| `tracking_gain` | 1/s | 5 | 0.01 .. 100 |
| `measurement_filter_s` | s | 0 | 0 .. 1 |
| `control_min_ms` | ms, integer | 5 | 1 .. 50 |
| `feedback_gap_ms` | ms, integer | 40 | 2 .. 100 |
| `sample_age_ms` | ms, integer | 20 | 1 .. 99 |
| `lease_ms` | ms, integer | 50 | 3 .. 100 |
| `hold_enter` | unit | 5 | 0.1 .. 20 |
| `hold_exit` | unit | 10 | 0.2 .. 40 |
| `session_ms` | ms, integer | 45000 | 1000 .. 60000 |
| `saturation_ms` | ms, integer | 5000 | 100 .. 30000 |
| `tracking_error` | unit | 50 | 1 .. 275 |
| `tracking_ms` | ms, integer | 10000 | 100 .. 30000 |
| `reverse_deadtime_ms` | ms, integer | 2 | 1 .. 100 |

Relations: min control < max feedback/control gap < lease; age < lease;
hold enter < exit; Imin < Imax; tracking_gain * max_dt <= 1;
saturation/tracking deadlines <= total session. All timing fields are integers.
Target, raw abort, contact threshold, build30s, feedforward0, executor5000/800,
and physical arming are protected independently of this RAM group.

## Atomic parameter transaction

Single Modbus master, existing 115200 8N1 / station1 / CRC16 transport.
FC06 register0x100 value0xB101 begins a fresh staged group; 0x110..0x13F hold
all 24 IEEE754 binary32 parameters in the table order (high16 then low16).
Every word must be written. FC06 register0x101 value0xC101 validates and commits
the whole group only IDLE + outputOFF, without START. Runtime changes reject.
Invalid/partial/stale transactions preserve the old entire group. A new begin
discards the previous staging. Config version increments only on successful apply.
FC03 over0x110..0x13F reads the active group. Maximum read is still11 words.
Config digest is FNV1a32 over canonical little-endian IEEE754 bytes in field order;
it is a change detector, not a cryptographic authentication mechanism.

FC04 0x100..0x107: F101 capability, physical lock, config word count48, diagnostic
word count112, config version high/low, digest high/low. New START is FC05 coil0x10
ON only; new explicit fault reset is coil0x11 ON. Existing coil1 OFF is STOP.
Legacy AUTO_START, direct and JOG movement are rejected in ForceServo.

## Coherent diagnostics

FC06 register0x102 value0xD101 freezes the current published diagnostic once.
FC04 register0x200..0x26F then reads that immutable snapshot in <=11-word chunks.
Control updates cannot modify it. Only another snapshot-latch command replaces it.
Field order/types are in [protocol_schema.json](protocol_schema.json), generated
from the C header; all fields use32 bits, high16 first. No C struct ABI is exposed.
The complete snapshot is112 words. The u32 fields precede the float fields.

`control_sequence` counts accepted numeric PID updates; `sample_hi/lo`,
`received_ms`, `raw`, dt, reference, P/I/D/FF, raw_output, control_committed and
next_integral are from that one update. Contact initialization has control count0.
`now_ms`, state/fault/detail, current_committed, outputOFF/PWM/lease reflect the
published status (which can be a subsequent STOP). Thus control_committed is
kept as the historical control result while current_committed becomes0 at STOP.
Latest sample fields separately show raw safety input, including a fault sample.
`rx_interval_*` are receiver ISR extrema since receiver initialization;
`delivered_interval_*` are actual main delivery extrema. Neither is sensor
acquisition time. A raw fault sample never enters the qualified PID peak statistic.

The captured snapshot can predate the latch by one loop; its MCU now/control times
are explicit. UART and snapshot reads do not renew output. Byte counts, PC wall
time, skipped receiver sequences, missed control updates and coverage limits are
reported; no unobserved peak or stable interval is interpolated. The bus transfer
and turnaround can miss control samples. Stability remains INSUFFICIENT_DATA
when coverage is insufficient. PID request changes do not reset HOLD continuity.

## Tools

`python tools/force_servo_data.py --defaults` prints a complete seed JSON group.
`--validate FILE` checks it offline; `capture_force_servo.ps1 -Mode Parameters`
applies and reads back a whole group with motor power disconnected, never STARTs.
`-Mode Observe` is the sole next field step; see the current README command.
`-Mode SingleStart` implements one guarded START and finally STOP for a later
qualified release, but refuses the currently locked firmware. Lost write/START
echo does not cause a retry. No auto fault reset or automatic repetition exists.
Existing capture and firmware-verifier scripts continue to target their original
AutoTarget candidate. Do not use them to assess ForceServo HOLD.
