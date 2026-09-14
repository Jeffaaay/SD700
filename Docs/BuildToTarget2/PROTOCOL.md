# BuildToTarget2 protocol / capture contract

Identity: schema0xF10B, build0x4653010D, live profile7/unit2. The existing station1,115200/8N1, FC03/04/05/06 framing, CRC, exact response length and100 ms transaction timeout remain. Every read is split into<=11-register chunks; no CRC/length/error relaxation or START retry.

- FC04 capability0x100:50 config words,300 diagnostic words.
- FC03 active config0x110:25 floats; all firmware-owned for this candidate.10/0/0, reference200/1000, age20/gap125/lease130 are unchanged. The ordinary continuous press_cap is0 because continuous output ownership is unavailable.
- FC03 profile0x180:33 floats, profile7, contact10, target range1..3000, all normal elapsed session/build/capture fields0. Old peak/continuous plan-derived fields must be0. The historical Assist1/2/4 fields are retained but have no Assist admission in this candidate.
- FC03 disabled catalog0x400: unchanged198 words, entries20/30/40 disabled and unqualified; not a runtime selection/unlock mechanism.
- FC03 **immutable Build profile0x600:66 words (33 u32)**. High word first; field order and fixed values below. No write range exists. FNV-1a over each u32's little-endian bytes gives digest1817994819. Both complete values and diagnostic digest are checked before START.
- Atomic plan remains FC06 BEGIN0x500,10 staged words0x510, COMMIT0x501, full12-word FC03 readback0x540,4 ACK words0x520, ARM0x524. Plan contains target float and two reserved zero floats (formerly AssistPercent/ContinuousPercent), source version and digest. Partial/stale/mismatched plans, nonzero reserved slots or generic field tuning are rejected. Readback-derived assist_command/continuous_cap are both0. This is not a claim of zero bounded-segment authority.
- Exactly one existing START coil0x10 request is consumed after complete readback/ACK and human confirmation. MCU still waits for the next fresh post-START pressure frame before output. Boot/full-rest inhibition, pending post-pulse evidence or exhausted no-response budget blocks START; fault reset does not grant an extra pulse. STOP retains priority and revokes plan authority.

| u32 index | Field | Value |
|---|---|---|
|0|version|2|
|1|approach_command|5000|
|2|contact_N|10|
|3|approach_hard_ms|100|
|4|approach_total_ms|8000|
|5|micro_min_command|800|
|6|micro_max_command|3000|
|7|boost_step_command|300|
|8|boost_max_command|4000|
|9|build_ceiling|7000|
|10|pulse_base_ms|10|
|11|pulse_hard_max_ms|11|
|12|pulse_hard_min_ms|3|
|13|taper_margin_N|50|
|14|fine_margin_N|3|
|15|fine_min_command|400|
|16|fine_max_command|1000|
|17|fine_boost_max_command|1000|
|18|off_settle_ms|30|
|19|total_on_ms|12000|
|20|full_rest_ms|108000|
|21|no_response_ms|5000|
|22|progress_N|2|
|23|low_response_N|1|
|24|low_response_count|2|
|25|excessive_rise_N|25|
|26|pulse_min_ms|2|
|27|hard_guard_ms|1|
|28|coarse_check_ms|200|
|29|coarse_step_command|500|
|30|coarse_max_command|3500|
|31|approach_ceiling|8500|
|32|approach_command_ms_budget|40000000|

Frozen FC04 diagnostics start0x200, end0x32B. Existing86 u32 fields are followed by32 Build u32 fields, then existing27 floats and5 Build floats, exactly as `tools/force_servo_data.py --build-to-target-schema` and the C serializer specify. New fields cover:

- `build_mode`, immutable config digest, build phase, segment request/phase/command/hard time/start/deadline/end/reason;
- pending response plus persistent last valid response request/received time/64-bit sequence;
- exposure epoch, reserved total/approach hard time, bridge-enabled elapsed upper estimate, full-rest remaining/inhibit;
- boost command, consecutive low-response count, cumulative no-response time;
- persistent paired `pulse_force_before/after` and cumulative progress anchor.

`post_pulse_valid` refers to the last completed response and can coexist with `post_pulse_pending` for a newer segment. `post_pulse_request` identifies that pair; do not pair it with the current `segment_request`. Sequence/timestamp validity is causal: accepted after actual segment end+30 ms, not a during-segment frame. Snapshot readback can miss an entire short pulse; the persistent event fields prevent assigning its response to the next command. Raw diagnostic pressure is not interpolated.

Build phases:0 idle,1 approach,2 build,3 taper,4 targetOFF. Machine states12 approach,13 build,15 taper,16 targetOFF monitor; no state14 active HOLD. TargetOFF is an active monitoring state, so freshness/invalid/overforce protections remain. It never re-enables output if force drops. STOP exits to IDLE (or preserves FAULT) with output_off1, current0, TIM2/3 zero and no lease.

New fault details:21 exposure budget (Fault5),22 true no-force-response (Fault5),23 execution cutoff/owner contract (Fault6),24 excessive post-pulse rise>=25 N (Fault5). An earlier runtime/executor hardware guard may report its original hardware/lease detail; no fault is reclassified as successful completion. Existing sensor invalid/order/stale, overpressure, STOP/urgent runtime handling and fault latching are retained. Detail17 does not terminate BuildToTarget2; tracking time is diagnostic. Target-reached run reason6 (below3000) or5 (3000) remains persistent through manual STOP; `fault/detail` still report any later real fault.

The current field wrapper accepts Target only and passes two reserved zeros. It prints the actual fixed stage ceilings/time/thermal reservations before one START. It never changes supply current limit. Capture has no normal overall deadline; targetOFF continues sampling until operator STOP, external device STOP or a real fault. CSV is flushed throughout the session; CSV/report/metadata are created only at new paths. Report separates first target touch from sampled OFF-force retention/decay; no stable holding or physical-stop qualification is inferred. Failed identity readback still sends STOP, but cannot prove StopVerified.

**PHYSICAL_STATUS=NOT_RUN.** This protocol and the synthetic fixture do not establish measured current, temperature, physical pulse duration, mechanical stop or force attainment.

Added diagnostics: segment_base_command, segment_mode (1 COARSE/2 MICRO/3 FINE), segment_normal_ms, coarse_boost_command, coarse_check_ms, approach_command_ms; requested_equivalent_V and mapped_pwm_percent. Existing current_committed and tim2/tim3 disclose the actual software commit and planned CCR. These are not measured electrical outputs. See the root OLD_TO_NEW_CONTROL_MAP.md for source-derived timing and per-mode ceilings.
