# BuildToTarget2 - actual software results

**PHYSICAL_STATUS=NOT_RUN.** Baseline main/origin/main1a4c93d57d386e25efa300a64828e2fef5a67071 was clean and synchronized. No serial connection, flashing, hardware movement, current/voltage/temperature measurement or ZIP creation. Historical firmware and field evidence remain unchanged.

| Executed check | Result |
|---|---|
| Existing host regression / compile policies |33 host groups +22 policy cases PASS|
| Original locked ForceServo |15 groups at each O0/O2/Os PASS|
| Existing commissioning ForceServo |42 groups at each O0/O2/Os PASS|
| Existing range/boost/post-assist fixture |55 groups at each O0/O2/Os PASS|
| Historical RuntimeCharacterization2 |15 groups at each O0/O2/Os PASS|
| BuildToTarget2 machine/executor/guard/TIM5 tests |16 groups at each O0/O2/Os PASS|
| Production response parser and full Observe |46 cases PASS|
| Historical runtime capture |24 cases +6 invalid inputs PASS|
| Current Build capture through production parser |27 cases +6 invalid inputs PASS|
| Force / pressure / AUTO capture self-tests |PASS, no serial I/O|
| Existing data/schema/report |11 schema self-tests +15 data tests PASS|
| Runtime2 / Build schema/report |5 +6 tests PASS|
| Compile/arming gates |19 cases PASS|
| Strict firmware verifier rejection tests |33 tests PASS|
| Strict current and historical AUTO verifiers |PASS|
| ARM Release and independent rebuild |PASS; both exact HEX/ELF hashes match the final pair|
| Optional developer objcopy comparison |PASS; field verification is Python-only|

All23 components of `python tools/verify_build_to_target.py --output output/BuildToTarget2/verification-1` passed. The final Build host result is replaced by the later `python tools/verify_build_to_target.py --output output/BuildToTarget2/final-host-fine --only build_to_target_force` run, after the final STOP/readback budget guard and additional in-machine FINE boost/reset assertions. No other production change followed the verified final ARM rebuild.

Actual command arguments, UTC timestamps, durations, exit codes and original log SHA256 values are in [verification.json](verification.json). [software_test_output.txt](software_test_output.txt) includes complete selected command output with only line-ending/trailing-whitespace normalization. Every selected raw log hash was checked before this report was generated. Reproduction uses the same runner with any new evidence path; existing evidence paths are never overwritten.

Final ARM: text47,548 bytes, data100, bss5,032;47,648 addressed load bytes including RAM initializers. F10B /4653010D / profile7, config version2/digest1817994819. Readback:50 active config words,66 profile words,198 disabled-catalog words,66 immutable Build config words,300 frozen diagnostic words. The strict verifier retains all actual identity/configuration/lock/guard/owner/HEX/ELF/hash assertions and mutates every33-u32 Build config word independently of hashes. Continuous admission remains an unconditional INVALID return. BuildToTarget1 and older firmware are explicitly rejected as current. Python with empty PATH and optimized Python checks still pass.

Final HEX SHA256:248E4D0149AD4DEC418EBE6D7E1EF7768EEF2CB9B36F02DFCBC2BAA39958B141

Final ELF SHA256:D4D704893E9188F16F29D43B64A423B7B5E4B4079AB49EC1414C792CF4E78740

The16 Build groups exercise all16 requested behaviors.0 N enters independent APPROACH; contact10 N latches adaptive MICRO and cannot return to COARSE within START. Two low post-pulse movements add300; good or negative>=1 N movement clears boost/count, while only cumulative new-high>=2 N resets no-response time. FINE is exercised in the real machine through boost1000, response reset and targetOFF. COARSE checks199/200/399 ms boundaries,500-command steps,3500 additive ceiling, reset, actual8500 command and planned CCR1700 through the production executor.

Source-derived amplitude/duration vectors cover MICRO/FINE interpolation,4000/1000 boost ceilings,10 ms compensation, minimum2 ms and independent hard guard. The old error4->3 N formula discontinuity is explicit in the source map and tests; no false strictly monotonic claim. A representative error5->2 transition with boost300 reduces command-time. The executor rejects invalid base/mode/ceiling/duration combinations and a high MICRO command disguised as FINE. At7000 the actual normal timer request is4 ms, hard5 ms, CCR1400. No direct old PWM driver or ONE Assist retrigger supplies output.

A synthetic0->100 N increase over9 s followed by600 ms plateau continues without Detail17. Slow1 N per2 s progress accumulates; sustained flat force/alternating1 N noise stops on the5 s cumulative no-response budget. STOP/fault/new plan cannot erase budgets; terminal pressure readbacks cannot reset them. Tests retain8 s approach and12 s overall hard reservations,108 s uninterrupted OFF/reset and boot lockout, with the added40M command-ms approach cap consuming budget faster at8500. These are software exposure limits, not thermal/current measurements or mechanical-stall detection.

Synthetic250/500/1000/2000/3000 targets reach true bridge OFF and remain OFF through10 s falling force. Low targets and initially attained/exceeded targets are covered.3000 on the first valid post-START frame is OFF before a segment; lower-target raw3000 faults. STOP is exercised during approach, BUILD, MICRO taper and FINE, plus commit/IRQ races; no automatic restart. Missing/bad/stale/duplicate/out-of-order feedback, request/token mismatch, settle timing, pending hard cutoff, old caller timestamps and independent normal/hard OFF remain checked.

The Windows full-capture fixture passes every simulated response through production Read-LengthAwareResponse/Get-ModbusResponseLength, station/function/CRC/exact length and chunking. Observe remains ZERO START; one-start cases continue beyond5 s, record target-OFF force decay and end on the simulated operator STOP. New config and equivalent-voltage/command/CCR/coarse-budget telemetry survive framed FC03/FC04 readback into CSV/metadata/report. This is a transport/schema fixture, not a plant model or proof of physically reaching a target. Invalid config/digest/gain/exposure/plan/cancel paths refuse START; fault/timeout/lost START echo never automatically retry.

Development evidence is retained locally: dev_host1 failed an incorrectly updated exposure expectation (22 versus the fixture's two10-ms reservations); dev_host2 expected recovery from an old pressure sample even though the unchanged safety path correctly latched FAULT. Tests were corrected to preserve that fail-closed behavior and separately exercise valid feedback. Later dev/final host runs pass. A final review moved post-response adaptation behind executor acceptance and prevented terminal readbacks from altering the cumulative progress budget; the final build and additional host run include it. The initial ARM development hashes are superseded only for this unpublished candidate; prior published field pairs are untouched. Existing ignored development outputs/logs remain on disk.

Not run: physical pulse timing, motor movement/force response, physical electrical/thermal measurements, HARDWARE_STOP_VALIDATION, STATIC_250, ROTATING_LOAD, and physical500/1000/2000/3000 N attainment.2000 N is synthetic logic validation only. No software result authorizes a motor/continuous/thermal rating or guarantees physical attainment within the safety budgets.
