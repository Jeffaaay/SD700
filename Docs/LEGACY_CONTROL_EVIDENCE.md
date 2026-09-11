# Legacy field-control evidence

Nothing in this document is approved active control configuration. The executable pressure controller, PID modules, pulse scheduler, fake mechanical model, UI coupling, and motor-control API were removed. `Config/legacy_control_reference.h` is reference-only and is not included by Application or Board code.

## Extracted values

| Value | Legacy source | Classification | Notes |
| --- | --- | --- | --- |
| Maximum pressure 1500 N | `pressure_control.h: MAX_PRESSURE_N`; `show.h: pressure_limit`; Modbus dispatcher | ACTIVE_IN_LEGACY_CODE | Protocol write range was 0..1500 |
| Contact threshold 10 N | `PressureControl_Update` and coarse mode | ACTIVE_IN_LEGACY_CODE | Used to distinguish initial approach from re-approach |
| Coarse threshold +/-50 N | `PRESSURE_ERROR_COARSE_*` | ACTIVE_IN_LEGACY_CODE | Despite stale README thresholds |
| Micro threshold +/-10 N | `PRESSURE_ERROR_MICRO_*` | ACTIVE_IN_LEGACY_CODE | Bound between micro and fine logic |
| Fine/hold threshold +/-1 N | `PRESSURE_ERROR_FINE_*`, `PRESSURE_ERROR_HOLD_*` | ACTIVE_IN_LEGACY_CODE | Hold entry used +/-1 N |
| Hold exit approximately +/-2 N | hold mode uses `FINE_HIGH * 2` | ACTIVE_IN_LEGACY_CODE | Hysteresis was a local mode assignment, not a robust transition contract |
| First approach 5.0 V | `APPROACH_VOLTAGE` | ACTIVE_IN_LEGACY_CODE | Comment says earlier 15 V was excessive |
| Re-approach 2.0 V | `REAPPROACH_VOLTAGE` | ACTIVE_IN_LEGACY_CODE | Used only after prior contact and pressure falling back low |
| Contact coarse clamps +2.5/-1.5 V | `COARSE_CONTACT_VMAX/VMIN` | DEFINED_BUT_UNUSED | Never referenced by the controller |
| Micro pulse 0.8..3.0 V | `MICRO_PULSE_VOLTAGE_*` | ACTIVE_IN_LEGACY_CODE | Scaled by error before adaptive boost |
| Fine pulse 0.4..1.0 V | `FINE_PULSE_VOLTAGE_*` | ACTIVE_IN_LEGACY_CODE | Scaled by error before adaptive boost |
| Default pulse voltage 1.5 V | init field assignment | DEFINED_BUT_UNUSED | The active pulse paths recomputed a local voltage |
| Base pulse 10 ms | `pulse_duration_ms`, `s_cur_pulse_duration` | ACTIVE_IN_LEGACY_CODE | Task executes every 10 ms |
| Claimed minimum pulse 2 ms | `PULSE_MIN_DURATION_MS` | KNOWN_TIMING_INVALID | A 10 ms task cannot terminate a real 2 ms pulse |
| Mechanical cooldown 30 ms | `PULSE_MIN_COOLDOWN_MS` | ACTIVE_IN_LEGACY_CODE | Used by post-pulse wait logic |
| Cooldown field 150 ms | `PULSE_COOLDOWN_MS` and init assignment | DEFINED_BUT_UNUSED, CONFLICTING_DEFINITIONS | Field was assigned but never consulted by the pulse paths; its comment incorrectly said 30 ms |
| Fresh-feedback wait timeout 400 ms | `PULSE_FRESH_TIMEOUT_MS` | ACTIVE_IN_LEGACY_CODE | Timeout permitted another pulse; it was not a fault |
| Pressure-feedback timeout 500 ms | `PRESSURE_FB_TIMEOUT_MS` | ACTIVE_IN_LEGACY_CODE | Set voltage zero and returned; no latched fault |
| Single-frame jump threshold 80 N | `PRESSURE_GLITCH_JUMP_N` | ACTIVE_IN_LEGACY_CODE | First three consecutive jumps were rejected; the fourth was accepted |
| Pulse boost step 0.3 V | `PULSE_BOOST_STEP_V` | ACTIVE_IN_LEGACY_CODE | Applied after two consecutive <1 N responses |
| Micro boost maximum 2.0 V | `PULSE_BOOST_MAX_V` | ACTIVE_IN_LEGACY_CODE | Added to base pulse |
| Fine boost maximum 1.0 V | `FINE_BOOST_MAX_V` | ACTIVE_IN_LEGACY_CODE | Added to base pulse |
| Coarse boost step/max 0.5/3.5 V | `COARSE_BOOST_*` | ACTIVE_IN_LEGACY_CODE | Checked every 200 ms during approach |
| Reverse/release boost multiplier 0.5 | pulse applied-voltage formula | ACTIVE_IN_LEGACY_CODE | Reduced only the boost component, not base voltage |
| Manual jog approximately +/-1 V | manual control switch | ACTIVE_IN_LEGACY_CODE, NEEDS_HARDWARE_VALIDATION | Negative meant up/release; positive meant down/press |
| Fast and one-touch approximately +/-10 V | manual and one-touch control | ACTIVE_IN_LEGACY_CODE, NEEDS_HARDWARE_VALIDATION | Exceeded the automatic controller's +/-6 V constants |
| Automatic output limit +/-6 V | `MAX_V`, `MIN_V` | ACTIVE_IN_LEGACY_CODE, NEEDS_HARDWARE_VALIDATION | PID fields carried the limit; approach/boost logic was separately bounded by its own values |
| Completion count 200 | global `Num_OK` | ACTIVE_IN_LEGACY_CODE | Counted control-task executions, not fresh samples or stable elapsed time |
| Control timing 5/10/20 ms | control interval field / task delay / comments | CONFLICTING_DEFINITIONS | Field was 5 ms, caller ran every 10 ms, comments repeatedly claimed 20 ms |

The header also records that three consecutive abnormal jumps were rejected and the fourth was accepted. The comment saying “three or more are accepted” does not precisely match that implementation.

## Retained engineering experience

| Behavior | INTENT | LEGACY_IMPLEMENTATION | KNOWN_PROBLEM | WHAT_MUST_BE_DECIDED_LATER |
| --- | --- | --- | --- | --- |
| Fast first approach | Reach the workpiece quickly before load | Continuous +5 V while pressure <=10 N and no prior contact | No travel limit, command lease, or contact redundancy | Safe approach prerequisites and abort conditions |
| Slower re-approach | Avoid a second overshoot after losing contact | Continuous +2 V after `s_has_contacted` became true | Contact truth was session-local and based on one filtered stream | Contact state ownership and re-entry rule |
| Contact detection | Switch from approach to bounded corrections | Pressure around 10 N | Sensor plausibility and raw safety pressure were not separate | Validated contact evidence and debounce |
| Bounded pulses after contact | Limit energy and overshoot | Micro/fine voltage pulse calculated from error | Pulse timing was task-based | Hardware-timed actuation design, only after Phase 2 tables |
| Smaller pulses near target | Reduce fine adjustment energy | 0.8..3.0 V micro and 0.4..1.0 V fine scaling | Values lack current/load/mechanical validation | Approved energy envelope and tuning process |
| Reduced reverse boost | Avoid excessive rollback on release corrections | Negative correction added only half the adaptive boost | Base reverse voltage was not similarly scaled | Direction-specific energy limits |
| Boost after low response | Overcome friction/stiction | Pulse boost after two <1 N responses; coarse boost every 200 ms | “No response” could be stale feedback or sensor filtering | Sample-qualified response test and boost ceiling |
| Brake between pulses | Resist rollback | Enabled both IR2104 channels with zero compare after a pulse | Electrical behavior not independently verified | Coast/stop/brake truth table from schematic and bench review |
| Brake during hold | Maintain pressure against backdrive | Same brake request throughout hold | Could create current/thermal behavior not measured | Hold actuator policy and thermal limits |
| Mechanical settling | Avoid correcting transient motion | Required at least 30 ms after pulse | Fixed time was not tied to mechanics or fresh samples | Settling evidence and criteria |
| Wait for new feedback | Avoid consecutive pulses on the same reading | Compared receive timestamp with a snapshot taken at pulse start | A frame received during the pulse could satisfy “fresh” after pulse end; no sequence number | Post-actuation sample sequence and ownership |
| Feedback timeout | Stop blind motion | 500 ms without receive timestamp set voltage zero | Did not enter a latched fault or define reset policy | Fault reaction and recovery table |
| Jump rejection | Reject motor-noise spikes | Hold previous value for the first three >80 N jumps, accept fourth | Raw safety maximum and filtered control value were the same path | Separate safety/raw and control/filtered channels |
| Fine fallback | Recover when error widened | Local reassignment from fine to micro | Not a complete hysteretic state transition and could be overwritten next update | Explicit state/event transition rules |
| Hold exit | Re-engage correction after drift | Leave hold beyond roughly +/-2 N | `pressure_stable` existed as an independent mutable truth | Single authoritative state and stability definition |
| Completion | Return UI after sustained hold | After 200 hold executions call `interface_press_init()` directly | Counted executions, not fresh data or elapsed stable time; control invoked UI | Completion event definition and UI ownership |

## Historical PID and mechanical model

The integrated controller initialized PID fields to Kp 0.62, Ki 0.0005, and Kd 0.0003, but no PID calculation drove the retained coarse path. The separate generic PID module used a different 2.0/0.5/1.0 set and was not called by the active application. Both sets are historical/misleading `DEFINED_BUT_UNUSED` evidence and are intentionally absent from active configuration.

The controller assigned a spring coefficient of 100 N/mm and screw pitch of 2 mm as explicit example values, then never used them. They are fake mechanical-model evidence, not retained configuration, and no replacement model was created.

## Direction evidence

Positive voltage selected TIM3 duty and meant press/down. Negative voltage selected TIM2 duty and meant release/up. This field relationship is retained exactly as passive mapping; it is not an active motor API.
