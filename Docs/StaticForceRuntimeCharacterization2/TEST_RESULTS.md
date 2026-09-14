# StaticForceRuntimeCharacterization2 actual software results

Software checks PASS. physical test NOT RUN. No serial port, flashing, motor
motion, force/current/temperature measurement or physical STOP test was performed.
The baseline was clean and matched fetched origin/main:
36c9416c098d63a3ed7111034101616875573fa9.

| Actual check | Result |
| --- | --- |
| Existing host regression |33 host groups +22 compile-policy cases PASS |
| Plain locked ForceServo |15 groups at each O0/O2/Os PASS |
| Existing commissioning ForceServo |42 groups at each O0/O2/Os PASS |
| Existing synthetic range/boost/post-assist fixture |55 groups at each O0/O2/Os PASS |
| Runtime characterization production chain |15 groups at each O0/O2/Os PASS |
| Existing production length parser/full Observe |46 cases PASS |
| Runtime capture |24 cases +6 invalid-input cases PASS |
| Force/pressure/AUTO capture self-tests |PASS, no serial I/O |
| Existing schema/report checks |11 schema/self-test +15 report tests PASS |
| Runtime schema/report |5 tests PASS |
| Compile/arming gates |14 cases PASS |
| Strict verifier rejection suite |28 tests PASS |
| ARM Release + separate clean rebuild |PASS; exact HEX and ELF byte equality |
| Strict offline verifier + developer objcopy |PASS;41,688 addressed load bytes including RAM initializers |

New production tests advance simulated MCU time through12 seconds of valid
progress at targets250/500/1000/2999/3000,70 seconds of ongoing build, and70 seconds
of active HOLD. They prove no replacement30/45/60-second deadline, retain the
receive-anchored130 ms lease, and classify late lease expiry as a fault. Target3000
turns OFF without HOLD both in active BUILD and on the first valid post-START
frame; stale boundary pressure cannot count as target attainment. Other targets
still fault at raw3000. STOP, invalid/stale feedback and independent assist cutoff
remain fail-closed; STOP/fault never rearms or restarts output.

Zero-force START uses normal continuous approach for6.5 simulated seconds before
contact. Duplicate, stale and invalid feedback cannot grant an assist. First fresh
contact at20 N enables the single1/2/4 ms plan, followed by verified lower-command
handoff and unchanged post-assist response evaluation. Contact/dropping below20 N
does not reset the session or four-ms assist reservation. Targets1 and19 work
below the contact threshold; even an initially contacted run can unload into a
permitted low-target HOLD. Separate no-response tests require the unchanged
conditional5000 ms fault, rather than confusing it with an overall session stop.
Existing anti-windup, trajectory, output caps/slew, direction OFF interlock,
TIM5 pending-expiry races, STOP queue priority and post-assist tests also pass.

The capture test transport sends every simulated response through production
Read-LengthAwareResponse/Get-ModbusResponseLength and station/function/CRC checks.
Successful runs begin with0 N readback, continue beyond5 simulated PC seconds,
observe target/HOLD, and stop only at injected manual STOP after12 seconds.
Target3000 instead terminates at the device's boundary OFF event without HOLD.
CSV content is inspected while the run is still active, proving incremental
persistence. A late partial-frame communication timeout after6.5 seconds remains
a capture error with one STOP and no START retry. The suite also covers zero
settings, stale/mismatched plans, gain/session/build mutation rejection, lockout,
cancel, failed plan reads, lost START echo, real device faults and the four literal
README command examples. Reports retain historical session-timeout decoding while
correctly reporting current operator-ended HOLD and first boundary reach.

Strict verification retains all hashes, addressed HEX/ELF matching, HEX checksum,
ELF type/bounds, output guard, every fixed config/profile/catalog field and all
runtime-contract words. The preceding RuntimeCharacterization1 ELF is explicitly
rejected as the current firmware. Empty-PATH and optimized-Python rejection tests
pass; field validation requires no ARM executable. Developer objcopy cross-check
and a separate ARM build agree with the pinned field pair. ARM Release size:
text41,588, data100, bss4,576 bytes. Schema F109/build4653010B;50 config words,
66 profile words,226 diagnostic words and198 retained disabled-catalog words.

HEX SHA256: C7915B062D9E9016717B862BE15218ADF5C96645C6045B60CD101F6117F7972F

ELF SHA256: 953C06FF39F46B8C5212D94807D82A91CA7882D6C4E7C18EC9909A2914123AA5

[verification.json](verification.json) records all21 final component commands,
actual UTC starts, durations, exit codes and raw-log SHA256 values. Readable
[software_test_output.txt](software_test_output.txt) retains the actual output
with line-ending/trailing-whitespace normalization only. Two development failures
are recorded separately: a long low-target fixture failed its active-state
expectation after falling pressure (source inspection identified retained tracking
expiry; the earlier fault code was not captured), and the initial capture validator
still assumed a finite session. Neither fix removed a safety assertion to obtain PASS. Local raw
logs and SYNTHETIC CSV/report/metadata are retained under this candidate's output
directory; generated executables/objects and duplicate build outputs are removed.
Historical firmware and original field evidence remain unchanged. No ZIP.

NOT RUN: HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD,3000 N qualification,
physical assist timing, current measurement, thermal or continuous ratings and
mechanical acceptance. Software PASS does not establish physical behavior or
safe thermal exposure, and does not promise reaching any target.
