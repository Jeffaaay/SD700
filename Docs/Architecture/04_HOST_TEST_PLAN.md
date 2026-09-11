# Minimal V1 host test plan

## Purpose

The host suite tests physical/control behavior through a fake MotorExecutor and a deterministic clock. It does not test removed generations, evidence intersections, publisher acknowledgements, fault-occurrence records, compiler padding, or architecture bookkeeping.

Each test starts from a fresh fixture unless it explicitly builds a sequence. The fake records ordered calls, active direction/command, timer arms, output-disabled state, and injected failures. No host test enables real hardware.

```text
HOST_LOGICAL_SCENARIO_COUNT=25
HOST_EXECUTABLE_COUNT=23
```

## Core suite

| ID | Setup and stimulus | Expected state/result | Essential assertion |
| --- | --- | --- | --- |
| `HST-01` | Reset fixture and enter `BOOT_SAFE` | BOOT_SAFE | First motor call is disable; fake output and every motion timer are off before any boot service. |
| `HST-02` | BOOT_SAFE receives `EVT_BOOT_OK` with all checks passing | IDLE / accepted | Output remains disabled; transient jog/auto/fault data is clear. |
| `HST-03` | BOOT_SAFE receives `EVT_BOOT_FAULT` | FAULT / `FAULT_BOOT_FAULT` | Disable precedes fault commit; no run/pulse call occurs. |
| `HST-04` | IDLE has valid readiness; source A sends `CMD_JOG_PRESS`, then repeats it | JOG_PRESS / accepted twice | First command arms press run, keepalive, and hard maximum before output; repeat from A renews only keepalive. |
| `HST-05` | IDLE has valid current readiness; source A sends `CMD_JOG_RELEASE`, then repeats it | JOG_RELEASE / accepted twice | Release direction is requested; hard maximum is unchanged by keepalive; pressure readiness is not required. |
| `HST-06` | Parameterize global STOP over JOG_PRESS, JOG_RELEASE, AUTO_APPROACH, AUTO_SETTLE, AUTO_PULSE, AUTO_HOLD, and COMPLETE; also owner `CMD_JOG_STOP` in both jogs | IDLE / accepted | Disable is the first side effect, all motion/runtime timers and transient contexts clear; non-owner JOG_STOP is rejected but global STOP still succeeds. |
| `HST-07` | Active jog receives matching `TIMEOUT_JOG_KEEPALIVE` | IDLE / protective-stop detail | Fake executor is already disabled; keepalive and hard timers are off; no FAULT is latched. |
| `HST-08` | Active jog is repeatedly renewed but reaches `TIMEOUT_JOG_MAX_DURATION` | IDLE / maximum-duration detail | Hard maximum never moved; output is already disabled and further keepalive cannot resume it. |
| `HST-09` | IDLE, valid pressure below contact, valid current, approved config; `CMD_AUTO_START` | AUTO_APPROACH with FIRST profile / accepted | Approach and auto-cycle limits arm before press output. An executor-arm failure instead yields FAULT/`FAULT_MOTOR_FAULT` with output off. |
| `HST-10` | AUTO_APPROACH receives a valid sample that reaches contact | AUTO_SETTLE / `SETTLE_WAIT_DELAY` | Disable occurs before state change; approach timer cancels; settle timer arms; pressure sample cannot start another motion in this dispatch. |
| `HST-11` | AUTO_SETTLE receives samples during delay, then matching settle timeout, duplicate gate sequence, then a later valid sequence at target | AUTO_HOLD after the later sample | During-delay samples only update safety/cache; timeout captures latest sequence; duplicate cannot control; later sequence alone drives the decision. |
| `HST-12` | AUTO_SETTLE/WAIT_SAMPLE receives a later valid pressure at contact but below hold-entry band | AUTO_PULSE | One approved pulse is requested; duration and later backstop arm before output; pulse count advances once. |
| `HST-13` | AUTO_PULSE hardware duration expires and fake publishes normal `EVT_MOTOR_DONE` | AUTO_SETTLE / WAIT_DELAY | Output is disabled before event handling; pulse/backstop timers are off; a new settle timer is armed. |
| `HST-14` | AUTO_SETTLE/WAIT_SAMPLE receives a later valid pressure inside hold-entry band | AUTO_HOLD | Motor stays disabled; stability begins with this distinct sample only. |
| `HST-15` | AUTO_HOLD receives distinct valid in-band samples until both configured time and count minima are met | COMPLETE | No motor call occurs; repeated sequence or elapsed time without a new sample cannot complete. |
| `HST-16` | COMPLETE receives `CMD_ACK_COMPLETE` | IDLE / accepted | Output remains disabled and automatic context/result is cleared. |
| `HST-17` | Parameterize AUTO_SETTLE/WAIT_SAMPLE and AUTO_HOLD with a later valid sample below contact | AUTO_APPROACH with RECONTACT profile | Stability resets; separately calibrated re-contact command and timeout arm before output. |
| `HST-18` | Parameterize every state with raw `EVT_OVERPRESSURE`; also feed a frame the filter would reject | FAULT / `FAULT_OVERPRESSURE` | Raw safety bypasses filter/control; disable is first; active motion and timers are off. |
| `HST-19` | Parameterize pressure timeout and invalid stream over JOG_PRESS and all four AUTO states; also JOG_RELEASE | Required states: FAULT/`FAULT_PRESSURE_SENSOR_FAULT`; JOG_RELEASE: remains bounded jog | Required-state output disables; JOG_RELEASE keeps its existing timers; raw overpressure remains separately active. |
| `HST-20` | Parameterize qualified `EVT_OVERCURRENT` over all motion states | FAULT / `FAULT_OVERCURRENT` | Immediate disable precedes fault commit; no new motor request occurs. |
| `HST-21` | Parameterize matching approach, pulse-backstop, and automatic-cycle timeouts | FAULT / `FAULT_MOTION_TIMEOUT` with exact simple detail | Executor-owned motion cases are already output-off; all remaining timers clear. An irrelevant kind/deadline cannot advance state. |
| `HST-22` | Inject MotorExecutor start/renew failure, asynchronous motor fault, impossible transition, and CPU/internal fault | FAULT / `FAULT_MOTOR_FAULT` or `FAULT_INTERNAL_FAULT` | Every branch ends disabled with transient contexts cleared; detailed cause is logged without secondary-fault machinery. |
| `HST-23` | FAULT receives AUTO_START, both jog commands, SET_TARGET, ACK_COMPLETE, and STOP | Remains FAULT | Motion/config commands reject; STOP is accepted only as idempotent disable and cannot clear fault. |
| `HST-24` | FAULT reset subcases: source still active; executor not disabled/idle; source clear and executor safe | First two remain FAULT; accepted case enters BOOT_SAFE, then boot OK enters IDLE | Reset never targets the previous jog/AUTO state and never restores an old timer, owner, profile, pulse, or cycle. |
| `HST-25` | Parameterize UI and retained Modbus mappings: auto, jog ON repeat, jog OFF, STOP, set target, reads, and protected direct pulse aliases | Exact semantic command/result or snapshot value from `03_INTERFACES_AND_DATA.md` | Direct pulses are unsupported in Locked/RealCompileCheck and use Modbus -> Machine -> MotorExecutor only in ScopeTest/RealBench; SET_TARGET succeeds only in IDLE. |

## Additional checks within the 25 logical scenarios

The parameterized assertions above also cover:

- STOP availability in every state;
- no direct reversal between jog directions;
- AUTO_START at/above contact entering SETTLE without motion;
- samples received during settle delay never controlling;
- feedback timeout while waiting for the post-settle sample;
- high-but-not-overpressure HOLD behavior remaining disabled with stability reset;
- duplicate pressure sequences never advancing HOLD;
- pulse duration/backstop ordering;
- automatic-cycle timeout from every AUTO state;
- command results correlated to Modbus requests without transaction-based jog ownership;
- snapshot coherence;
- pressure producer reset/order loss becoming a pressure fault;
- MotorExecutor timer-arm failure never enabling output.

## Test implementation order

1. Build the fake MotorExecutor and deterministic clock.
2. Implement the four enums and minimal contexts from `01`/`03`.
3. Make `HST-01` through `HST-08` pass before automatic logic.
4. Make `HST-09` through `HST-17` pass with pure pressure classifiers.
5. Add safety/reset tests `HST-18` through `HST-24`.
6. Add semantic interface test `HST-25`.

Passing the host suite permits review of minimal Phase 2B logic. It does not close any real-motor hardware gate.

## Section 9A regression additions

`tools/run_host_tests.ps1` reproducibly builds 23 host executables with C11 and warnings-as-errors. The additions cover all four build policies, direct-command Modbus semantics, detailed MotorExecutor failure stages, the actual TIM5 implementation against an STM32 shim, the actual real motor hardware layer against a HAL shim, ScopeTest/RealBench full-chain direct-pulse paths using the real Modbus/Runtime/Machine/MotorExecutor layers, and a static ownership check that permits no Modbus-to-hardware shortcut.

The actual real-hardware HAL-shim test additionally records both PWM-start counts and order plus the timer/compare state at the SD-enable call. PRESS and RELEASE must each show both carriers running with both CCR3 values zero when SD becomes high, followed by the unchanged direction mapping and no observation of both CCRs nonzero. Injected failures cover zero-compare commit, each zero-carrier start, zero-carrier verification, driver-enable verification, and final active-compare verification; every case must latch its exact stage and finish completely disabled. A PRESS -> immediate disable -> RELEASE transition proves a full disabled boundary between directions.

Interface-level real-hardware fakes model both carrier timers enabled during an accepted active request and define a leg conflict only as both CCR3 values being nonzero. Stopped expectations remain both timers off, both compares zero, and both drivers disabled. These are host proofs of ordering and shutdown behavior only; field isolation of U6/U7 still requires a motor-disconnected driver-output re-test and does not prove root cause or motion.
