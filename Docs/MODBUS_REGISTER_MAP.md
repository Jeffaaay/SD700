# Active Modbus RTU contract

This document describes the current source in the four base build modes plus the explicit RealBench AutoTarget option. Legacy addresses `0x0002` and `0x0003` are retained as aliases, but their current meaning is a mode-restricted, fixed direct pulse—not legacy one-touch product motion.

## Transport

```text
UART=USART2
TX=PA2
RX=PA3
DE_RE=PA1
BAUD=115200
FORMAT=8N1
STATION_ID=1
CRC_ORDER=LOW_BYTE_FIRST
```

PA1 is low while receiving, high only during interrupt-driven response transmission, and low again after transmit completion or error. The bounded ISR-to-main SPSC receiver accepts fragmented requests and multiple frames. CRC-valid frames for other stations are ignored.

Requests supported by the current server are eight bytes. Four slots hold normal requests. Coil `0x0001` OFF has a separate priority latch: when taken, it disables the Machine and discards all older queued normal requests before its echo is published. No other coil/value is STOP.

## Functions and exceptions

| Function | Name | Active behavior |
| ---: | --- | --- |
| `0x03` | Read Holding Registers | Contiguous reads from the holding map |
| `0x04` | Read Input Registers | Contiguous reads; maximum 11 registers per request |
| `0x05` | Write Single Coil | Automatic START, priority STOP, or mode-restricted direct pulse |
| `0x06` | Write Single Register | Target pressure only |

Unsupported functions return exception `0x01`. Invalid addresses and mode-disabled commands return `0x02`. Invalid quantity, coil encoding, direct-pulse OFF, or target value returns `0x03`. Other Machine admission or executor failures return `0x04`.

## Holding register

| Address | Meaning | Access |
| ---: | --- | --- |
| `0x0000` | `target_pressure_units` | Read/write in `IDLE`; units remain uncalibrated raw sensor-count control units |

A successful `0x06` write echoes the request. No other holding-register address is implemented.

## Coils

| Address | Value | Current semantic meaning | Mode behavior |
| ---: | ---: | --- | --- |
| `0x0001` | `0xFF00` | `CMD_AUTO_START` | Accepted in Locked and explicit RealBench AutoTarget when admission checks pass; rejected in RealCompileCheck, ScopeTest, and ordinary RealBench |
| `0x0001` | `0x0000` | Priority `CMD_STOP` | The only priority STOP encoding; accepted in every mode/state and never clears a latched fault |
| `0x0002` | `0xFF00` | Legacy-address alias for `CMD_DIRECT_RELEASE_PULSE` | Enabled only in ScopeTest and RealBench; rejected in Locked and RealCompileCheck |
| `0x0003` | `0xFF00` | Legacy-address alias for `CMD_DIRECT_PRESS_PULSE` | Enabled only in ScopeTest and RealBench; rejected in Locked and RealCompileCheck |
| `0x0002` or `0x0003` | `0x0000` | Invalid value | Not STOP and never stops an active direct pulse |
| `0x0004` or `0x0005` | any | Jog/fast legacy address | Unsupported in every mode |

The GuardFixV5 direct-pulse test profile is fixed at 10000 mV for 100 ms with a 150 ms TIM5 backstop; it is not a production pressure-holding profile. The direct aliases never bypass Machine or MotorExecutor. Values other than `0xFF00` and `0x0000` are invalid coil encodings; for `0x0002`/`0x0003`, OFF is also deliberately invalid.

Coil `0x0000` remains unsupported. Coil reads do not provide direct-pulse state readback.

## Input registers

The complete readable input range is `0x0000` through `0x0024`, for a total count of `0x0025` (37) registers. A single `0x04` request is still limited to 11 contiguous registers by `MODBUS_RTU_MAX_READ_REGISTERS`.

| Address | Meaning | Encoding |
| ---: | --- | --- |
| `0x0000` | Current `control_pressure_units` | `0xFFFF` when unavailable |
| `0x0001` | `raw_pressure_counts` | `0xFFFF` when unavailable |
| `0x0002` | `MachineState` | Numeric enum from `Application/machine.h` |
| `0x0003` | `MachineFault` | Numeric enum |
| `0x0004` | `FaultDetail` | Numeric enum |
| `0x0005` | Last `MachineCommandResult` | Numeric enum |
| `0x0006` | Last `MotorAction` | Numeric enum from `Board/Motor/motor_executor.h` |
| `0x0007` | Last motor `command_mv` | Saturated to `uint16_t` |
| `0x0008` | Planned TIM2 CCR3 | Current executor snapshot |
| `0x0009` | Planned TIM3 CCR3 | Current executor snapshot |
| `0x000A` | Status flags | Bit field below |
| `0x000B`–`0x0011` | Last valid pressure-frame bytes 0–6 | One zero-extended byte per register |
| `0x0012` | Last motor failure result | Numeric `MotorResult`; `0` means `MOTOR_RESULT_OK` |
| `0x0013` | Last motor failure stage | Numeric `MotorFailureStage`; table below |

Appended diagnostics (all modes readable; old addresses unchanged):

| Address | Meaning | Encoding |
| --- | --- | --- |
| `0x0014`–`0x0017` | Device pressure sample sequence | uint64, most significant word first |
| `0x0018`–`0x0019` | Pressure received_at_ms | uint32, high word first; wraps |
| `0x001A` | Last accepted request direction | 0=PRESS, 1=RELEASE; meaningful when request sequence nonzero |
| `0x001B` | Last accepted request command | mV request, not measured voltage; retained through STOP/settle |
| `0x001C` | Last accepted request duration | ms request, retained through STOP/settle |
| `0x001D`–`0x001E` | Last accepted request sequence | uint32 high word first |
| `0x001F`–`0x0020` | Last accepted request start time | uint32 device ms, high word first |
| `0x0021` | Target readback | Same as holding `0x0000` |
| `0x0022` | AutoTarget capability/version | `0xA701` for opt-in V1; zero otherwise; not a binary hash |
| `0x0023`–`0x0024` | Device now_ms for this read | uint32 high word first |

One FC04 response is assembled in main context; the Machine sample is not
modified by the pressure ISR during response assembly. Across multiple reads,
callers must check sample/request identities before and after the other reads.
The static capture script excludes inconsistent rows from metrics and uses
sequence changes rather than PC poll counts to identify new pressure frames.
Intermediate frames/requests can still be missed. Diagnostics cannot start motion.

Status flags:

| Bit | Meaning |
| ---: | --- |
| 0 | Pressure is valid and fresh |
| 1 | Target is valid |
| 2 | Logical motor request is active |
| 3 | Physical output is compile-time locked |
| 4 | Physical output is disabled |
| 5 | Bench raw-count control mode is active |
| 6 | Bench release correction is enabled |

## MotorFailureStage values

These numeric values are append-only diagnostics exposed at input `0x0013`.

| Value | Symbol |
| ---: | --- |
| 0 | `MOTOR_FAILURE_STAGE_NONE` |
| 1 | `MOTOR_FAILURE_STAGE_HW_INIT_TIM2_PWM` |
| 2 | `MOTOR_FAILURE_STAGE_HW_INIT_TIM2_CHANNEL` |
| 3 | `MOTOR_FAILURE_STAGE_HW_INIT_TIM3_PWM` |
| 4 | `MOTOR_FAILURE_STAGE_HW_INIT_TIM3_CHANNEL` |
| 5 | `MOTOR_FAILURE_STAGE_HW_INIT_CONFIG_VERIFY` |
| 6 | `MOTOR_FAILURE_STAGE_HW_INIT_DISABLED_VERIFY` |
| 7 | `MOTOR_FAILURE_STAGE_HW_APPLY_NOT_INITIALIZED` |
| 8 | `MOTOR_FAILURE_STAGE_HW_APPLY_INVALID_DUTY` |
| 9 | `MOTOR_FAILURE_STAGE_HW_APPLY_ARMING_DENIED` |
| 10 | `MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM2` |
| 11 | `MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM3` |
| 12 | `MOTOR_FAILURE_STAGE_HW_APPLY_PRESTART_VERIFY` |
| 13 | `MOTOR_FAILURE_STAGE_HW_APPLY_PWM_START` |
| 14 | `MOTOR_FAILURE_STAGE_HW_APPLY_POSTSTART_VERIFY` |
| 15 | `MOTOR_FAILURE_STAGE_HW_APPLY_SD_ENABLE_VERIFY` |
| 16 | `MOTOR_FAILURE_STAGE_HW_DISABLE_VERIFY` |
| 17 | `MOTOR_FAILURE_STAGE_TIMER_INIT_NULL_HANDLER` |
| 18 | `MOTOR_FAILURE_STAGE_TIMER_INIT_CLOCK_INVALID` |
| 19 | `MOTOR_FAILURE_STAGE_TIMER_ARM_PRECONDITION` |
| 20 | `MOTOR_FAILURE_STAGE_TIMER_ARM_RANGE` |
| 21 | `MOTOR_FAILURE_STAGE_TIMER_ARM_START_VERIFY` |
| 22 | `MOTOR_FAILURE_STAGE_TIMER_COMMIT_PRECONDITION` |
| 23 | `MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT` |
| 24 | `MOTOR_FAILURE_STAGE_TIMER_IRQ_NOT_ARMED` |
| 25 | `MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE` |
| 26 | `MOTOR_FAILURE_STAGE_TIMER_IRQ_UNKNOWN_STATUS` |
| 27 | `MOTOR_FAILURE_STAGE_TIMER_IRQ_MISSING_HANDLER` |
| 28 | `MOTOR_FAILURE_STAGE_EXECUTOR_INIT_OUTPUT_NOT_DISABLED` |
| 29 | `MOTOR_FAILURE_STAGE_EXECUTOR_START_PRECONDITION` |
| 30 | `MOTOR_FAILURE_STAGE_EXECUTOR_START_OUTPUT_ARMING` |
| 31 | `MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_ARM` |
| 32 | `MOTOR_FAILURE_STAGE_EXECUTOR_START_HW_APPLY` |
| 33 | `MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_COMMIT` |
| 34 | `MOTOR_FAILURE_STAGE_EXECUTOR_COMPLETION_OUTPUT_NOT_DISABLED` |
| 35 | `MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_UNHEALTHY` |
| 36 | `MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_NOT_ARMED` |
| 37 | `MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_LOGICAL_BACKSTOP` |
| 38 | `MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_OUTPUT_DROPPED` |
| 39 | `MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_ACTIVE_REQUEST_INVALID` |
| 40 | `MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT` |
| 41 | `MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_TIMER` |
| 42 | `MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_OUTPUT_NOT_DISABLED` |
| 43 | `MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_TIMER_STILL_ARMED` |
| 44 | `MOTOR_FAILURE_STAGE_UNSPECIFIED` |
| 45 | `MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_COMPARE_COMMIT` |
| 46 | `MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM2_ZERO_CARRIER` |
| 47 | `MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM3_ZERO_CARRIER` |
| 48 | `MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_CARRIER_VERIFY` |
| 49 | `MOTOR_FAILURE_STAGE_HW_APPLY_DRIVER_ENABLE_VERIFY` |
| 50 | `MOTOR_FAILURE_STAGE_HW_APPLY_ACTIVE_COMPARE_VERIFY` |

## Exact RTU examples

CRC is transmitted low byte first.

```text
RELEASE direct ON: 01 05 00 02 FF 00 2D FA
PRESS direct ON:   01 05 00 03 FF 00 7C 3A
Priority STOP:     01 05 00 01 00 00 9C 0A
```

Successful writes echo the complete eight-byte request. The direct commands are bench diagnostics only: ScopeTest requires motor power physically disconnected, and RealBench remains prohibited without separate powered-test authorization.
