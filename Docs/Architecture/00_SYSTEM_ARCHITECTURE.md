# SD700 system architecture

## Status and scope

This directory describes the minimal V1 firmware architecture. It is design documentation only. It does not add a production state machine, MotorExecutor, host-test source, or any motor-enable path.

```text
ARCHITECTURE_STATUS=DESIGN_ONLY
ARCHITECTURE_DOCUMENT_COUNT=5
STATE_COUNT=12
FAULT_COUNT=7
COMMAND_COUNT=10
EVENT_COUNT=11
INVARIANT_COUNT=12
HOST_LOGICAL_SCENARIO_COUNT=25
HOST_EXECUTABLE_COUNT=23
READY_FOR_MINIMAL_PHASE2B=YES
READY_TO_ENABLE_MOTOR=NO
```

Phase 2B may implement domain types, a pure state machine, a fake MotorExecutor, and host tests. Real output remains prohibited until the hardware gates below are closed.

## First-principles design

The controller has one job: accept semantic operator commands, move only through a bounded motor interface, use pressure feedback to control an automatic press cycle, and stop safely whenever required feedback or execution guarantees are lost.

The architecture therefore keeps only these boundaries:

```text
pressure bytes -> decode -> raw safety --------------------.
                         -> filter/control sample ----------+---> machine
current ADC ----> qualify -> overcurrent safety ------------'

UI / Modbus -----------> semantic command -----------------> machine
timers / MotorExecutor -> completion or timeout -----------> machine

machine -> bounded motor request -> MotorExecutor -> motor hardware
machine -> immutable snapshot ----> UI / Modbus / diagnostics
```

The rules are deliberately small:

- `MachineContext` contains the one authoritative state and is written only by the machine module.
- The machine handles commands and events synchronously in one cooperative control loop.
- Only MotorExecutor may access motor PWM, direction, shutdown pins, or motion timers.
- STOP and every fault call the disable path before changing state or logging.
- Every nonzero motor action has an independently enforced executor timeout.
- Pressure raw safety is evaluated before filtering; filtered pressure is used only for normal control.
- UI and Modbus publish semantic commands and read snapshots. They never own machine state.

## Supported V1 behavior

| Capability | V1 behavior |
| --- | --- |
| Safe boot | Output is disabled before HAL/clock initialization. Boot checks must pass before IDLE. |
| IDLE | Motor disabled; accepts target, jog, and automatic-cycle commands. |
| Jog press/release | Repeated direction commands act as keepalive; both a keepalive limit and a nonrenewable maximum duration bound motion. |
| Automatic press | First approach, contact, settle, fresh sample, correction pulse, re-contact, hold, and complete. |
| STOP | Available in every state; motion states return to IDLE after disabling. A fault remains latched. |
| Fault reset | Rechecks the current fault source and motor-disabled status, then returns through BOOT_SAFE. It never resumes prior motion. |
| Pressure safety | Missing/invalid required feedback faults motion; raw overpressure bypasses the filter. |
| Current safety | Qualified overcurrent disables and faults. |
| Motion timeout | MotorExecutor stops output without waiting for the cooperative loop. |
| Semantic interfaces | UI and retained Modbus addresses map to the ten commands in `01_STATE_MACHINE.md`. |
| Protected direct pulses | ScopeTest and RealBench alone accept fixed, bounded PRESS/RELEASE pulses through Machine and MotorExecutor. Automatic control is disabled in every real-output build. |

### Real-output start and stop invariant

An accepted real-output request crosses one controlled hardware boundary: force-disable first; commit zero compare to TIM2_CH3 and TIM3_CH3; start and verify both zero-duty carriers with SD1/SD2 low; enable both drivers; then write exactly one nonzero directional CCR. PRESS selects TIM3 and RELEASE selects TIM2. Both PWM channels run during the active request, so direction safety is defined by no more than one nonzero CCR, not by only one enabled timer.

Boot, STOP, completion, backstop, timer error, and every fault use the unchanged complete force-disable path: both SD pins low, both CCRs zero, both channel enables clear, and both timer counters stopped. No active-request carrier state persists as braking.

This ordering change is based on field isolation of U6/U7 driver-output behavior and still requires a motor-disconnected hardware re-test. It is not evidence that the root cause or motor motion has been resolved.

## Explicit non-capabilities

V1 does not include:

- production one-touch/product-cycle motion;
- PID or position control;
- encoder-based travel control;
- dynamic braking;
- a FreeRTOS application architecture;
- production-approved real motor enable.

Legacy coils `0x0002` and `0x0003` are retained as protected direct RELEASE/PRESS pulse commands. They are accepted only in explicitly acknowledged ScopeTest or RealBench builds; Locked and RealCompileCheck return unsupported. These bench pulses use fixed Section 9A constants and do not run the automatic pressure controller. RealBench is not production approval; Section 9A performed no overshoot tuning or pressure calibration.

## Execution model

The initial implementation is a bounded cooperative loop:

1. service immediate hardware and CPU safety;
2. decode pressure and run raw overpressure safety before filtering;
3. qualify pressure/current and detect feedback faults;
4. service MotorExecutor completions and timeouts;
5. dispatch safety events, then STOP, then one normal input;
6. update the machine synchronously;
7. publish one coherent snapshot and bounded diagnostics;
8. feed the watchdog only when all required services are healthy.

There are only three input priorities:

```text
safety fault or motion timeout > STOP > normal FIFO input
```

FIFO order resolves ties within a priority. There is no 29-rank arbitration table. Work per iteration must be bounded, but a loop stall cannot sustain motor output because MotorExecutor owns the hardware stop timer.

## Module boundaries

| Module | Owns | Does not own |
| --- | --- | --- |
| `Application/Machine` | `MachineContext`, transitions, command results, auto/jog bookkeeping | Registers, transport framing, UI state |
| `Board/Motor` | MotorExecutor, PWM/direction/shutdown registers, motion timers, immediate disable | Pressure policy, machine state, Modbus/UI |
| `Control/Pressure` | Raw safety check, validation/filter, contact/hold/correction decisions | Motor hardware, machine mutation |
| `Control/Current` | Current calibration/qualification and overcurrent detection | Motor commands, machine mutation |
| `Transport/Pressure` | Nonblocking acquisition and frame decode | Filtering policy, state changes |
| `Transport/Modbus` | RTU framing, retained address mapping, response correlation | Jog/machine ownership, fake keys, direct motor access |
| `Runtime` | Bounded service order, timer service, three-priority dispatch, snapshot publication | Duplicate machine truth |
| `UI` | Gesture-to-command mapping and snapshot rendering | State/PWM writes |

If an RTOS is later justified, a single control task remains the only `MachineContext` writer. Other tasks pass immutable commands, events, and samples.

## Verified production baseline

The current production firmware remains Phase 1.1 safe-idle code:

- the first statement in `main()` is `MotorHw_EarlyForceDisable()`;
- PB1/SD1 and PB2/SD2 are driven to the disabled-low level with internal pull-downs;
- the low output value is latched before the pins become outputs;
- TIM2_CH3 and TIM3_CH3 compare registers are zero and both channel enables are cleared;
- the exception handlers call immediate force-disable;
- `SafeIdle_Run()` is non-returning;
- there is no production state machine, MotorExecutor, active enable, nonzero PWM, brake, or motion path.

The 2026-03-19 schematic identifies an STM32F411RCT6, 8 MHz HSE, PA4/ADC1_CH4, PB1/SD1, and PB2/SD2 for that board revision. The assembled controller has not been electrically validated. The legacy direction note says press/down used TIM3_CH3 duty with TIM2_CH3 zero, while release/up used TIM2_CH3 duty with TIM3_CH3 zero; this is historical evidence, not permission to enable output.

## Hardware-enable gates

Real motor motion remains blocked until all applicable items have dated evidence for the actual board, motor, bridge, supply, and firmware revision:

1. authoritative ARM build and the minimal host suite pass;
2. MotorExecutor fake-register and timeout tests pass;
3. press/release direction and bounded command-to-duty behavior are confirmed by a supervised low-energy test;
4. shutdown pins, PWM-off latency, reset/debug behavior, and the external SD1/SD2 fail-safe limitation are measured and accepted;
5. pressure calibration, sample rate, invalid/disconnected behavior, raw overpressure threshold, and reset hysteresis are approved;
6. current-sense calibration, polarity, bandwidth, threshold, debounce, and trip behavior are approved;
7. jog, approach, settle, pulse, hold, and overall-cycle timing are calibrated and motion/pulse timeout shutdown is measured on a scope;
8. watchdog and product emergency-stop requirements are resolved;
9. production one-touch/product-cycle motion remains blocked until independent travel/limit sensing is validated;
10. dynamic brake remains blocked until bridge current paths and thermal behavior are validated.

```text
HARDWARE_ENABLE_GATE_PASS_COUNT=0
READY_TO_ENABLE_MOTOR=NO
```

## Removed overengineering

The simplified design intentionally removes settle generations, fault-occurrence IDs, three-part reset evidence, publisher acknowledgement/retry protocols, secondary-fault unions, 29-way arbitration, request/cycle ID propagation, generic wrap-result APIs, whole-object canonicalization rules, separate post-settle sample state, and large declarative effect/action-outcome graphs.

What remains is kept because it directly protects the machine or makes ordinary debugging materially clearer: one state, one context owner, one motor owner, immediate disable, bounded motion, a raw pressure safety path, a post-settle sample gate, a simple fault/detail record, and immutable interface snapshots.

## Document map

- `01_STATE_MACHINE.md` defines states, commands, events, and transitions.
- `02_SAFETY_AND_FAULTS.md` defines fault reactions, invariants, reset, open architecture decisions, and calibration TODOs.
- `03_INTERFACES_AND_DATA.md` defines minimal data, ownership, MotorExecutor, runtime, UI, and Modbus boundaries.
- `04_HOST_TEST_PLAN.md` defines the 25-test V1 host suite.
