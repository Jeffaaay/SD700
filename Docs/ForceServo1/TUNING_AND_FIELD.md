> Historical ForceServo1 locked-candidate record. CommissioningUnlock1 now
> supersedes its output lock, current parameter bounds and next field step.
> Follow the [current README](../../README.md) and
> [commissioning record](../CommissioningUnlock1/TEST_RESULTS.md).
> The historical instructions below do not require a separate Observe field round.

# ForceServo1 — the one next field step

**Next step: motor power physically disconnected; locked firmware; collect
pressure receive/telemetry timing for at most 60 seconds on COM5, ZERO START.**
This candidate cannot enable the motor. Do not use AutoTarget/ApproachOnly,
manual START, or try to bypass the lock. Hash and exact command are in the
current README. The operator may flash the identified locked HEX with motor
power disconnected; this task did not connect, flash, or test hardware.

Record actual retained current-limit setting, initial gap, and that motor power
is disconnected. Return the generated CSV, report and brief field notes.
Do not automatically repeat. The report contains the whole parameter readback,
build ID, config version/digest and operator HEX attestation. Neither this nor
firmware identity registers constitute a readback hash of MCU flash.

This one observation establishes only what was observed about receive intervals
and polling coverage. It cannot establish worst-case timing or validate the new
motor stop path. Current and temperature without actual instrumentation are
NOT_MEASURED. It is not a static250 force-performance trial.

## Required hardware qualification before a later powered tuning release

With reviewed budgets and a separate explicitly authorized qualification build,
scope the new continuous path: boot OFF; first enable; same-direction updates
without OFF gaps; zero/STOP shutdown; both direction reversals with measured OFF
deadtime; missing sensor updates; stopped control/main task while TIM5 runs;
lease expiry/renewal boundary; pending timer event and STOP racing an update;
guard faults; raw overpressure; contact loss; build/session deadlines. Measure
receive-to-commit and timer-expiry-to-driver-OFF latency at the pins. Also verify
reset/brownout behavior and actual external E-stop shutdown. Host register shims
and prior pulse hardware results do not qualify any of these new paths.

Required before power: an attending operator, permitted load/travel/cumulative
motion and thermal exposure, usable external E-stop, actual continuous driver/
motor ratings, justified sampling/lease/IRQ/deadtime limits, and retained current
limit. A power-supply current limit is not a force protection. Log visible current
limiting/sag, abnormal heating or mechanical behavior and stop promptly.

## One-page tuning order after those blockers are resolved

1. **Static P.** Select a documented permitted target within 1..275 and a bounded
   session. Start with measured/approved low continuous caps and reference limits;
   Ki=Kd=0, FF=0. Review force/reference/error and actual committed PWM command.
   The included defaults are numerical seeds only, not recommended powered gains.
   Increase no current limit automatically. One explicit script START per run.
2. **Static PI.** P need not perfectly reach250 before I is useful. Introduce a
   small deliberately selected Ki after confirming control sign and adequate
   response. Inspect I[k], next I, saturation/rate/interlock flags and committed
   output. Tune against the measured curve; do not translate 10 ms pulse response
   into a continuous safe voltage or guaranteed gain. Stop after the one run.
3. **D only if necessary.** First resolve sensor timing/noise and filter delay.
   Use measured derivative filtering, then a deliberately small Kd if it improves
   the observed disturbance response. D is on measurement; keep FF zero without
   identification. Never perform unattended repeated START/auto gain/output growth.
4. **Fixed rotating process condition.** Only after static qualification, keep
   the same force controller and independently establish the actual workpiece
   rotational/line speed and permitted loading. That is a disturbance test,
   not an invented rotating-axis command or loading-axis velocity loop.

Each change uses one complete RAM parameter group, only IDLE + verified OFF.
Record readback/version/digest and one CSV/report per trial. Existing outputs
are never overwritten. A rejected group keeps the previous whole group.
Parameter application never STARTs or clears faults. No production performance
acceptance is implied by HOLD entry or by the safety timeout. Customer targets
for rise time, overshoot and stable holding must be agreed and measured; none
have been invented here. Static250, rotating-load and target500 remain untested.
