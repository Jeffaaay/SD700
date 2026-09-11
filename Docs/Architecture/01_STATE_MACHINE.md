# State machine

## States

```c
typedef enum
{
    BOOT_SAFE = 0,
    IDLE,
    JOG_PRESS,
    JOG_RELEASE,
    AUTO_APPROACH,
    AUTO_SETTLE,
    AUTO_PULSE,
    AUTO_HOLD,
    COMPLETE,
    FAULT,
    DIRECT_PRESS_PULSE,
    DIRECT_RELEASE_PULSE
} MachineState;
```

```text
STATE_COUNT=12
```

| State | Purpose | Motor condition |
| --- | --- | --- |
| `BOOT_SAFE` | Disabled startup and self-check | Disabled |
| `IDLE` | Accept configuration and supported starts | Disabled |
| `JOG_PRESS` | Operator-owned bounded press jog | May run under lease and hard maximum |
| `JOG_RELEASE` | Operator-owned bounded release jog | May run under lease and hard maximum |
| `AUTO_APPROACH` | Move toward contact using FIRST or RECONTACT profile | May run under approach timeout |
| `AUTO_SETTLE` | Keep output off, wait for mechanics, then wait for one newer pressure sample | Disabled |
| `AUTO_PULSE` | Apply one bounded correction pulse | May run under pulse duration/backstop |
| `AUTO_HOLD` | Confirm target stability on new pressure samples | Disabled |
| `COMPLETE` | Hold cycle result until acknowledgement | Disabled |
| `FAULT` | Reject motion until explicit safe reset | Disabled |
| `DIRECT_PRESS_PULSE` | Protected fixed PRESS bench pulse | Active only until the 100 ms duration or 150 ms backstop |
| `DIRECT_RELEASE_PULSE` | Protected fixed RELEASE bench pulse | Active only until the 100 ms duration or 150 ms backstop |

`AUTO_APPROACH` contains one small `ApproachProfile` value (`FIRST` or `RECONTACT`). Waiting for post-settle feedback is a phase inside `AUTO_SETTLE`, not another state.

## Commands

```c
typedef enum
{
    CMD_AUTO_START = 0,
    CMD_STOP,
    CMD_JOG_PRESS,
    CMD_JOG_RELEASE,
    CMD_JOG_STOP,
    CMD_FAULT_RESET,
    CMD_ACK_COMPLETE,
    CMD_SET_TARGET,
    CMD_DIRECT_PRESS_PULSE,
    CMD_DIRECT_RELEASE_PULSE
} MachineCommandType;
```

```text
COMMAND_COUNT=10
```

| Command | Meaning |
| --- | --- |
| `CMD_AUTO_START` | Start an automatic cycle from IDLE after readiness checks. |
| `CMD_STOP` | Global stop. It is never rejected for ownership. |
| `CMD_JOG_PRESS` | Start press jog from IDLE, or renew it when repeated by the current owner. |
| `CMD_JOG_RELEASE` | Start release jog from IDLE, or renew it when repeated by the current owner. |
| `CMD_JOG_STOP` | Stop a jog when issued by its owner. Global STOP remains available to everyone. |
| `CMD_FAULT_RESET` | Request the simple reset check in FAULT. |
| `CMD_ACK_COMPLETE` | Acknowledge COMPLETE and return to IDLE. |
| `CMD_SET_TARGET` | Set a validated target in IDLE only. |
| `CMD_DIRECT_PRESS_PULSE` | Request one fixed 10000 mV, 100 ms PRESS test pulse with a 150 ms backstop; not production pressure holding. |
| `CMD_DIRECT_RELEASE_PULSE` | Request one fixed 10000 mV, 100 ms RELEASE test pulse with a 150 ms backstop; not production pressure holding. |

A command carries its source (`UI`, `MODBUS`, or `SERVICE`) and source instance. That stable source owns a jog. A transaction number is response correlation only. Opposite-direction jog commands never reverse a running motor; the current jog must stop first.

The direct commands are bench-only and use the normal semantic path: Modbus -> Machine -> `MotorExecutor_StartPulse()`. ScopeTest and RealBench accept them from IDLE; Locked and RealCompileCheck return unsupported. Automatic start is enabled only in Locked, so direct pulses and automatic closed-loop control cannot coexist in one build.

## Events

```c
typedef enum
{
    EVT_BOOT_OK = 0,
    EVT_BOOT_FAULT,
    EVT_PRESSURE_SAMPLE,
    EVT_CURRENT_SAMPLE,
    EVT_MOTOR_DONE,
    EVT_TIMER_TIMEOUT,
    EVT_PRESSURE_FAULT,
    EVT_OVERPRESSURE,
    EVT_OVERCURRENT,
    EVT_MOTOR_FAULT,
    EVT_INTERNAL_FAULT
} MachineEventType;
```

```text
EVENT_COUNT=11
```

| Event | Minimal payload and use |
| --- | --- |
| `EVT_BOOT_OK` | Boot checks passed. Valid only in BOOT_SAFE. |
| `EVT_BOOT_FAULT` | Boot diagnostic detail. Enters `FAULT_BOOT_FAULT`. |
| `EVT_PRESSURE_SAMPLE` | One raw/control sample, sequence, receive time, and validity. Raw safety is already evaluated first. |
| `EVT_CURRENT_SAMPLE` | Qualified current sample for snapshots/control diagnostics. |
| `EVT_MOTOR_DONE` | Normal completion kind and completion time; used for a pulse that has already stopped. |
| `EVT_TIMER_TIMEOUT` | `MachineTimeoutKind` and the scheduled deadline. Motion timeouts are already output-off when published. |
| `EVT_PRESSURE_FAULT` | Required stream missing or invalid; detail distinguishes timeout and invalid data. |
| `EVT_OVERPRESSURE` | Raw sample and approved limit; immediate fault. |
| `EVT_OVERCURRENT` | Qualified current sample and approved limit; immediate fault. |
| `EVT_MOTOR_FAULT` | MotorExecutor/hardware error detail. |
| `EVT_INTERNAL_FAULT` | CPU, unexpected state, queue corruption, or impossible transition detail. |

Timeouts use one compact enum:

```c
typedef enum
{
    TIMEOUT_NONE = 0,
    TIMEOUT_JOG_KEEPALIVE,
    TIMEOUT_JOG_MAX_DURATION,
    TIMEOUT_APPROACH,
    TIMEOUT_SETTLE,
    TIMEOUT_PRESSURE_FEEDBACK,
    TIMEOUT_PULSE,
    TIMEOUT_AUTO_CYCLE
} MachineTimeoutKind;
```

A timeout event includes the deadline that was armed. The state accepts it only when state, kind, and stored deadline match. Timer cancellation removes a pending instance before a replacement is armed. There are no publisher acknowledgements or retry state machines.

## Main flow

```text
BOOT_SAFE --boot OK--> IDLE
BOOT_SAFE --boot fault--> FAULT

IDLE --jog press/release--> JOG_PRESS / JOG_RELEASE
JOG_* --jog stop or STOP--> IDLE
JOG_* --keepalive/max timeout--> IDLE

IDLE --protected direct pulse--> DIRECT_PRESS_PULSE / DIRECT_RELEASE_PULSE
DIRECT_* --normal motor done--> IDLE
DIRECT_* --backstop/timer/motor fault--> FAULT

IDLE --auto start--> AUTO_APPROACH(FIRST)
                  or AUTO_SETTLE when contact is already present

AUTO_APPROACH --contact--> AUTO_SETTLE
AUTO_SETTLE --settle elapsed + newer sample--+--> AUTO_HOLD
                                               +--> AUTO_PULSE
                                               `--> AUTO_APPROACH(RECONTACT)
AUTO_PULSE --normal motor done--> AUTO_SETTLE
AUTO_HOLD --stable samples/time--> COMPLETE
AUTO_HOLD --low pressure--> AUTO_PULSE or AUTO_APPROACH(RECONTACT)
COMPLETE --acknowledge--> IDLE

any state --qualified fault--> FAULT
FAULT --safe reset--> BOOT_SAFE --boot OK--> IDLE
```

## BOOT_SAFE and IDLE

| State/input | Guard | Action | Next/result |
| --- | --- | --- | --- |
| BOOT_SAFE + `EVT_BOOT_OK` | Clock, configuration, required services, and motor-disabled check pass | Keep disabled; clear transient context | IDLE / accepted |
| BOOT_SAFE + `EVT_BOOT_FAULT` | Any boot check fails | Disable; store fault/detail | FAULT / `FAULT_BOOT_FAULT` |
| BOOT_SAFE + STOP | Always | Disable | BOOT_SAFE / accepted |
| IDLE + `CMD_SET_TARGET` | Target encoding/range valid | Update configuration | IDLE / accepted |
| IDLE + `CMD_AUTO_START` | Configuration and required pressure/current readiness valid | Initialize auto context and cycle timeout | See automatic admission below |
| IDLE + `CMD_JOG_PRESS` | Press jog and required pressure/current readiness valid | Start bounded press run | JOG_PRESS / accepted |
| IDLE + `CMD_JOG_RELEASE` | Release jog and current readiness valid | Start bounded release run | JOG_RELEASE / accepted |
| IDLE + direct PRESS/RELEASE | ScopeTest or RealBench, valid configuration, healthy idle executor; RealBench also requires fresh pressure | Start one fixed bounded pulse through MotorExecutor | DIRECT_PRESS_PULSE or DIRECT_RELEASE_PULSE / accepted |
| IDLE + STOP | Always | Disable idempotently | IDLE / accepted |

Automatic admission uses the latest valid control pressure:

- below the approved contact threshold: select `FIRST`, arm approach and auto-cycle limits, then start `AUTO_APPROACH`;
- at or above contact: keep output disabled and enter `AUTO_SETTLE` directly;
- no valid required pressure/current status: reject the command in IDLE without starting motion.

If MotorExecutor cannot arm all required timers before output enable, it stays disabled and the machine enters `FAULT_MOTOR_FAULT`.

## Jog

| Input | Result |
| --- | --- |
| Same direction command from current owner | Renew only the keepalive timer; the original maximum-duration timer is unchanged. |
| Same direction command from another source | Reject ownership mismatch; motion/timers unchanged. |
| Opposite direction command | Reject busy; never reverse directly. |
| Owner `CMD_JOG_STOP` | Disable first, clear jog owner/timers, enter IDLE. |
| Global STOP | Disable first regardless of owner, clear jog, enter IDLE. |
| `TIMEOUT_JOG_KEEPALIVE` | MotorExecutor has disabled output; clear jog and enter IDLE with protective-stop detail. |
| `TIMEOUT_JOG_MAX_DURATION` | MotorExecutor has disabled output; clear jog and enter IDLE with maximum-duration detail. |
| Required pressure fault in JOG_PRESS | Disable and enter FAULT. |
| Pressure fault in JOG_RELEASE | Record pressure unavailable; keep the existing bounded release jog. Raw overpressure still faults. |

The hard maximum is nonrenewable. A keepalive received at or after that maximum cannot extend output.

## Automatic approach

`ApproachProfile` selects calibration only:

```c
typedef enum
{
    APPROACH_FIRST = 0,
    APPROACH_RECONTACT
} ApproachProfile;
```

| Input | Action/result |
| --- | --- |
| Valid pressure below contact | Update sample and remain in AUTO_APPROACH. |
| Valid pressure reaches contact | Disable immediately, cancel approach timer, record latest pressure sequence, enter `AUTO_SETTLE/WAIT_DELAY`. |
| `TIMEOUT_APPROACH` | Output is already disabled; enter FAULT with `FAULT_MOTION_TIMEOUT` and approach detail. |
| Required pressure fault, overpressure, overcurrent, motor/internal fault | Disable and enter the mapped FAULT. |
| STOP | Disable, clear auto context/timers, enter IDLE. |

FIRST and RECONTACT have separately calibrated bounded command and timeout values, but share this state logic.

## Settle and fresh pressure

`AUTO_SETTLE` has two simple phases:

```c
typedef enum
{
    SETTLE_WAIT_DELAY = 0,
    SETTLE_WAIT_SAMPLE
} SettlePhase;
```

Entry always occurs after `MotorExecutor_Disable()` has completed. While `SETTLE_WAIT_DELAY`:

- the motor remains disabled;
- pressure events update the latest sample/sequence for safety and diagnostics but cannot drive control;
- the one settle timer is armed.

When the matching `TIMEOUT_SETTLE` arrives, the machine records the latest processed pressure sequence as `settle_gate_sequence`, changes to `SETTLE_WAIT_SAMPLE`, and arms one feedback timeout. Because pressure acquisition is serviced before timers, samples already queued during the delay are processed before the gate is captured.

While `SETTLE_WAIT_SAMPLE`:

- duplicate sequence `== settle_gate_sequence` is ignored;
- the first later valid sample from the ordered pressure producer is eligible;
- if the pressure producer restarts, loses ordering, or cannot provide a later sample before the feedback timeout, it publishes `EVT_PRESSURE_FAULT` rather than resetting the sequence silently.

No generation, disabled-evidence handshake, or separate wait state is needed. MotorExecutor is already stopped synchronously before entry, and the single ordered producer plus captured sequence prevents a pre-settle sample from controlling.

The eligible sample is classified by the pure pressure-control helper:

| Pressure condition | Next action |
| --- | --- |
| Below contact threshold | Start bounded RECONTACT approach -> AUTO_APPROACH. |
| At contact but below hold-entry band | Start one bounded correction pulse -> AUTO_PULSE. |
| Inside hold-entry band | Initialize stability tracking -> AUTO_HOLD. |
| Above hold-entry band but below raw safety limit | Keep motor disabled, enter AUTO_HOLD with stability reset, and wait for new samples. |

The feedback timeout or any required-stream fault enters `FAULT_PRESSURE_SENSOR_FAULT` with output disabled.

## Pulse

- The planner chooses only from approved calibrated pulse command/duration limits.
- MotorExecutor arms pulse duration and a later hardware backstop before enabling output.
- Normal duration expiry disables output first and publishes `EVT_MOTOR_DONE`.
- A matching normal completion enters `AUTO_SETTLE/WAIT_DELAY`.
- Backstop or matching `TIMEOUT_PULSE` enters `FAULT_MOTION_TIMEOUT` after output is already off.
- STOP and all safety faults disable immediately.

There is no adaptive boost in V1 unless later calibration evidence explicitly approves it.

## Hold and complete

AUTO_HOLD evaluates only a new valid pressure sample:

| Sample condition | Action |
| --- | --- |
| In hold band | Start/continue stable elapsed time and distinct-sample count. When both calibrated minima are met on this sample, enter COMPLETE. |
| Below contact | Reset stability and start bounded RECONTACT approach. |
| Below hold band but still at contact | Reset stability and start one bounded pulse. |
| Above hold band but below overpressure | Remain disabled in AUTO_HOLD and reset stability. |
| Duplicate/invalid sample | Never advances stability; invalid required stream follows pressure-fault policy. |

COMPLETE keeps the motor disabled. `CMD_ACK_COMPLETE` clears the cycle and enters IDLE. STOP also enters IDLE after an idempotent disable. Motion commands are rejected until completion is acknowledged.

## STOP and fault defaults

STOP is accepted everywhere:

- in JOG/AUTO: disable first, cancel executor/runtime timers, clear transient motion context, enter IDLE;
- in COMPLETE: keep disabled, clear the completed context, enter IDLE;
- in IDLE/BOOT_SAFE: disable idempotently and remain there;
- in FAULT: disable idempotently and remain FAULT. STOP never clears a fault.

Any qualified safety fault from any state disables first, stores one fault class and diagnostic detail, clears transient jog/auto state, and enters FAULT. An unrecognized enum or impossible transition is `FAULT_INTERNAL_FAULT`. Logging failure never delays disable.

## Configuration policy

`CMD_SET_TARGET` is accepted only in IDLE. It is rejected as busy in jog and AUTO, rejected pending acknowledgement in COMPLETE, and rejected in BOOT_SAFE/FAULT. Because configuration cannot change during AUTO, the active cycle reads one stable target without keeping a duplicate frozen target.
