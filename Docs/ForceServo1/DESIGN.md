> Historical ForceServo1 locked-candidate record. CommissioningUnlock1 now
> supersedes its output lock, current parameter bounds and next field step.
> Follow the [current README](../../README.md) and
> [commissioning record](../CommissioningUnlock1/TEST_RESULTS.md).
> The historical instructions below do not require a separate Observe field round.

# ForceServo1 — reviewable implementation, physical output LOCKED

Baseline is `f24d4a62240ec1630e026fd1eeba491f7c35b11f` (ConvergenceMeasure1).
HEAD matched that commit and the worktree was clean at entry. No later changes
were rolled back. The 45-second convergence / 20-ms PRESS alternatives were not
implemented. The new **total** session default is 45 seconds including approach
and HOLD; the contact-to-first-HOLD failure deadline remains 30 seconds.

## Audit and actual signal chain

The old `Machine_ConfigureForcePi` rejects nonzero Ki/integral bounds in AutoTarget;
`Machine_StartCorrection` overrides positive PI with base + boost. Old HOLD turns
outputs off, and the pulse executor rejects requests while active. These legacy
functions and constants remain for their separate build and regression suite.

`-ForceServo` selects `force_servo_machine.c` in place of legacy `machine.c`'s
implementation. Both still export the established Machine API, so runtime,
pressure conversion and Modbus remain the actual production chain. Build policy
rejects AutoTarget + ForceServo and any ForceServo backend except RealBench.
Legacy AUTO/direct/JOG commands cannot drive ForceServo. Its sole START is the
new coil 0x10; STOP remains the existing high-priority coil 1 OFF. All physical
arming is compiled false by `motor_real_gate.c`, including bounded approach.
There is no RAM parameter or target build switch that unlocks this candidate.
Host HAL shims allow logical motion solely to test this implementation.

```
USART6 7-byte valid frame -> latest receiver snapshot (no backlog replay)
 -> identity pressure units -> raw safety -> trajectory + PID
 -> amplitude + command slew -> executor direction interlock + PWM quantization
 -> successful software commit -> back-calculation for next integral
```

The sensor protocol supplies neither sensor sequence nor sample timestamp.
Sequence is the MCU receiver's 64-bit count; time is MCU frame receive time,
not sensor acquisition time. The 115200 8N1 frame takes at least ~0.608 ms on the
wire; this does **not** establish the sensor period or worst-case latency.
No measured MCU scheduling/receive jitter data exists in this workspace.
PC poll spacing cannot establish either. Valid receive sequence/tick wrap is
handled by unsigned half-range comparisons. Duplicate samples never integrate
or renew. Reverse sequence/timestamp, stale input, excessive control dt and
invalid input stop an active session. Latest samples arriving sooner than the
minimum control interval update safety but are not integrated; their history is
not later replayed. The next dt is the interval since the last control sample.

## Trajectory and controller

At first contact, reference and filtered measurement start at that measurement;
I and committed command start at zero. The 10000 mV approach is not inherited.
For distance D, use a cubic smoothstep with duration
`T=max(1.5*abs(D)/reference_rate, sqrt(6*abs(D)/reference_acceleration))`.
Its endpoint rates are zero; maximum rate and acceleration satisfy both bounds
without overshooting the target. Targets 1..275 use the same algorithm.
The active target is fixed for the session. One measurement change cannot reset
the reference. Low targets can require a descending reference after contact.

For each accepted control sample: filter measurement with `alpha=dt/(tau+dt)`;
filter its derivative with a separate time constant. Compute
`u_raw = Kp*(ref-filtered) + I[k] - Kd*d_filtered_measurement/dt + 0`.
Kd=0 explicitly produces PI, and Ki=0 explicitly disables integral memory.
The derivative does not differentiate the target. Feedforward is protected zero
because there is no identified feedforward model; no writable inert FF exists.

Clamp to independent commissioning caps, then slew relative to the last
successful committed signed command. Executor may further commit zero during
direction deadtime. Only on successful commit calculate
`I[k+1]=clamp(I[k]+dt*(Ki*error+tracking_gain*(u_committed-u_raw)), Imin,Imax)`.
This explicit next-step update has no algebraic loop. It tracks all downstream
software restrictions, including integer mV quantization and direction interlock.
STOP/fault/feedback failure revoke output and reset/freeze controller memory;
the next accepted START reinitializes it. Config, dt, state and calculations
reject non-finite values. Output is **PWM-equivalent requested mV**, not measured
terminal voltage, current, torque, velocity or a certified cascaded servo.

## States, deadlines and safety

Legacy values 0..11 retain their meanings. New values are FORCE_APPROACH=12,
FORCE_BUILD=13 and FORCE_HOLD=14. IDLE=1 and FAULT=9 remain shared.
Approach reuses the existing bounded 10000 mV / 20 ms / 50 ms backstop profile,
then OFF + 50 ms settle and a new sample before another approach request.
Timely valid contact interrupts approach and starts the new force loop.
Initial approach does not consume the 30-second build budget; already-contacted
START anchors it to START. Contact loss (new valid reading below 20) now latches
CONTACT_LOST, with threshold and triggering raw value recorded. It is evidence
of the sensor predicate, not proof of mechanical separation. No automatic
10000 mV recontact exists in this mode.

BUILD/HOLD share one controller without resetting I or imposing periodic OFF.
HOLD requires trajectory complete and error within enter tolerance; exit uses
the wider tolerance. It may command PRESS, zero or bounded RELEASE. First HOLD
satisfies the build deadline; the separate session deadline continues through
every transition. First HOLD is not performance acceptance. Sustained amplitude
saturation and excessive reference tracking error have separate bounded exits.
Those diagnostic experiment budgets are not mechanical ratings.

New details: 13 lease expired, 14 contact lost, 15 total session deadline,
16 sustained saturation, 17 sustained tracking error, 18 numerical failure.
Build expiry retains Fault5/Detail8. Raw >=325 bypasses filters and stops before
control; target max275 and contact20 are protected. User pressure units are
sensor counts, **NOT calibrated N**. No target500 or rotating-axis interface.

## Executor / TIM5 timing and race boundaries

Continuous mode is a distinct executor action with a session token. Same-direction
updates change one compare while carriers continue; zero immediately disables.
Reversal first verifies OFF, then waits at least the configured deadtime; a later
new sample must request the reverse direction. PWM configuration/compare/driver
checks remain in GuardOutput/ActiveRequest. Pulse mode remains mutually exclusive.
Hard executor limits are +5000/-800; tunable commissioning limits are strictly
lower. No continuous-duty rating is inferred from the old pulse envelope.

Successful new-sample control commits alone renew the lease. Deadline is
`received_ms+lease_ms`, never `processing_time+lease_ms`. TIM5 runs at 10 kHz;
lease uses a free-running CC1 compare (ARR UINT32_MAX) and an IRQ that disables
drivers before cancelling the timer and latching completion. Renewal changes CCR1
without cancelling the counter or clearing SR/NVIC events. It checks old compare
time, pending flags, and pending completion again. A conservative 1 ms subtraction
allows for the granularity of HAL receive/processing timestamps; this is not a
measured IRQ latency allowance. Old pulse one-shot compare/backstop behavior is
unchanged. Continuous lease does not claim an independent second hardware cutoff.

Executor commit/STOP/lease renewal run inside a saved-PRIMASK critical section.
A pending compare during renewal fails closed. Expiry invalidates the token;
STOP closes it. Only a new accepted START can create a new machine session and
later executor token. No ISR prints, sends telemetry or performs PID work.
Main handles bounded Modbus ingress and queued STOP before sample/PID processing,
then keeps the original safety/guard/STOP/normal-command sequence.

This covers main-loop/control/telemetry stalls **only while the timer IRQ can
execute**. It does not cover disabled global interrupts, a failed CPU, peripheral
failure, broken shutdown wiring or energy remaining in mechanics. HAL IWDG/WWDG
modules are enabled in configuration, but no watchdog initialization/refresh is
called by this application. Early force-disable, disabled motor initialization,
and no automatic restart remain. External E-stop is still required.

## Commissioning timing contract — NOT VALIDATED

| Parameter | Numerical seed | Current evidence / meaning |
| --- | --- | --- |
| Minimum control interval | 5 ms | New-sample scheduling floor; no 1 kHz claim |
| Maximum feedback/control gap | 40 ms | Synthetic test budget; **not measured sensor capability** |
| Maximum sample age at commit | 20 ms | Receive age, not sensor acquisition age |
| Lease | 50 ms | Receive-anchored, timer compare subtracts 1 ms |
| Measurement filter tau | 0 s | Default bypass; nonzero adds frequency-dependent delay |
| Derivative filter tau | 0.02 s | Mathematical seed, D defaults zero |
| Reversal deadtime | 2 ms | Added minimum OFF; electrical adequacy NOT_VALIDATED |
| Build / total session | 30000 / 45000 ms | Bounded failure exit / experiment end |
| Saturation / tracking exit | 5000 / 10000 ms | Bounded experiment diagnostics |

The missing worst-case sensor delay/jitter, control execution/IRQ latency,
driver deadtime and allowed continuous thermal exposure block physical arming.
The old 200 ms OFF-settle freshness allowance was not adopted as continuous
drive safety evidence. All numerical seeds are **COMMISSIONING_NOT_TUNED**.

## References and evidence boundaries

Output tracking after downstream limits follows the structural approach described
by [MathWorks anti-windup](https://www.mathworks.com/help/simulink/slref/anti-windup-control-using-a-pid-controller.html).
[Bumpless transfer](https://www.mathworks.com/help/simulink/slref/bumpless-control-transfer-between-manual-and-pid-control.html)
informs state continuity; safety limits take precedence over continuity here.
[Delta Motion's P then I tuning discussion](https://deltamotion.com/support/webhelp/rmctools/Content/Starting_Up_the_RMC/Tuning/Tuning_Pressure_Force_Axis.htm)
and [Bosch's force-control note](https://community.boschrexroth.com/sfk-pressing-how-tos-7931wc8u/post/force-control-jCU6Lb35k8I637d)
are structure/method references, not sources for this machine's gains or ratings.
No MATLAB or vendor runtime was introduced.

The user describes a field curve falling after104 then rising to199. Its cause
remains UNKNOWN; no attribution to backdrive, current limiting or physical
ceiling is justified. The named prior PressBoostRetain1 CSV/report were absent
at baseline; all such values are user-reported evidence, not fabricated files,
RAM measurements or current measurements. All original evidence found on disk
is hash-checked for preservation. **physical test NOT RUN**.
