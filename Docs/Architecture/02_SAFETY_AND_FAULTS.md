# Safety and faults

## Fault classes

```c
typedef enum
{
    FAULT_NONE = 0,
    FAULT_BOOT_FAULT,
    FAULT_PRESSURE_SENSOR_FAULT,
    FAULT_OVERPRESSURE,
    FAULT_OVERCURRENT,
    FAULT_MOTION_TIMEOUT,
    FAULT_MOTOR_FAULT,
    FAULT_INTERNAL_FAULT
} MachineFault;
```

`FAULT_NONE` is not included in the count.

```text
FAULT_COUNT=7
```

One secondary enum is enough for debugging without expanding control policy:

```c
typedef enum
{
    FAULT_DETAIL_NONE = 0,
    FAULT_DETAIL_BOOT_CLOCK,
    FAULT_DETAIL_BOOT_CONFIGURATION,
    FAULT_DETAIL_PRESSURE_TIMEOUT,
    FAULT_DETAIL_PRESSURE_INVALID,
    FAULT_DETAIL_PRESSURE_ORDER_LOST,
    FAULT_DETAIL_APPROACH_TIMEOUT,
    FAULT_DETAIL_PULSE_TIMEOUT,
    FAULT_DETAIL_AUTO_CYCLE_TIMEOUT,
    FAULT_DETAIL_MOTOR_REQUEST_REJECTED,
    FAULT_DETAIL_MOTOR_TIMER,
    FAULT_DETAIL_MOTOR_HARDWARE,
    FAULT_DETAIL_INTERNAL_CPU,
    FAULT_DETAIL_INTERNAL_STATE,
    FAULT_DETAIL_INTERNAL_QUEUE,
    FAULT_DETAIL_DIRECT_PULSE_TIMEOUT
} FaultDetail;
```

| Fault | Examples recorded as diagnostic detail | Required reaction |
| --- | --- | --- |
| `FAULT_BOOT_FAULT` | Clock, initialization, configuration, required service unavailable | Disable and remain out of IDLE until boot checks pass |
| `FAULT_PRESSURE_SENSOR_FAULT` | Required sample timeout, invalid frames, lost calibration, producer restart/order loss | Disable and fault in JOG_PRESS or any AUTO state |
| `FAULT_OVERPRESSURE` | Raw pressure at/above approved safety threshold | Immediate disable before filter/control logic |
| `FAULT_OVERCURRENT` | Qualified current at/above approved limit | Immediate disable |
| `FAULT_MOTION_TIMEOUT` | Approach, automatic pulse backstop, direct-pulse backstop, or automatic-cycle maximum | Timer/ISR disables first, then machine faults |
| `FAULT_MOTOR_FAULT` | Executor rejected start, timer arm failure, register self-check, bridge/driver fault | Disable and fault |
| `FAULT_INTERNAL_FAULT` | CPU exception, invalid enum/state, queue corruption, impossible transition | Immediate/best available disable and fault |

Detailed causes belong in this small `FaultDetail` field or a bounded diagnostic log. They do not create more machine fault states, fault occurrence identities, secondary-fault unions, or reset evidence protocols. If another fault arrives while FAULT is already active, output is disabled again and the last diagnostic detail may be updated; the active fault class is not needed to decide motor safety because every fault has the same immediate reaction.

Every motor-originated fault is published as `FAULT_MOTOR_FAULT` with a nonzero motor detail. The MotorExecutor snapshot additionally exposes the exact `MotorResult` and `MotorFailureStage`; diagnostics never bypass Machine ownership.

Jog keepalive and jog maximum-duration expiry are protective stops, not machine faults. MotorExecutor disables output and the machine returns to IDLE with a stop detail. Automatic approach, pulse, and cycle expiry are faults because the requested process did not complete within its approved bound.

## Immediate disable rule

The common fault sequence is:

```text
detect qualified fault
    -> MotorExecutor_Disable() or exception-safe immediate disable
    -> cancel motion and runtime timers
    -> clear transient jog/auto context
    -> store MachineFault + FaultDetail + timestamp
    -> enter FAULT
    -> publish snapshot / best-effort diagnostic
```

Output-off does not wait for logging, queue space, state commit, UI, Modbus, or the cooperative runtime. Executor motion timeouts disable directly from the hardware timer/ISR boundary before publishing a timeout event.

During an accepted request only, MotorExecutor's hardware layer starts and verifies both PWM channels at zero compare while SD1/SD2 are low, raises both driver enables, and writes the selected direction compare last. Both carriers may therefore be enabled and running, but TIM2 CCR3 and TIM3 CCR3 must never both be nonzero. Any start, zero-carrier, driver-enable, or final-compare verification failure immediately returns to complete force-disable.

The disabled invariant is unchanged at boot and after STOP, normal completion, backstop, timer error, or fault: SD1/SD2 low, both CCR3 values zero, both CC3E bits clear, and both CEN bits clear. The dual-carrier active state is not used as a stop or brake state. This sequence responds to field-isolated U6/U7 output behavior and requires motor-disconnected electrical re-verification; it does not claim the physical issue is solved.

## Pressure safety

The pressure path stays split:

```text
sensor frame -> decode/units -> raw sample --+--> raw overpressure check --> EVT_OVERPRESSURE
                                             |
                                             `--> validity/filter --> control pressure
                                                                    --> contact/hold/pulse decisions
```

The raw branch executes first and cannot be suppressed by filter rejection, averaging, hold logic, or a planner. The approved raw limit and reset hysteresis remain calibration TODOs; no legacy number is automatically accepted.

Live valid pressure is required in:

- `JOG_PRESS`;
- `AUTO_APPROACH`;
- both phases of `AUTO_SETTLE`;
- `AUTO_PULSE`;
- `AUTO_HOLD`.

RealBench direct PRESS/RELEASE pulses also require fresh valid pressure and fault on pressure loss while active. ScopeTest direct pulses deliberately do not require pressure because that mode exists for disconnected, low-energy waveform inspection. Both remain bounded by the same MotorExecutor timer/ISR shutdown.

A timeout or invalid required stream in those states becomes `FAULT_PRESSURE_SENSOR_FAULT`. IDLE uses the same validity/freshness checks to reject a start without latching a fault. `JOG_RELEASE` may continue without live pressure because release reduces pressing force, but any available raw overpressure indication still disables and faults.

After contact or pulse motion, control follows this exact physical order:

1. disable the motor;
2. enter `AUTO_SETTLE/SETTLE_WAIT_DELAY` and wait the calibrated settle duration;
3. capture the latest processed pressure sequence when the settle timer expires;
4. accept only a later sample from the ordered pressure producer;
5. choose HOLD, PULSE, or RECONTACT.

Samples received during the delay may update safety/cache state but cannot cause a normal control decision. Pressure service is ordered before timer service, so already queued samples are consumed before the sequence gate is captured. A producer reset/order loss is a pressure fault, not a silent sequence restart.

## Current safety

Current acquisition produces calibrated samples with explicit validity. The current safety service owns debounce/qualification and publishes `EVT_OVERCURRENT`; UI, Modbus, and the state machine do not compare raw ADC counts to a guessed limit. Overcurrent disables from every motion state and may also latch in non-motion states when the safety service considers the reading meaningful.

The shunt/INA240 gain, polarity, offset, ADC reference, sample rate, bandwidth, threshold, and debounce are unresolved until measured.

## Motor timeout safety

Only MotorExecutor may enable motion output. Before nonzero output it must successfully arm all required hardware timers:

| Motion | Required independent bounds |
| --- | --- |
| Jog press/release | Renewable keepalive deadline plus a separately armed nonrenewable maximum duration |
| FIRST approach | Approach timeout |
| RECONTACT approach | Separately calibrated approach timeout |
| Correction pulse | Normal pulse-duration timer plus a later fail-safe backstop |

The automatic cycle also has a cooperative overall-cycle timer. It is not a substitute for MotorExecutor's hardware stop timer.

MotorExecutor behavior on any motion timer expiry is simple: set PWM compares to zero, disable the relevant outputs, disarm motion timers, then publish the completion/timeout. A full event queue may lose a report but may not delay output-off.

## Fault reset

`CMD_FAULT_RESET` is considered only in FAULT. The command source must already have passed whatever UI/service authorization policy the product adopts; authorization is an ingress responsibility, not a three-part machine evidence object.

Reset uses one current read-only `SafetyStatus` assembled from the actual subsystem owners:

- MotorExecutor reports disabled and idle;
- the source corresponding to the active fault reports clear/healthy;
- required boot/configuration checks are able to run again.

```c
typedef struct
{
    bool motor_disabled;
    bool motor_idle;
    bool boot_checks_ready;
    bool pressure_source_healthy;
    bool overpressure_clear;
    bool overcurrent_clear;
    bool motor_source_healthy;
    bool internal_reset_allowed;
} SafetyStatus;
```

`Safety_CanReset(active_fault, status)` checks only the fields relevant to the active fault plus the common motor-disabled/idle requirement. It reads current subsystem status; it does not preserve or intersect historical qualifications.

If those conditions are false, the machine remains FAULT and disabled. If true:

```text
FAULT --CMD_FAULT_RESET / source clear and motor disabled--> BOOT_SAFE
BOOT_SAFE --complete self-check--> IDLE
```

Reset clears transient jog/auto context and the active fault after entering BOOT_SAFE. It does not restore a prior command, owner, timer, pulse, approach profile, or automatic-cycle position. Fault history may be a simple bounded diagnostic log outside control decisions.

Internal/CPU faults may require a physical reboot instead of accepting runtime reset. `Safety_CanReset()` decides that from the current fault class and health status; it does not use stored freshness windows, occurrence IDs, or producer generations.

## Core invariants

These are the complete V1 architecture invariants.

| ID | Invariant | Verification |
| --- | --- | --- |
| `INV-01` | There is one authoritative `MachineState`. | Structure/static audit and transition tests |
| `INV-02` | Only the machine module writes `MachineContext`. | Dependency/static audit |
| `INV-03` | Only MotorExecutor accesses motor-control hardware. | Include/register scan and fake-executor tests |
| `INV-04` | BOOT_SAFE, IDLE, AUTO_SETTLE, AUTO_HOLD, COMPLETE, and FAULT keep the motor disabled. | State/action tests |
| `INV-05` | STOP is accepted in every state and disables before any transition or diagnostic. | Parameterized STOP test |
| `INV-06` | Every fault disables before state/diagnostic work. | Fault injection and ordered fake-call assertions |
| `INV-07` | Every nonzero motor action has an independently enforced MotorExecutor timeout; jog also has a nonrenewable maximum. | Timer/fake-register tests |
| `INV-08` | Raw overpressure bypasses filtered control and stops immediately. | Dual-path pressure test |
| `INV-09` | Loss or invalidity of required pressure feedback stops/faults motion. | Required-state pressure tests |
| `INV-10` | After contact/pulse motion, output is off for settle delay and a post-delay pressure sequence is required before another control action. | Settle gate tests |
| `INV-11` | UI and Modbus publish semantic commands and read snapshots; they never write PWM or state. | Dependency and mapping tests |
| `INV-12` | Fault reset returns through BOOT_SAFE and never resumes prior motion. | Reset test |

```text
INVARIANT_COUNT=12
```

An invariant violation maps to `FAULT_INTERNAL_FAULT` and immediate disable. There are no invariants whose sole purpose is maintaining generations, acknowledgements, canonical padding, or fault-record bookkeeping.

## Open architecture decisions

These are the major unresolved design/hardware decisions. Closing one requires dated evidence for the applicable board and firmware revision.

| ID | Decision | Evidence needed | Safe interim position |
| --- | --- | --- | --- |
| `AD-01` | Motor electrical enable, direction, command-to-duty envelope, and SD1/SD2 fail-safe disposition | Schematic/PCB review, external pull-down decision, low-energy direction test, scope/current/thermal measurements | No real motor enable |
| `AD-02` | Pressure safety architecture and sensor validity | Sensor units/calibration, rate/latency, disconnect/corrupt tests, raw limit/reset hysteresis review | No pressure-controlled motion |
| `AD-03` | Overcurrent safety architecture | Shunt/INA240/ADC calibration, bandwidth, polarity, threshold/debounce and trip-energy tests | Overcurrent readiness false |
| `AD-04` | Approved motor command and timing envelope | Plant/tooling trials for jog, approach, pulse, settle, hold, and overall cycle plus scope-verified stop timing | All motion values unapproved |
| `AD-05` | Position/limit sensing availability and safety role | Installed sensor proof, wiring, direction, homing, travel envelope, plausibility and missed-signal tests | No one-touch or position control |
| `AD-06` | Future dynamic-brake support | Bridge truth table, current path visibility, energy and thermal validation | Force-disable only; no brake command |
| `AD-07` | Watchdog strategy | Critical-service list, heartbeat budget, reset handling, and fault persistence policy | Do not add a feed site yet |
| `AD-08` | Product emergency-stop architecture | Product risk assessment, required category, wiring/channels, diagnostics, and validation plan | Firmware STOP is not claimed as a hardware E-stop |

```text
OPEN_ARCHITECTURE_DECISION_COUNT=8
```

## Calibration and implementation TODOs

The following are ordinary calibration/configuration work, not separate architecture decisions. No values are selected here:

- target pressure range and encoding;
- contact threshold;
- raw overpressure trip/reset thresholds;
- pressure freshness/invalid-frame policy;
- current trip and debounce;
- jog press/release commands, keepalive, and maximum duration;
- FIRST and RECONTACT command/timeout profiles;
- settle duration and post-settle feedback timeout;
- pulse command, duration choices, backstop margin, and maximum correction count;
- hold-entry/exit bands, minimum time, and minimum distinct-sample count;
- overall automatic-cycle timeout;
- filter parameters and sensor sample rates.

Each value needs units, bounds, evidence, applicable hardware revision, and tests. Legacy constants are observations, not approved defaults.
