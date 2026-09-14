# StaticForce3000_Boost1 input evidence

Recorded from the user's report for this change. No raw CSV/report/scope file
was supplied with this request; none was fabricated or substituted. Existing
tracked field files and historical firmware remain unchanged.

- Target250, Kp10, Ki0, continuous cap720 (nominal3%).
- Command reached720 and TIM3 reached144; software saturated for several seconds.
- Pressure27 ->27; MCU session peak27; motor did not move.
- PSU display stayed approximately0.028 A with the field supply limit0.5 A unchanged.
- Scope captured one approximately600 ns /22 V high pulse.
- Final STOP removed PWM and output_off=1.
- No abnormal noise, heat or jamming was reported.

This is real field evidence reported by the user that the tested3% continuous
profile produced no measurable motion or pressure response in that setup.
It does not establish all electrical/mechanical causes, a thermal rating, a
winding-current measurement or a calibrated force limit. The reported600 ns
pulse is not replaced by the nominal CCR144/4800 calculation (1.5 us at20 kHz).
One captured pulse also does not establish the entire waveform history.

The user authorizes one short supervised breakaway experiment with6000 peak,
10 ms maximum/reserved duration and720 continuous control afterward. This
is not a continuous25% rating or3000 N qualification. New candidate physical
test NOT RUN. Hardware stop validation, STATIC_250 performance and ROTATING_LOAD
remain unvalidated by this software delivery; reported prior STOP evidence is
preserved as stated, not expanded into a complete safety qualification.
