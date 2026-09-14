# Runtime characterization protocol - F109 / build4653010B

All transport checks remain mandatory: station1, exact function/length, CRC,
100 ms transaction timeout; FC03/FC04 reads at most11 registers per request.
FC04 info0x100..107 supplies schema, lock,50 config words,226 frozen diagnostic
words, config version/digest. FC06 snapshot0x102=D101 freezes86 u32+27 float fields;
FC04 reads0x200..0x2E1. Canonical scalar wire order is high16-bit word first.
The struct is never sent as a host C ABI. The schema JSON lists exact field order.

Active FC03 config0x110 has25 float32 fields. Active profile0x180 has33 float32
fields; runtime profile ID5/unit2/source3. Only profile peak_press and
continuous_press vary with a committed plan. Config press_cap equals the latter;
all other config fields must equal the compiled defaults. The three historical
catalog entries at0x400 remain disabled and are not selectable for this build.

## Atomic runtime plan

IDLE, no pending START, logical/physical output OFF are required for all writes.

1. Read active plan0x540..54B to obtain current uint32 version.
2. FC06 0x500=B501 invalidates previous START authority and opens staging.
3. FC06 writes **all10** words0x510..519: target_N float32 (integer1..3000),
   assist_percent float32 (0..40), continuous_percent float32 (0..10),
   expected base version uint32, requested plan digest uint32.
4. FC06 0x501=C501 validates finite values, bounds, complete mask, base version
   and digest together. It atomically installs target/profile/config, increments
   version and leaves START unarmed. Version exhaustion fails closed, no wrap replay.
5. FC03 read **all12 words**0x540..54B: the three requested float32 fields (6 words),
   derived assist command uint16, continuous cap uint16, version uint32, digest
   uint32. Verify each requested value, both commands, version and digest.
6. Echo readback version and digest into0x520..523, then0x524=A501. Firmware also
   requires complete active-plan read coverage before arming. Partial/stale/mismatched
   plans or acknowledgements invalidate permission; no fallback to an old plan.
7. FC05 START0x10=FF00 consumes authority exactly once. A new fresh sensor frame
   within250 ms rechecks the complete profile before starting continuous ownership.
   Missing/stale/invalid pressure cannot energize. Extra START has no restart path.

FNV1a digest starts2166136261, xor each byte and multiply16777619 modulo2^32,
over the three float32 values in listed order, each in canonical little-endian
IEEE754 bytes. This detects plan inconsistency; it is not cryptographic
authentication. Commands use nonnegative half-up round(float32(percent*240)).
Readback comparisons use the same float32 encoding. No runtime gain/timing knobs.
An engineering Parameters command is rejected on this firmware even while OFF.
Historical firmware engineering tools retain their motor-disconnected restriction.

STOP is the existing priority coil0x1=0000. It bypasses the normal queue, turns
OFF, revokes plan authority and preserves fault/event evidence. FAULT_RESET does
not refund reservations, remove lockout, re-arm an old plan or start output.
Only a new deliberate plan/START after the5000 ms administrative lockout can open
a new session with one4 ms reservation. It does not establish thermal recovery.

## Persistent diagnostics

The appended u32 fields are run_reason, target_reached, target_reached_ms,
plan_version and plan_digest. Run reasons:0 none,1 operator/PC STOP,2 fault,
3 TARGET_NOT_REACHED_WITHIN_SESSION,4 SESSION_COMPLETE after target reach,
5 BOUNDARY_TARGET_REACHED (OFF, no HOLD). MCU timestamps wrap modulo2^32.
Reasons3/4 are retained for historical report interpretation; this runtime profile
has no overall session deadline and does not produce them. Config session_ms and
profile energized_ms/session_ms/build_ms/capture_ms are fixed0 sentinels. They do
not disable the normal amplitude budget, receive lease, assist cutoff or fault
latch. An expired receive lease remains a lease fault even after5 seconds.
Contact20 is latched on the first valid contact during approach, without renewing
session timing or assist authority. It is not a low-target trip in runtime unit2.

Existing boost_peak_command, assist_peak_ccr, boost_elapsed_ms/boost_spent_ms,
boost_handoff_command, boost_pressure_before/after and after receive sequence
survive STOP/fault. Logical active peak stays distinct from post-assist response
peak. Detail20 is the characterization post-assist >=25 N abort; Detail19 is the
active-assist abort. Post-assist pending protection remains before control-grid
processing and any ordinary increase, without lease renewal from old feedback.

raw/latest_raw retain raw_pressure_counts. unit2 measured/force_N is measured_force_N
under USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT, scale1 offset0. The16-bit
raw field, valid/order/freshness checks and raw trip stay independent protections.
At target3000 the first fresh valid raw >=3000 is a terminal OFF event, not an
invitation to drive beyond the boundary; other targets fault at raw >=3000.
No sensor-saturation assumption and no higher mechanical/calibration certificate.

CSV records actual continuous command and state over time. Metadata records the
verified plan, config/profile/digests, host repository commit and repository HEX
hash, operator flash attestation, gap and unchanged0.5 A PSU setting. Reports
include MCU target/termination events, before/after assist delta, sampled HOLD
min/max/mean and MCU dwell. Missing response/motion/current/temperature remains
unknown. PSU setting is not winding current; planned CCR is not a scope trace.

Runtime SingleStart capture has no observation deadline. Metadata maximum_observation_seconds
is null; capture_end_policy is OPERATOR_STOP_OR_DEVICE_TERMINAL_OR_FAULT. Complete
snapshots are flushed as CSV rows throughout the run; interrupted partial snapshots
are counted and discarded. STOP is attempted before final reporting. Observe-only
diagnostics and all transactions retain their existing finite timeout checks.
