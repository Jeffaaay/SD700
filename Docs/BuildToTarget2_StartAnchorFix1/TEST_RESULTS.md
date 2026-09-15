# BuildToTarget2_StartAnchorFix1 - actual software results

**PHYSICAL_STATUS=NOT_RUN.** Baseline main/origin/main75317233441d596c8a34cb01255994f86563c86b was clean and synchronized. The only control change is the removal of the once-per-epoch guard around progress_anchor initialization in ForceBuildMachine_Begin. It now uses the valid fresh frame admitted by the existing new-START gates. The other production change is build identity4653010D ->4653010E; schemaF10B/profile7/config version2/digest1817994819 and every output/protection parameter remain unchanged.

Before touching production code, the added regression reproduced the reviewed bug with Target500: first10->150 N, STOP, unload0, wait5100 ms, commit/ACK/arm a new plan and START, then second10 N plus1 N every100 ms. The fixture leaves100 ms of genuine no-response charge before the first STOP to verify that START cannot clear it. The unmodified7531723 code faults at59 N with anchor150 and no_response_ms5000, Detail22. This is the reported approximately60 N failure, not a field measurement.

The actual failed command was `python tools/verify_build_to_target.py --output output/BuildToTarget2_StartAnchorFix1/repro-before --only build_to_target_force`; its exit code1 and raw-log SHA256 remain in [regression_before.json](regression_before.json). [regression_before.txt](regression_before.txt) is the output with only line endings/trailing whitespace normalized. The original failed log/binaries remain at the recorded output path. It is recorded as a real pre-fix FAIL, never relabeled PASS.

With the fix, the same second cycle grows continuously10->160 N without a fault, retains the incoming no-response charge until the first qualified>=2 N post-pulse net gain, then stops on a genuinely flat160 N plateau at5000 ms with Detail22. The final run passes all tests below.

| Actual check | Result |
|---|---|
| Existing host / compile-policy regression |33 groups +22 policy cases PASS|
| Original locked ForceServo |15 groups at each O0/O2/Os PASS|
| Existing commissioning ForceServo |42 groups at each O0/O2/Os PASS|
| Existing range/boost/post-assist fixture |55 groups at each O0/O2/Os PASS|
| Historical RuntimeCharacterization2 |15 groups at each O0/O2/Os PASS|
| Current Build machine/executor/TIM5 |19 groups at each O0/O2/Os PASS|
| Production response parser / full Observe |46 cases PASS|
| Historical runtime capture |24 cases +6 rejected inputs PASS|
| Current Build capture |27 cases +6 rejected inputs PASS|
| Force / pressure / AUTO capture self-tests |PASS, no serial I/O|
| Existing schema / target data tests |11 +15 PASS|
| Runtime2 / Build schema-report tests |5 +6 PASS|
| Compile/arming gates |19 cases PASS|
| Strict firmware verifier rejection/equality tests |34 tests PASS|
| Strict current and historical AUTO verifiers |PASS|
| ARM Release and independent rebuilt pair |PASS; exact HEX and ELF equality|
| Developer objcopy cross-check |PASS; field verifier remains Python-only|

All23 components of `python tools/verify_build_to_target.py --output output/BuildToTarget2_StartAnchorFix1/verification-final` passed. [verification.json](verification.json) records every actual command, UTC start, duration, exit code and original raw-log SHA256. [software_test_output.txt](software_test_output.txt) contains the full output, with line endings/trailing whitespace normalized only. Each original log hash was checked when constructing these records. Use a new output directory to reproduce; do not overwrite prior evidence.

The three added Build groups are:

- TestBuildSecondStartProgressAndPlatform: the two-cycle regression and subsequent true plateau shutdown; carried no-response charge is not reset at START.
- TestBuildStartAnchorRequiresFreshFrame: nonzero original approach/time/command-time reservations, new plan, repeated STOP, accepted but pending START, busy duplicate START, fresh pre-START timestamp, duplicate sample, ordered but21-ms-old sample, and the eventual qualified fresh frame. Only that last frame establishes anchor10. All exposure/approach reservations remain; only the new pulse's existing11-ms hard reservation is added. A1 N gain cannot clear the timer; a valid cumulative2 N post-pulse gain can. A repeated START while active cannot lower an established anchor, and terminal STOP readbacks cannot reset it or the timer.
- TestBuildRestartNoiseBadFramesAndStop: repeated explicit sessions/STOP/new plans with10/11 N alternating noise exhaust the same5000-ms response allowance. Pending START plus invalid frame, duplicate-only timeout, stale-only timeout or STOP never establishes a new baseline, refunds budgets or restarts output. Nonzero approach reservations and epoch identity are checked throughout.

The original16 Build groups remain, including target reach true OFF/decay without automatic repress; synthetic250/500/1000/2000/3000 targets; low/already-reached targets; persistent no-response and exhausted-budget rejection; pulse cutoff/STOP races; freshness/request/lease/settle gates; exposure/cooling; and old-source pulse/coarse compensation. No thresholds were loosened for PASS.

The additional verifier test rejects the previous BuildToTarget2 identity while comparing actual old/new ELF bytes for g_force_build_config, default config, operating profile, disabled catalog, characterization/legacy auto config, hardware arming guard and unconditional continuous-owner denial. All are identical. The full identity contract is identical except the build ID. Existing strict ELF/HEX bounds/checksums/addressized load bytes, config/guard assertions, actual hashes, empty-PATH operation and corruption rejection tests remain intact. [unchanged_surfaces.json](unchanged_surfaces.json) also records unchanged source hashes for the output algorithm/config, executor, TIM5 and START/safety architecture, plus the preserved7531723 field pair.

Final ARM size: text47,528, data100, bss5,032 bytes;47,628 addressed load bytes including RAM initializers. HEX SHA256:0FB3F0BE63871F66FADA18F8240AE8B3B46B65BA679F36C08037882B868DD394. ELF SHA256:C8346251004C69386A0C3AAD3BC4F0C29C964D2766B4EBD92BF0C76778717209. Current paths are in [HANDOFF.md](HANDOFF.md) and the current firmware manifest. Historical evidence, firmware and manifests are retained; no new ZIP was generated.

BUILD pulse gaps remain completely bridge OFF. The old0.2-0.6 V interpulse preload is absent; **physical equivalence has not been demonstrated**, and no energized preload was added. This patch is a session-baseline correction, not a controller redesign, output retune or physical force qualification. The existing full108 s OFF epoch reset remains; START/STOP/new plans themselves never erase the cumulative budgets.

Not run: real serial capture, flashing, machine motion, physical stopping/current/temperature/force measurements, HARDWARE_STOP_VALIDATION, STATIC_250 or ROTATING_LOAD. Synthetic PASS is not physical validation. **PHYSICAL_STATUS=NOT_RUN.**
