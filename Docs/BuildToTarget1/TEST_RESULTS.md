# BuildToTarget1 ? actual software results

**PHYSICAL_STATUS=NOT_RUN.** Baseline main9de33246b67603e797ff31cc8472f54b9bd0520d was clean and equal to fetched origin/main. No serial connection, flash, motion, physical STOP, force/current/temperature measurement or ZIP generation was performed.

| Actual check | Result |
|---|---|
| Existing host / compile-policy regression |33 groups +22 policy cases PASS|
| Plain locked ForceServo |15 groups at each O0/O2/Os PASS|
| Existing commissioning ForceServo |42 groups at each O0/O2/Os PASS|
| Existing synthetic range, boost and post-assist |55 groups at each O0/O2/Os PASS|
| Historical RuntimeCharacterization2 |15 groups at each O0/O2/Os PASS|
| BuildToTarget1 production machine/executor/HW/TIM5 chain |13 groups at each O0/O2/Os PASS|
| Existing production response parser / full Observe |46 cases PASS|
| Historical runtime capture |24 cases +6 rejected inputs PASS|
| Current BuildToTarget1 capture |27 cases +6 rejected inputs PASS|
| Force / pressure / AUTO capture self-tests |PASS, no serial I/O|
| Existing data/schema/report |11 self-tests +15 report tests PASS|
| Runtime2 / Build schema/report |5 +5 tests PASS|
| Compile/arming gates |19 cases PASS|
| Strict verifier rejection suite |32 tests PASS|
| Strict current and historical AUTO firmware verifiers |PASS|
| Candidate ARM Release and independent final rebuild |PASS, exact HEX and ELF bytes equal|
| Developer objcopy cross-check |PASS; field verifier remains pure Python|

Final ARM size: text47,068, data100, bss4,944 bytes;47,168 addressed load bytes including RAM initializers. SchemaF10A/build4653010C,50 config words,66 profile words,198 disabled-catalog words,52 immutable Build config words and284 frozen diagnostic words. No ordinary continuous owner is admitted; strict verifier pins its literal unconditional INVALID return and requires the independent segment implementation, actual output guard and TIM5 cutoff functions.

HEX SHA256:21258CF7E2F4D9D0ADDDA251DF482852BC7F3F087A81E475BDE548DA0EEED6F2

ELF SHA256:F98652424AACE83A525BBEBCD85EC1A3D81190B0CD5FC5638D0CDC53F06DC2E3

The13 Build groups cover0 N approach, low targets1/3/5/10/20, initially attained/exceeded targets, synthetic250/500/1000/2000/3000 attainment followed by10 s force decay with bridge OFF and no HOLD/repress. Target3000 on the first valid post-START frame is OFF before any segment; raw3000 for target250 faults. Initially IDLE at raw trip remains rejected, preserving overforce protection.

The0?100 N replay takes over9 s, then pauses600 ms without Detail17; slow1 N per2 s increments accumulate to credible2 N net progress. Truly constant force and alternating1 N noise eventually fault22. STOP/new plans retain no-response/exposure budgets, and deliberate fault reset plus a new plan cannot buy one extra pulse after exhaustion. Reserved approach8 s and total12 s stop additional admission; only full108 s uninterrupted OFF clears the epoch. Shorter OFF, pulse transitions, deliberate START/STOP and reboot do not bypass it (reboot requires a full wait).

Executor tests cover wrong request/token, duplicate/stale/during-pulse response, exact30 ms post-end acceptance, independent normal/hard cutoff without machine service, STOP before/during commit, pending cutoff at arming, pending/elapsed cutoff during an early reduction, old caller timestamps, invalid amplitude/duration/energy, lease anchoring, hardware OFF and no automatic restart. Boost reaches7000 at hard4 ms with CCR1400, retains authority after valid rise and resets low-response count; taper energy decreases to zero at/above target. Every post-event retains a matched before/after/request while the next segment is pending. Existing trajectory, numeric/D and actual-output anti-windup tests remain included. New diagnostics are command/register/MCU-time evidence, not measured electrical output or winding current.

Current capture tests pass every simulated reply through production Read-LengthAwareResponse/Get-ModbusResponseLength, station/function/CRC/length checks and real chunking. Observe is ZERO START. Successful single-start cases0?targets1/5/250/500/1000/2000/2999/3000 keep streaming beyond5 s and after targetOFF; a manual STOP at12 simulated seconds ends the run. CSV is inspected while active. Reports record1 N OFF decay separately from target reach, never active HOLD. Gain/config/build/profile/plan/digest/version/exposure/no-response mismatch and cancellation reject START; lost START echo, real fault and late partial-frame timeout never retry. Diagnostic identity corruption prevents a false StopVerified even though STOP is sent. The literal current README Target-only command is parsed and exercised without opening a serial port.

Verifier tests retain checksums, strict addressized HEX/ELF load equality, ELF class/type/machine/ABI/bounds, actual hashes, output guard, all25 configuration floats,33 profile floats,99 disabled-catalog floats,16 characterization-contract words and26 Build config words. Mutating any protected Build word, enabling the continuous owner, deleting required implementation or supplying Runtime2/historical firmware is rejected. Empty PATH and optimized Python remain checked; no ARM executable is needed on the field PC.

Actual commands, UTC timestamps, durations, exit codes and raw-log hashes are recorded in [verification.json](verification.json). [software_test_output.txt](software_test_output.txt) contains their output with line-ending/trailing-whitespace normalization only. The Build host evidence selects the later arm-final-5 run after the final pending-cutoff fix; all other final components come from final-verification-2. Both final ARM builds match the pinned pair. Reproduce the full suite with `python tools/verify_build_to_target.py --output output/BuildToTarget1/NEW_DIRECTORY` (a fresh path is mandatory).

Development failures were retained: initial host macro conflict; an invalid initial-IDLE-at3000 fixture (corrected to first post-START boundary, without weakening raw protection); an expiry injection before timer arming (corrected to exercise the armed timer); insufficient approach iterations for budget exhaustion; an overly strong expected StopVerified on corrupt diagnostic identity (corrected to require false); incorrectly escaped README command text; and a verifier assumption that old continuous helpers would be eliminated by linking (replaced with the stronger applicable unconditional owner-denial check). The first aggregate rebuild comparison failed because source improvements had advanced the build while the unpublished candidate still carried older hashes; it was not labeled PASS. After freezing the final contract, the current pair and independent rebuild pass. Earlier local logs, failed assertions, SYNTHETIC CSV/metadata/reports and previous final-verification evidence are preserved; no old field evidence was edited.

Not run: HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD, physical pulse timing, motor response, thermal/continuous ratings, mechanical qualification and physical1000/2000/3000 N attainment. Synthetic PASS cannot establish those outcomes or promise reaching the configured target within the finite safety budgets.

Cleanup limitation: automatic command approval rejected both the scoped generated-file cleanup and a single explicit generated executable deletion; the only supplied reason was `blocked by policy`. Ignored generated executables/objects/build copies are therefore retained locally alongside logs. They are not part of the tracked delivery; the current HEX/ELF and original evidence remain intact. No permission prompt or destructive alternative was used.
