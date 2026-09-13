# Target250Continuous2 - field evidence and narrow output trace

Baseline/local HEAD/origin-main after fetch were all
`c73e55ba505c4f7f43dff481c32155d0558075b8`; initial worktree was clean.
No later work was discarded. No reset, force push, ZIP or serial connection.

## User-reported field evidence

The requested `target250-mvp1-20260913-131725-660.csv` and matching `.metadata.json`
were not found by workspace filename inventory, including ignored captures.
Their contents were not independently parsed. The following is the user's
provided evidence, not fabricated CSV, RAM readback or current measurement:

- Target250MVP1 HEX SHA256
  `4277556949A4AD6A2D5D24E49F8A1DEA97C7D8906CA48AA9878D68B5DF924055`.
- START accepted and continuous control executed. Settled reference250, pressure22,
  error228, P/raw228, submitted100, planned TIM3 compare20.
- RUN snapshots remained22; saturation clock reached5001 ms; Fault5/Detail16.
  Subsequent software readback reported output OFF and lease inactive.

This establishes a capped software command and no observed pressure rise. It
cannot independently establish terminal voltage, winding current, motion,
sensor failure, mechanical stall or a permissible higher continuous command.
The old profile is reproduced by production-path tests at raw228/command100 and
its unchanged5000 ms clipping timeout. Field5001 ms reflects a sampled deadline,
not a newly changed threshold. Saturation itself is a software operating condition.

The user also reports an older version reached250 on this machine. The exact
revision/raw evidence was not found. `git log --all` contains the reviewed chain
back to `ca68ba7` clean baseline. Searches of retained engineering records found
prior AutoTarget non-attainment reports, not identification of that successful
build. No historical AutoTarget revision is silently relabeled proven. Existing
firmware, records and field failures remain unchanged.

## Production path and restrictions

| Stage | Actual source/behavior | Target250Continuous2 change |
| --- | --- | --- |
| Pressure intake | `force_servo_machine.c`, valid flags, sequence/order, receive time, fresh sample and raw protection | Control/safety gating unchanged; session peak telemetry added |
| Reference/error | `ForceServo_Prepare`, existing cubic smoothstep, optional measurement filter, reference minus filtered pressure | No trajectory change |
| P/I/D | Kp*error, causal I, derivative on measurement; ff remains zero | One controller, defaults1/0/0 unchanged |
| Amplitude | clamp raw to -release_cap/+press_cap | Parameter maximums use direction profile definitions |
| Rate | old symmetric previous_committed +/- rate*dt | Explicit profile ramps increases; magnitude reductions immediate; reversal ramps from0 |
| Integer | machine truncates post-limit float toward0 | Unchanged; new float telemetry distinguishes it from integer request |
| Session executor | ownership token, unique sequence, freshness, lease and independent hardware guard | Same checks; +/-100 duplicates replaced with direction profile ceilings |
| Extra downstream bounds | old explicit path also saw5000/-800 fallback after100 checks | Fallback retained only for the default locked logical path; explicit profile uses authoritative ceilings |
| PlanCommand | positive magnitude; direction; bounds; ceil(magnitude*4799/24000) | Direction-specific profile checks replace common100 check; no silent clamp to100 |
| PWM hardware |96 MHz, PSC0, ARR4799, PWM1 active high,20 kHz | No change |
| Enable/update | safe zero-carrier apply, SD1/SD2 then selected compare; same-sign update preserves carrier | No change |
| Output guard | actual timer/CCR/enable/pin state compared with the plan | No change |
| Protocol/capture | complete RAM config transaction and48-word FC03 readback | Same transaction; limits sourced from profile; capture checks actual readback ranges/relationships |
| Firmware verifier | exact defaults, embedded contract, arming symbol and load image | New schema/build; exact16-word contract includes ceilings, defaults, rate policy and readiness0 |

The two former direct100 executor checks were PlanCommand and UpdateContinuous.
The two parameter maxima and defaults each also encoded100. Python schema/capture
inherited the parameter maxima; verifier separately pinned both defaults and
contract100/100. These are now linked to the profile source where practical;
the verifier deliberately keeps independent exact expected values to detect drift.

Selected default caps remain100/100, bounded by separate100/100 ceilings. P-only
therefore still produces100 at pressure22/reference250. No claim of improved
actuator authority is made. A compile-time HOST override exercises ceilings600/200
and operating400/100, Kp4, taper error100, without a second controller. It tests
unclipped values above100, independent RELEASE bounds, protocol reads and physical
register shims. Those numbers are deliberately non-authoritative test inputs.
A non-host build rejects any changed profile ceiling pending hardware review.

At100 command, ceil(100*4799/24000)=20. Nominal duty is20/(4799+1)=0.416667%;
high time is20/96 MHz=0.208333 us; carrier period50 us. Neither command100 nor
compare20 is a measured100 mV terminal voltage. Existing diagnostic tim2/tim3
are planned counts. Actual registers are checked by the production output guard,
but no new actual-register telemetry or external waveform measurement is claimed.

## Hardware and older-code comparison

The visually inspected power-stage sheet (page2) of
[2026-03-19_SCH_servo_press.pdf](../../Reference/Hardware/2026-03-19_SCH_servo_press.pdf)
labels U6/U7 IR2104STRPBF, Q4-Q7 DON30N10T,22-ohm gate resistors, FR107 bootstrap
diodes,1 uF bootstrap capacitors,12 V driver supply and INA240A1DR/R35 1 milliohm.
This is a schematic, not confirmation of fitted components, motor model, current
sense calibration, cooling or safe continuous duty. The drawing itself notes a
resistor-value adjustment. No separate motor ratings or qualified continuous
thermal/current evidence was located in this repository.

The inspected current AutoTarget configuration uses the same4799/24000 mapping
and direction polarity. It has up to5000 mV PRESS for10 ms,40 ms backstop and50 ms
settle; first approach10000 mV/20 ms and recontact10000 mV/10 ms. Source-defined
ceilings, durations and OFF/settle intervals do not establish continuous ratings.
The older retained legacy evidence describes positive PRESS/negative RELEASE,
10 ms base pulses and a30 ms minimum cooldown, with several timing comments marked
unreliable. None of these values were promoted into the new continuous ceiling.
No broad legacy controller reconstruction was performed.

| Claim class | Evidence available |
| --- | --- |
| A: representable command | Source mapping and host register tests; within24000 command representation |
| B: switching circuit reproduces input pulse | Nominal timing calculated; actual gate/terminal waveform unavailable |
| C: produces mechanical response | Current100 trial reported no pressure rise; older250 success reported but unidentified |
| D: acceptable continuous electrical/thermal range | Not established; motor rating and measured motor-side current/temperature missing |

Keep physical supply limit0.5 A unchanged. It is not a winding-current limit.
A larger number needs fitted motor continuous-current/duty/thermal specifications
and a reviewed proposed-command measurement of gate/terminal waveform, motor-side
current and temperature for the actual board/load. Board component names or a
supply current setting cannot supply that approval. **POWERED_TEST_READY=NO.**

## Primary references used for interpretation

The [official IR2104 datasheet](https://www.infineon.com/assets/row/public/documents/24/49/infineon-ir2104-ds-en.pdf)
lists dynamic propagation delays and deadtime under stated test conditions. It
provides no basis here for treating turn-on delay as a guaranteed minimum accepted
IN pulse width or declaring0.208 us definitely suppressed. No frequency/deadtime
change or compensation was inferred from that hypothesis.

[maxon](https://support.maxongroup.com/hc/en-us/articles/360006322414-Power-conversion-in-PWM-power-stages)
explains PWM power conversion and why supply current differs from motor current.
It supplies no machine-specific duty limit.

[Bosch Rexroth force control](https://community.boschrexroth.com/sfk-pressing-how-tos-7931wc8u/post/force-control-jCU6Lb35k8I637d)
describes continuously active force control through search and hold and warns that
P must fit the application. Its force-to-velocity gain is not this firmware's
force-to-command gain and was not copied numerically.

[MathWorks anti-windup](https://www.mathworks.com/help/simulink/slref/anti-windup-control-using-a-pid-controller.html)
supports tracking constrained actuator output for integrator correction. Existing
actual-committed tracking and the quantization exception remain tested.
[MathWorks rate limiting](https://www.mathworks.com/help/simulink/slref/ratelimiter.html)
distinguishes rising and falling rates; it does not certify mechanical acceleration.
The chosen magnitude-decrease behavior is this task's small software correction,
not a new motion planner or machine-specific validated braking model.
