# StaticForceRuntimeCharacterization1 ? actual software verification

Software checks **PASS**. **physical test NOT RUN**. No serial port was opened,
no flashing was performed and no motion, force, current or temperature was measured.
The supplied installed-sensor N semantics and experimental envelope are user
instructions; host shims are not new field evidence or hardware qualifications.

The final source was tested at O0/O2/Os and built with ARM Release. A separate
clean build directory produced byte-identical HEX **and ELF**. Strict pure-Python
verification passed with addressed load bytes, RAM initializer load addresses,
HEX checksums, ELF bounds/type, exact identity/defaults/profile/runtime envelope,
output guard and real pinned hashes; developer objcopy independently agreed.

| Actual check | Result |
| --- | --- |
| Existing host regression | 33 host groups +22 compile-policy cases PASS |
| Plain locked ForceServo | 15 groups at each O0/O2/Os PASS |
| Existing commissioning ForceServo | 42 groups at each O0/O2/Os PASS |
| Existing synthetic range/boost/post-assist fixture | 55 groups at each O0/O2/Os PASS |
| New production characterization chain | 13 groups at each O0/O2/Os PASS |
| Existing length-aware capture/full Observe suite | 46 cases PASS |
| New characterization capture | 21 cases +6 invalid input cases PASS |
| Force/pressure/AUTO capture self-tests | PASS; no serial I/O |
| Existing schema/report checks | 11 schema/self-test checks +15 report tests PASS |
| New characterization schema/report | 4 tests PASS |
| Compile/arming gates | 14 cases PASS, including missing commissioning/arming and invalid overrides |
| Strict firmware verifier rejection suite | 27 tests PASS, including all16 characterization contract words and all25 config fields |
| ARM Release + independent rebuild | PASS; exact HEX/ELF equality |
| Offline verifier + developer objcopy | PASS; field verifier needs no ARM executable |

The13 new production groups exercise arbitrary target/assist/continuous values,
288 valid mapping combinations, invalid/NaN/out-of-range plans, atomic masks,
stale version/digest rejection, required full readback/ACK and single-use START;
all fixed gains/filter; zero assist and zero ordinary PRESS; actual continuous
ceilings0/720/1200/1800/2040/2400 through the executor and TIM3; P-only taper
2200?100?0 with target250; one assist/session;1/2/4 ms timing; missing-main/TIM5
cutoff and pending-expiry race; <=cap handoff without receive-lease renewal;
normal versus excessive first post-assist response; STOP during assist/handoff/
HOLD; priority STOP under queued Modbus traffic; no auto-restart or retrigger;
contact/raw/feedback validity; five-second high-target attempts; conservative
4999 ms session compare before motor Service; wrap-safe sequence/time/lockout;
3000-boundary OFF both during BUILD and on the first valid post-START frame.
Existing trajectory, D/numeric, actual-output anti-windup, executor direction
OFF/deadtime and lease-race tests also execute in that dedicated build.

The new21 capture cases include Observe ZERO START, each requested target example,
intermediate25/35% settings, zero settings, altered gains, stale or mismatched
plan/readback/digest, lockout, operator cancel, plan-read timeout,
unknown START echo and runtime fault. The four literal README commands are parsed
and run through the same production capture core. Every simulated response goes
through production Read-LengthAwareResponse/Get-ModbusResponseLength plus normal
station/function/CRC checks. No complete-frame fake bypass proves field usability.

Implementation failures are retained in verification.json and local logs. The
first new synthetic frame generator emitted an incorrect byte-count, which the
unchanged production parser rejected. The corrected harness later exposed an
absolute-path handling error when launched by the full runner; that was fixed
and all21 cases rerun. Neither fix relaxed transport, hash or safety checks.
The final runtime audit added the conservative session-compare classification,
first-frame boundary and timestamp tests, then reran all four ForceServo variants.

Final ARM size: text41696, data100, bss4568 bytes; strict addressed load41796 bytes.
Schema F108/build4653010A;50 config words,66 active-profile words,226 diagnostic
words (86 u32+27 float),198-word retained disabled catalog. One current field pair:

HEX SHA256: `35A72464F8DBD5E3CFE1C8C72FDED97D7771BF8F2BD956603E77FC7EBF21D99E`

ELF SHA256: `60F5AB4CFF7A8793DB8C6C3BC3C7C082565C56D4ADC0F2CD7A4EA76B843C8A14`

[verification.json](verification.json) records21 final component executions,
actual commands, exit codes and raw-log hashes, together with earlier attempts.
[software_test_output.txt](software_test_output.txt) preserves their readable
output (line-ending/trailing-whitespace normalization only). Raw logs and clearly
labeled synthetic CSV/report/metadata remain locally under
output/StaticForceRuntimeCharacterization1; generated executables/build objects
are removable. The separate legacy host run has no stopwatch record; its duration
is explicitly unknown rather than reconstructed.

HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated. Also
NOT RUN:3000 N validation, motor/thermal/continuous ratings, current measurement,
scope timing, mechanical or production acceptance. No physical root cause,
thermal recovery time or target attainment is inferred from these software checks.
