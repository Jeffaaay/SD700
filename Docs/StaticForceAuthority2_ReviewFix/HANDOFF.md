# StaticForceAuthority2 ReviewFix handoff

**PHYSICAL_STATUS=NOT_RUN.** This is a completed focused software correction,
not completion of the original high-output field-delivery objective. Actual live
profile4 remains PRESS720/RELEASE100/no assist/5 s. No serial connection, flashing,
START, PSU adjustment, higher-output enable or background automation occurred.
Default ForceServo remains compiled LOCKED. Delivery is normal Git main; no ZIP.

## Reproduction and correction

Fetched main and clean local HEAD matched the review baseline
b9df15185526be1ecf096a270c5e4da6b9708e3f. The supplied original reproduction is
preserved byte-for-byte in Review_repro_original.c; it confirmed the gap at
O0/O2/Os. A regression requiring the correct stop failed on baseline O0. After
the correction that same regression passes at all three optimizations.

| Synthetic event | Before fix | After fix |
| --- | --- | --- |
| Admission: pressure27, assist4800, rise3/end9/hard12 ms | Existing fixture | Unchanged |
| +3 ms peak |4800/CCR960 |4800/CCR960 |
| +9 ms handoff |89/CCR18, boost_active0 |89/CCR18, boost_active0, response_pending1 |
| +101 ms first pressure37 |Fault0, BUILD,93/CCR19 |Fault5/Detail19, OFF before normal update |
| Pressure37 at+4 ms during assist |Detail19, OFF |Unchanged |
| Raw325 after handoff |Raw overpressure, OFF |Unchanged |

No PID/trajectory/executor/TIM5/PWM/source-profile or protection limit was changed.
The normal handoff is not forced upward to10%; actual-output anti-windup, signed
P/PI, conservative RELEASE and break-before-make remain. Production changes are
in the existing machine, its diagnostic state and firmware/schema identity.

Normal verified handoff ends boost_active immediately and stores the last
observed sequence as its response boundary. It separately marks one response
evaluation pending. Existing validity, ordering, receive-age, raw/force limits,
feedback deadlines and contact-loss checks run first. The pending evaluation
then runs before control-grid skipping, ordinary P/PI or an executor update.
Rise >= the selected profile's excessive_rise_units faults with existing
Fault5/Detail19 and immediate OFF. A below-threshold first response consumes the
check once; later normal BUILD pressure does not stay attributed to that assist.

A qualifying after frame has a strictly later sequence than observed at handoff,
and a receive timestamp at/after handoff end. Signed modular time comparison and
existing serial-number ordering handle wrap. The same frame causing early
handoff is checked while assist is active, then excluded from the after slot.
A later sequence received in the same millisecond as handoff may qualify, even
when the control grid would skip it. A fresh-delivered frame timestamped before
handoff neither consumes pending evaluation nor runs normal control/renews lease.

Duplicate/out-of-order/over-age/invalid frames cannot consume pending state or
renew the receive lease. Fault/STOP preserve unevaluated pending state as evidence;
active=false prevents late telemetry from evaluating, changing the first fault or
reviving output. An explicit later session clears its diagnostic event while
retaining the existing cumulative budget. Raw trip/contact loss/gap/lease/session
policies retain their original priority. Waiting for after data adds no timer,
assist, powered-session or receive-lease budget. No automatic restart/retrigger.

## Wire/report semantics

Schema F107, build46530109:81 u32 +27 floats,216 frozen diagnostic words.
Config50/profile66/catalog198 FC03 words and11-word chunk bounds are unchanged.
The expanded snapshot's worst-case budget is2100 ms; the existing5 s capture
planner includes it automatically. CRC/station/function/length, catalog, active
config/profile digest and strict firmware identity/hash checks are retained.

- assist_pressure_peak: sampled peak while boost_active was logically true;
  not a peak measured continuously during physical PWM.
- assist_response_peak: that peak plus the first post-handoff frame actually
  evaluated while the session remains active. It freezes after that one check.
- assist_response_pending:1 means the normal-handoff check is still unevaluated;
  it may remain1 after STOP/fault, which is inactive evidence, not authorization.
- assist_after_result:0 unevaluated,1 evaluated below threshold,2 excessive.
- boost_pressure_after / boost_after_received_ms / boost_after_sample_hi/lo:
  first fresh ordered in-range after observation and its timestamp/sequence.
  Late STOP/fault observations may populate after data without evaluating it.

For the reproduced case, active sampled peak stays27, response peak becomes37,
after pressure37 and after_result2. The report explicitly separates these fields.
For STOP before response, after data may later show37 while pending1/result0 and
response peak27 remain: no assertion of an evaluated powered response is made.
Existing boost exit1/end-reason2 continues to describe normal peak handoff; the
later Fault5/Detail19 separately describes the post-assist protection stop.

Capture retains its tail fix: full bounded transactions with STOP reserve,
partial-snapshot discard, genuine errors preserved, and UNKNOWN lost START echo
never retried. The new post-assist frozen event is tested through production
length parsing to CSV, metadata and report. No first-crossing HOLD claim.

## Actual live limits and remaining experimental conditions

| Status / source | Facts |
| --- | --- |
| Given by user / preserved source | Candidates20/30/40%=4800/7200/9600; separate normal ceiling2400; RELEASE100; Kp10/Ki0/Kd0; PSU setting0.5 A unchanged |
| Enabled compiled live profile4 | PRESS720/RELEASE100; approved peak budget0; no assist; absolute5 s; target1..275 legacy counts/raw trip325; age20/gap125/lease130 ms |
| Hardware evidence described by user |3% command720/CCR144, pressure27 to27 and no motion, PSU display0.028 A; final STOP removed observed PWM. No new physical verification here; active STOP/feedback-loss qualification incomplete |
| Synthetic only | Rise3/end9/hard12/total12/OFF1000 ms; response2/excessive-rise10/taper20 counts; real production-path timer mappings and89 handoff proven in host register shims |
| Undetermined / no reviewed source supplied | Actual higher-output on-time/current/thermal envelope, exact rise/end/cutoff/cumulative/OFF times, normal10% powered-duration allowance and pressure-exit thresholds suitable for hardware |

To enable one high-output experimental profile, the specific missing values are:
selected assist candidate (20,30 or40%, not automatic escalation); assist_rise_ms,
assist_end_ms, boost_ms independent cutoff and boost_total_ms cumulative exposure;
required off_ms; allowed normal PRESS cap up to2400 and its energized/session/
build/capture durations (no extension of current5 s here); taper_margin,
response_units and excessive_rise_units in controller units. Each needs a reviewed
source for the actual actuator/driver/supply/load setup and allowed exposure.
Profile values are currently zero UNREVIEWED sentinels and enabled0, not zero-time
permission. Keep0.5 A; it is a supply setting, not measured winding current or a
thermal/current rating. Synthetic3/9/12 ms and1000 ms are not proposed field ratings.
No additional unknown value was invented to make the profiles runnable.

These output-experiment conditions are separate from N calibration and the
sensor/mechanical/current-time qualifications needed for a3000 N range. Unit0 is
counts-to-counts, not a fabricated1 N/count; qualification bits remain0. Missing
old HEX identity limits attribution of historical observations, but is **not a
blocker to this software fix**, which is reproduced, corrected and tested.
Do not request another known3% no-motion performance run. No new field test is
authorized by this patch. README's exact inherited command is a simulated tool
contract, not an instruction to run it now. HARDWARE_STOP_VALIDATION / STATIC_250 /
ROTATING_LOAD remain unvalidated. See TEST_RESULTS.md and verification.json for
actual commands, red/green evidence, strict artifacts and validation limits.
