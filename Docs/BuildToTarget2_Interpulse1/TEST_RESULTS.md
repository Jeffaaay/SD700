# BuildToTarget2_Interpulse1 actual software results

**PHYSICAL_STATUS=NOT_RUN.** Baseline main/origin/main5b41ad8dab6af67998ea40c0cb1bf1e141ecf54a was clean and synchronized. The user confirmed the Desktop old-source ZIP after the originally named dated archive could not be found. Actual source/member hashes and old lines are in [SOURCE_REFERENCE.json](SOURCE_REFERENCE.json). This release changes interpulse behavior through the existing executor, not the pulse amplitude/boost/duration controller.

All23 checks of `python tools/verify_build_to_target.py --output output/BuildToTarget2_Interpulse1/verification01` passed. Final comment/diagnostic-label cleanup was followed by `python tools/verify_build_to_target.py --output output/BuildToTarget2_Interpulse1/verification-final --only build_to_target_force arm_release rebuild_identity`: all3 passed against final source. [verification.json](verification.json) records all26 actual invocations, UTC start, duration, exit code and raw-log SHA256. [software_test_output.txt](software_test_output.txt) preserves full output with only line endings/trailing whitespace normalized. Original raw logs remain at recorded paths; hashes were checked when building this evidence.

| Actual check | Result |
|---|---|
| Existing host / compile-policy regression |33 groups +22 policy cases PASS|
| Original locked ForceServo |15 groups each O0/O2/Os PASS|
| Existing commissioning ForceServo |42 groups each O0/O2/Os PASS|
| Existing range/boost/post-assist fixture |55 groups each O0/O2/Os PASS|
| Historical RuntimeCharacterization2 |15 groups each O0/O2/Os PASS|
| Current Build machine/executor/TIM5 |24 groups each O0/O2/Os PASS|
| Production response parser / full Observe |46 cases PASS|
| Historical runtime capture |24 cases +6 rejected inputs PASS|
| Current Build capture |30 cases +6 rejected inputs PASS|
| Force / pressure / AUTO capture self-tests |PASS, no serial I/O|
| Existing schema / target data tests |11 +15 PASS|
| Runtime2 / Build schema-report tests |5 +7 PASS|
| Compile/arming gates |19 cases PASS|
| Strict verifier rejection/equality tests |35 tests PASS|
| Strict current and historical AUTO verifiers |PASS|
| ARM Release and separate rebuild |PASS; exact HEX and ELF equality|
| Developer objcopy comparison |PASS; field verifier remains Python-only|

Five new Build groups cover the production machine/executor/HW register shim/TIM5 chain:

- `TestInterpulseInitialMicroFineAndNoOff`: normal TIM5 ISR hands both MICRO and FINE to initial300 without main-loop assistance or restarting carriers. No output-OFF gap is accepted. Current200/600 bounds are admitted;0/199/201/601/700 are rejected. Old FINE400 can transition to an adapted600 preload; it is a separate bounded stage, not claimed to be a reduction in every case.
- `TestInterpulseDroopFreshGateAndPersistence`: exactly2 N drop does not increase preload; successive>2 N drops produce400,500,600,600. Both completed MICRO/FINE pulses share this rule. Duplicate and before30-ms frames cannot adapt/start a new pulse. New START preserves adapted preload and carried safety budgets; falling force earns no net-progress reset. Paired force telemetry remains associated with the completed request.
- `TestInterpulseEnergyDeadlineWithoutMain`: prepaid preload and pulse hard reservations reach the unchanged12000-ms cap. A shorter exposure deadline overrides the original receive lease; TIM5 alone disables output, revokes ownership and prevents restart. Full enabled wall time is charged without PWM-duty discount.
- `TestInterpulseStopFaultTargetAndLease`: STOP, bad/old feedback, overforce and target reach immediately OFF in either pulse or preload; later samples cannot restart output. Missing feedback trips the existing pressure guard. Receive-anchored expiry also turns OFF without machine polling, including20-ms-old admission.
- `TestInterpulseRacingCutoffAndNoReverse`: STOP, pending hard update and overcapture during both handoff directions cannot be cleared/renewed into output. A reverse owner remains rejected in this live Build, and negative/target error goes OFF. Existing legacy direction/break-before-make/release completion regressions remain in the full host suite.

Original coverage remains: fresh request/owner/sequence/age checks; pulse normal/hard deadlines and early taper; COARSE and PulseBoost exact source vectors; target OFF/decay and no automatic repress; synthetic250/500/1000/2000/3000 plus low/already-reached targets; sensor/STOP/fault paths; no-response noise/restarts and cross-START anchor repair; approach exposure and108-s uninterrupted cooling. The host TIM5 shim now models the Build OPM hard update as well as normal compare, and injection tests cover racing flags.

Some old test timelines assumed OFF gaps and therefore no longer fit the unchanged12 s exposure allowance. Synthetic higher-target ramps now use20 N/50 ms (still below the25 N excessive post-response threshold). The0-to100 N/600-ms plateau case and the slow accumulated1 N-per2 s case run as separate scenarios. The cross-START reproduction still performs10->150, STOP/unload0/wait5100/new plan/START, then second10 N plus1 N/100 ms through61 N, past the old59 N false-stop point. The subsequent flat61 N run stops after4900 ms plus100 ms already carried since the last>=2 N progress reset, exactly5000 ms no-response. Testing through160 N with energized gaps and then another5 s plateau would exceed the legitimate thermal budget. No protection parameter was relaxed.

The first development run really failed at the old assumption that400 additional pulses remained admissible after8 s of approach: [development_failure.json](development_failure.json), [development_failure.txt](development_failure.txt). It is preserved as FAIL. That fixture now runs until the same reservation cap rejects output, asserts preload time is included, retains approach8 s/40M accounting, and proves only108 s true OFF permits an epoch reset. STOP/restart cannot refund the reserved total or unused prepaid preload credit. Subsequent development and final suites passed.

The capture tests read all76 immutable Build words through production length-aware framing and expose preload in CSV/metadata/report. Altered initial/range/drop constants are rejected with ZERO START, along with all existing plan/config/safety rejection cases. The strict verifier independently mutates all38 Build words and preserves prior ELF/HEX/checksum/address/type/bounds/hash/guard checks, including empty-PATH Python-only operation. The previous StartAnchorFix1 and every older candidate are rejected as current firmware. [unchanged_surfaces.json](unchanged_surfaces.json) records the byte-identical32 original Build parameters, other config/profile/catalog/guard/owner-denial symbols, untouched controller/START/safety/HW source files, and all30 historical firmware files. Identity/layout are deliberately advanced toF10C/4653010F, Build version3/digest2848875769.

Final ARM size: text49624, data100, bss5104;49724 addressed load bytes including RAM initializers. HEX SHA256:BF145FFC2D3CCB7A3A9454C1F1F0EC23852D7DA6810B62D9F0C05D9F69CD978B. ELF SHA256:2A387A04708E543D47C79554E966ACF9E77D3A47712E365AB4FC10D791DC5F53. Paths are in [HANDOFF.md](HANDOFF.md) and the current firmware manifest. Historical evidence, failed logs and firmware remain intact; no ZIP was generated.

Not run: serial capture, flashing, physical motion/force retention, actual current/voltage/temperature/waveform measurement, hardware stopping or rotating load. Forward interpulse logic is ported; physical equivalence is not established. Final target HOLD remains deliberately absent: first valid reach is bridge OFF, with sampled force decay. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated. **PHYSICAL_STATUS=NOT_RUN.**
