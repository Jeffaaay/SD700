# Interfaces and data

## Ownership

| Mutable data/resource | Sole owner | Readers |
| --- | --- | --- |
| `MachineContext` | Machine module in the control loop | Pure decision helpers and snapshot builder by const reference |
| Motor output, active command, and hardware motion timers | MotorExecutor | Machine through status/result values only |
| Pressure receive/decoder state | Pressure transport | Pressure qualification service |
| Pressure filter state and sequence | Pressure control service | Machine through immutable samples |
| Current acquisition/debounce state | Current control service | Machine through immutable samples/events |
| Runtime settle/feedback/auto timers | Runtime timer service | Machine through `EVT_TIMER_TIMEOUT` |
| Modbus frame/transaction state | Modbus transport | Mapper/response builder |
| UI navigation state | UI | UI only |

There is no mutable application state hidden in function-static variables. UI and Modbus receive only a `MachineSnapshot`; no `MachineContext *` or register pointer crosses the boundary.

## Minimal machine data

The declarations below are shapes for Phase 2B, not production headers in this task. Fields may be grouped differently when implemented, but duplicate state/evidence machinery must not return.

```c
typedef uint32_t MachineTimeMs;
typedef uint32_t PressureSequence;

typedef enum
{
    COMMAND_SOURCE_UI = 0,
    COMMAND_SOURCE_MODBUS,
    COMMAND_SOURCE_SERVICE
} CommandSourceKind;

typedef struct
{
    CommandSourceKind kind;
    uint16_t instance;
} CommandSource;

typedef enum
{
    COMMAND_ACCEPTED = 0,
    COMMAND_INVALID_VALUE,
    COMMAND_NOT_ALLOWED,
    COMMAND_BUSY,
    COMMAND_NOT_READY,
    COMMAND_WRONG_OWNER,
    COMMAND_UNSUPPORTED,
    COMMAND_EXECUTOR_FAILED
} MachineCommandResult;
```

Only `CMD_SET_TARGET` uses `target_pressure_n`; other commands ignore it. The source is used for jog ownership, not safety evidence.

```c
typedef struct
{
    MachineCommandType type;
    CommandSource source;
    int32_t target_pressure_n;
} MachineCommand;
```

Pressure keeps raw and control values together so the event can be audited without rereading transport buffers:

```c
typedef struct
{
    PressureSequence sequence;
    MachineTimeMs received_at_ms;
    uint32_t raw_pressure_counts;
    int32_t control_pressure_n;
    bool frame_valid;
    bool calibration_valid;
} PressureSample;

typedef struct
{
    MachineTimeMs received_at_ms;
    int32_t current_ma;
    bool sample_valid;
    bool calibration_valid;
} CurrentSample;
```

The pressure producer increments `sequence` for each accepted ordered sample and publishes FIFO. Duplicate sequence values do not advance control. If the producer resets or loses ordering while pressure is required, it publishes `EVT_PRESSURE_FAULT`; the machine does not need a general-purpose sequence-generation protocol.

The automatic context contains only values used directly by the automatic algorithm:

```c
typedef struct
{
    ApproachProfile approach_profile;
    SettlePhase settle_phase;
    PressureSequence settle_gate_sequence;
    PressureSequence last_control_sequence;
    MachineTimeMs state_started_ms;
    MachineTimeMs settle_deadline_ms;
    MachineTimeMs feedback_deadline_ms;
    MachineTimeMs auto_deadline_ms;
    MachineTimeMs stable_started_ms;
    uint16_t stable_sample_count;
    uint16_t pulse_count;
} MachineAutoContext;

typedef struct
{
    CommandSource owner;
    MachineTimeMs keepalive_deadline_ms;
    MachineTimeMs maximum_deadline_ms;
} MachineJogContext;
```

`MachineConfig` stores approved calibration values. `CMD_SET_TARGET` changes only the target in IDLE. Because all configuration writes are rejected during an automatic cycle, the active target has one source and does not need a second frozen copy.

```c
typedef struct
{
    int32_t target_pressure_n;
    /* Reviewed pressure/current/motion calibration fields. */
} MachineConfig;

typedef struct
{
    MachineState state;
    MachineFault fault;
    FaultDetail fault_detail;
    MachineTimeMs state_entered_ms;
    MachineConfig config;
    PressureSample pressure;
    CurrentSample current;
    MachineJogContext jog;
    MachineAutoContext automatic;
    MachineCommandResult last_command_result;
} MachineContext;
```

Inactive jog/automatic fields are cleared for debugging convenience, but correctness depends on `state`, not on bytewise canonical representations or compiler padding.

## Events and snapshots

One tagged event carries only the payload its type needs:

```c
typedef struct
{
    MachineEventType type;
    MachineTimeMs occurred_at_ms;
    MachineTimeoutKind timeout_kind;
    MachineTimeMs timeout_deadline_ms;
    PressureSample pressure;
    CurrentSample current;
    FaultDetail detail;
} MachineEvent;
```

Phase 2B may use a tagged union if memory warrants it. Ordinary field-by-field validation is sufficient; there is no architecture rule about inactive union bytes or struct padding.

The public snapshot is a coherent value copy:

```c
typedef struct
{
    MachineState state;
    MachineFault fault;
    FaultDetail fault_detail;
    int32_t target_pressure_n;
    int32_t pressure_n;
    int32_t current_ma;
    bool pressure_valid;
    bool current_valid;
    MachineCommandResult last_command_result;
} MachineSnapshot;
```

Snapshot publication may use a short critical section or double buffer. Readers never become writers of machine truth.

## MotorExecutor

MotorExecutor is the only future motor-hardware owner. It has a small synchronous interface that can be replaced by a fake in host tests:

```c
typedef enum
{
    MOTOR_DIRECTION_PRESS = 0,
    MOTOR_DIRECTION_RELEASE
} MotorDirection;

typedef enum
{
    MOTOR_RESULT_OK = 0,
    MOTOR_RESULT_INVALID,
    MOTOR_RESULT_BUSY,
    MOTOR_RESULT_TIMER_ERROR,
    MOTOR_RESULT_HARDWARE_ERROR
} MotorResult;

typedef struct
{
    bool disabled;
    bool idle;
    MotorResult last_result;
} MotorStatus;

MotorResult MotorExecutor_Disable(void);
MotorResult MotorExecutor_StartRun(MotorDirection direction,
                                   uint32_t command_mv,
                                   uint32_t stop_after_ms,
                                   uint32_t hard_stop_after_ms);
MotorResult MotorExecutor_RenewRun(uint32_t stop_after_ms);
MotorResult MotorExecutor_StartPulse(MotorDirection direction,
                                     uint32_t command_mv,
                                     uint32_t duration_us,
                                     uint32_t backstop_us);
MotorStatus MotorExecutor_GetStatus(void);
```

Required behavior:

- `Disable` is idempotent: shutdown outputs are asserted, PWM compares become zero, motion timers are disarmed, and any pending normal completion is invalidated before return.
- `StartRun` validates direction/command/times, arms the stop timer and any distinct hard timer, then and only then enables output.
- For approach, `stop_after_ms == hard_stop_after_ms`; one physical stop timer is enough.
- For jog, `RenewRun` may re-arm only the short keepalive timer and may never move the original hard stop.
- `StartPulse` arms duration and a strictly later backstop before output. Duration expiry disables then publishes `EVT_MOTOR_DONE`; backstop disables then publishes `TIMEOUT_PULSE`.
- Any validation, timer, or hardware failure returns an error with output disabled.
- Motion timeout output-off occurs in the hardware timer/ISR path; event publication is secondary.
- MotorExecutor never reads `MachineContext`, pressure, UI, or Modbus data.

The status needed by reset is only `disabled`, `idle`, and a diagnostic result. There are no request/cycle IDs, disabled-interval generations, or evidence publication handshakes. A single operation can be active; disable/cancel clears its pending descriptor before a later operation may start.

## Time and timer handling

Configured intervals are validated as nonzero where required and shorter than half the `uint32_t` clock range. Software elapsed checks use the standard bounded-interval expression:

```c
static inline bool MachineTime_ElapsedAtLeast(uint32_t now_ms,
                                              uint32_t started_ms,
                                              uint32_t interval_ms)
{
    return (uint32_t)(now_ms - started_ms) >= interval_ms;
}
```

This is enough for the controller's short physical intervals. There is no generic family of wrap comparison, interval-sum, conversion-result, or deadline-policy types. Pulse safety stays in the executor's native microsecond timer domain; any unit conversion used for display cannot arm output.

The runtime timer service owns at most one active instance of each machine timer kind: settle, pressure feedback, and automatic cycle. It publishes one `EVT_TIMER_TIMEOUT(kind, deadline)` and removes the timer. State exit cancels the timer and removes a queued instance before a replacement is armed. The handler checks state, kind, and stored deadline; an irrelevant timeout is ignored and logged.

## Runtime input handling

The control loop uses a small bounded input queue with three priorities:

1. safety (`EVT_OVERPRESSURE`, `EVT_OVERCURRENT`, motion timeouts, pressure/motor/internal faults);
2. global `CMD_STOP`;
3. normal FIFO commands/events.

Producers do not need acknowledgement state. If a safety event cannot be queued, its producer still performs any immediate shutdown it owns and raises an internal fault flag for the next control pass. Pressure samples may be coalesced to the newest sample only when the raw safety check has already run for every received frame.

The machine handles one selected input synchronously:

```text
const input + current MachineContext + current time
    -> validate guard
    -> call MotorExecutor if required
    -> on success update MachineContext once
    -> on executor failure disable and enter FAULT
    -> publish command result/snapshot
```

There is no separate decision-effect graph or action-outcome interpreter. Pure helpers may classify pressure or validate configuration, but the transition function remains the only context writer.

## UI boundary

UI gestures map directly to semantic commands. UI pages and button codes are presentation details, not machine states. The UI may render `MachineSnapshot` and show a command result; it may not write state, PWM, shutdown pins, timers, or auto/jog fields.

## Modbus semantic mapping

The retained register/address constants remain protocol compatibility points. RTU station, function, address, value encoding, length, and CRC are validated before mapping.

### Reads

| Function/address | Result from one snapshot |
| --- | --- |
| 0x01 / coil 0x0000 | Deprecated operating-mode concept: always `0`. |
| 0x01 / coil 0x0001 | `1` in AUTO_APPROACH, AUTO_SETTLE, AUTO_PULSE, or AUTO_HOLD; otherwise `0`. |
| 0x01 / coils 0x0002 and 0x0003 | Retained direct RELEASE/PRESS write aliases; readback remains unavailable (`0`). |
| 0x01 / coil 0x0004 | `1` only in JOG_RELEASE. |
| 0x01 / coil 0x0005 | `1` only in JOG_PRESS. |
| 0x03 / holding 0x0000 | Validated target pressure encoding when approved. |
| 0x04 / input 0x0000 | Current valid control pressure encoding, or the approved unavailable representation. |
| 0x04 / input 0x0012 | Last MotorExecutor failure result. |
| 0x04 / input 0x0013 | Last MotorExecutor failure stage. |

### Writes

| Function/address/value | Mapping |
| --- | --- |
| 0x05 / coil 0x0001 / ON | `CMD_AUTO_START` |
| 0x05 / coil 0x0001 / OFF | Global `CMD_STOP` |
| 0x05 / coil 0x0002 / ON | `CMD_DIRECT_RELEASE_PULSE` in ScopeTest/RealBench; unsupported in Locked/RealCompileCheck |
| 0x05 / coil 0x0003 / ON | `CMD_DIRECT_PRESS_PULSE` in ScopeTest/RealBench; unsupported in Locked/RealCompileCheck |
| 0x05 / coil 0x0002 or 0x0003 / OFF | Invalid value; a direct pulse is stopped with global STOP, never by an OFF alias |
| 0x05 / coil 0x0004 / ON | `CMD_JOG_RELEASE` (start or natural keepalive) |
| 0x05 / coil 0x0004 / OFF | `CMD_JOG_STOP` |
| 0x05 / coil 0x0005 / ON | `CMD_JOG_PRESS` (start or natural keepalive) |
| 0x05 / coil 0x0005 / OFF | `CMD_JOG_STOP` |
| 0x06 / holding 0x0000 | `CMD_SET_TARGET(value)` |
| operating-mode coil | Return unsupported; publish no machine command |
| second target, runtime station ID, runtime baud rate | Return unsupported until separately designed |

ON is standard `0xFF00` and OFF is `0x0000`; other values are protocol errors. A successful frame parse does not imply command acceptance. The response reports `MachineCommandResult` for the originating transaction.

The prohibited legacy path remains:

```text
Modbus write -> fake UI key -> parallel state -> motor
```

The only allowed path is:

```text
Modbus write -> semantic command -> machine -> bounded MotorExecutor request
```

ScopeTest and RealBench require an explicit build acknowledgement. They enable only the two fixed direct-pulse commands and disable automatic closed-loop start; RealBench additionally requires live pressure safety. Neither mode is a production motor approval.

## Minimal Phase 2B file shape

A reasonable initial implementation needs only a small set of responsibilities:

```text
Application/Machine/machine_types.h
Application/Machine/machine.h
Application/Machine/machine.c
Application/Machine/machine_snapshot.c
Board/Motor/motor_executor.h
Board/Motor/motor_executor.c
Control/Pressure/pressure_control.c
Control/Current/current_safety.c
Runtime/runtime_loop.c
Runtime/runtime_timers.c
Transport/Modbus/modbus_semantic_map.c
Tests/Host/fake_motor_executor.c
Tests/Host/test_machine_core.c
```

This is a responsibility sketch, not an instruction to create these files in this task. Split a file only when measurements, ownership, or testability justify it.
