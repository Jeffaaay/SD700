# StaticForceAuthority2 handoff

**physical test NOT RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD
remain unvalidated.** No serial connection, flashing, START or hardware operation
was performed by Codex. No ZIP. Deliver by normal commit/push main.

## Delivered state and authority

Actual fetched main was07fe6077c095f4229f68fbbb9168b9aaf55271b2, newer than reviewed
ed3c213b689929524dba2af016d85724097c7850. Worktree was clean. Keep the newer
Boost1 independent TIM5 cutoff, verified lower handoff, persistent event and
cumulative reservation; replace its Target250/Kp10-specific assist admission with
the bounded profile path. Existing PID/trajectory/HOLD architecture remains.

The current HEX enables **only profile4:720 PRESS /100 RELEASE /no assist /5 s**.
It does not provide more live force authority than the reported3% experiment.
Default gains10/0/0, reference200/1000, ordinary slew1000 command/s,20 kHz PWM,
target1..275 legacy control units, raw trip325 and field PSU0.5 A remain. Unit0's
identity conversion is counts-to-counts, not1 N/count. Qualification flags stay0.
Plain ForceServo build is LOCKED, with no runtime unlock.

The compiled read-only catalog represents IDs20/30/40:4800/7200/9600 assist,
CCR960/1440/1920 of4800, with independent normal ceiling2400/CCR480 and RELEASE100.
All three are explicitly disabled: experiment_enabled0, limits_source0; rise,
normal end, hard/total assist, energized/session/build/capture and OFF fields are
zero UNREVIEWED sentinels. They cannot be selected or started; PC refuses them
before serial construction. Numerical9600 representation is not authorization:
real executor ceiling720, continuous ceiling720, approved peak budget0 are pinned
in ELF. Config writes above the active profile's720/100 are rejected.

Missing approval is specific: a reviewed candidate output level, bounded rise,
normal-end and independent-cutoff times, cumulative peak exposure, normal10%
on-time/session allowance, required OFF/cooling interval, and pressure-response/
excessive-rise exit thresholds in controller units. Current0.5 A PSU setting is
unchanged; it is not measured winding current or I-squared-t protection. Do not
infer these limits from duty polls, volt-seconds, old36 s operation or prior chat
25%/10 ms. Reboot is not permission to repeat exposure.

Source1 records the inherited **single short720/100 session**, already bounded to
5 s in the prior source; no numerical cooling rating exists for repeated live
sessions (off_ms0 is absence of such a claim, not a zero-cooling recommendation).
Source2 exists only in synthetic fixtures. The new assist integration fixtures
use invented3 ms rise /9 ms planned end /12 ms hard budget /12 ms total reservation
and1000 ms OFF to exercise ordering. TIM5's unchanged1 ms reserve cuts at11 ms if
main handoff is missed. These are test inputs, not field recommendations.

## Evidence actually available

The exact requested handoff and field files were not found in the workspace,
including ignored files, or the Codex attachments tree. A local-path clarification
was requested; no location was supplied during implementation. Missing:

- SD700_Handoff_StaticForceAuthority2.md
- target250-authority-current.csv, .metadata.json, .report.txt, .field.txt
- Target250-full2-20260914-combined-v2.csv
- Target250-full2-20260914-scope-percent.csv
- Target250-full2-20260914-pressure.report.txt

The following are **user-described observations, not independently parsed files**:
new Target250, Kp10/Ki0/Kd0, command720/CCR144, pressure27 to27, no motion, PSU0.028 A;
one approximately600 ns/22 V scope pulse not synchronized to the cap; final STOP
removed observed PWM. Manual active STOP and feedback-loss checks were incomplete.
Tail error: `No Modbus response received within 2 ms`.

Old report described ContactPulse7V, Start0, target250, peak/final260, target
crossing at PC36.384 s, STOP_READBACK36.461 s; pressure112 to0 then rebuild.
M+ duty polls10/20/30/40% near20 kHz, sparse M-10%, roughly129 ms median polling,
combined offsets up to71.121 ms. No measured current, synchronized differential
motor waveform, matched flashed old HEX hash or sustained active HOLD was supplied.
The script stopped on TARGET_REACHED. These observations neither identify the
pressure collapse's cause nor qualify10% continuous or20-40% burst timing.

Git history/source inspection found no ContactPulse7V label or matched firmware
identity. Historical AUTO source has5000 mV10 ms PRESS/40 ms backstop/50 ms settle,
and separately documented10000 mV approach variants; its comments do not qualify
the new continuous-assist envelope. The predecessor Boost1 HEX/ELF and its exact
manifest remain preserved. No field CSV, report, RAM reading, current measurement
or missing attachment was fabricated. Synthetic captures are clearly named.

## Production path and fail-closed transfer

START still waits for the next fresh ordered pressure frame. Assist additionally
requires enabled reviewed profile, initial static contact, positive ordinary
PRESS demand, target margin, healthy owned output and remaining fixed/cumulative
budget. No blind high-output approach, target250 special case, boost escalation,
retrigger or automatic restart. A full hard-budget reservation is charged before
admission and is never refunded by early exit, STOP, fault reset, config or later
START within a boot. Cooling is tracked separately and survives those operations.
Selection/config remain IDLE/output OFF only.

Normal P/PI computes its usual request. An admitted finite rise uses an explicit
assist contribution through the same executor; it does not merely raise a cap.
Actual committed output, quantization and interlocks still feed anti-windup; the
known assist contribution does not charge I toward the peak. Main-loop rise/end
service does not integrate feedback, advance its sequence or renew its lease.
The ordinary request regains control immediately on response/taper, non-PRESS
demand or planned end. New unsafe pressure is checked before any rising write.
BUILD-to-HOLD does not reset integral; zero output is allowed. Ki0 is a first
experimental default, not a guarantee of zero error or validated HOLD.

The independent TIM5 peak compare remains armed through lower hardware commit
and guard verification. Only a reduction no larger than actual output and the
saved normal request can occur before another sample. It restores the original
receive-anchored lease, never a new lease from transfer time. A missed rise cannot
turn this handoff into an increase. Pending expiry/late service/failed guard or
transfer force OFF; the IRQ never hands off to sustained output. Peak cutoff,
receive lease and absolute5 s session stay independent. STOP/fault, raw and
unit-qualified overpressure, contact loss,125 ms feedback gap,20 ms delivery age,
130 ms receive lease, reverse OFF and output guard remain fail-closed.

## Capture and telemetry

F106/build46530108:50 FC03 config words;66 active profile words;198 catalog words
from0x400;206 frozen diagnostic words (77 u32 +26 float). All reads use bounded
11-word chunks with existing station/function/CRC/exact-length checks. Active
profile/config digest and catalog identity are checked before START.

Each transaction requires its full100 ms timeout plus100 ms STOP reserve. Budget
and operator STOP are checked between chunks. Incomplete snapshots are discarded;
no2 ms timeout is manufactured. Genuine timeout/CRC/transport errors retain their
original evidence and CAPTURE_ERROR, even when the clock reaches the deadline.
Lost START echo remains UNKNOWN and is never resent. STOP is sent before readback;
post-STOP evidence collection does not extend the intended powered interval.
PC scheduling is not a hard real-time stop guarantee; MCU lease/session/TIM5 remain
independent. Capture preflight reserves250 ms START wait, calculated trajectory,
HOLD dwell, a worst-case2000 ms snapshot and100 ms STOP within5 s. It does not stop
at first target crossing. The exact README command is simulated in host tests.

Admission codes:0 none,1 admitted,2 disabled/unqualified,3 initial/current contact
missing,4 target margin,5 no positive PRESS demand,6 cumulative spent,8 prior HOLD.
Exit codes:0 none,1 planned end,2 response,3 taper/non-PRESS,4 excessive rise,
5 STOP,6 fault,7 lease expiry,8 failed service/handoff. Existing end-reason2 means
verified lower transfer;3 means aborted. An excessive rise records new fault
detail19. Persistent fields retain requested peak, maximum committed command/CCR,
start/planned-end/hard-deadline/end, duration/reservation, handoff, pressure before/
after/peak and receive timestamps. Pressure after is unavailable until the next
valid frame;101 ms sampling does not measure pressure at the9 ms handoff. Abort
elapsed is a capped service-time bound, not physical PWM on-time. MCU register
plans/guard checks, external waveform and physical force are distinct evidence.

Use the single conditional procedure in README. This delivery does not authorize
an unreviewed high-output field run or request another observe-only tuning round.
Do not claim safety-output checks as force-HOLD qualification. Supply the missing
specific envelope/source identity to enable an appropriate candidate in reviewed
firmware; no runtime bypass exists. Actual commands/results and earlier failed
software checks are in TEST_RESULTS.md and verification.json.
