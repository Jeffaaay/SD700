# BuildToTarget2_CoolingAnchorFix1 - commit A software evidence

PHYSICAL_STATUS=NOT_RUN. Baseline main/origin/main8e2973a5a6b9869d14cfc63569989dc8cf1d9ce8 was clean and synchronized. No intervening work was overwritten. The only controller-side changes are cooling OFF verification/accounting in MotorExecutor and the new build identity46530110; the BUILD algorithm, output parameters and protections remain unchanged.

The new regression was run before the production fix:3000 command, normal10/hard11 ms,300 preload,30-ms cooldown and accepted fresh post feedback; repeated until reserved12000. Budget rejection returned rest_remaining107970 instead of108000. This actual FAIL is retained in [regression_before.json](regression_before.json) and [regression_before.txt](regression_before.txt), with the original raw log and its hash. A second fault-injection regression found that an initial attempted fix started cooling even when the hardware OFF readback failed; after a simulated1000 ms still not OFF it reported107000. That failed intermediate test is separately labeled and retained, not represented as a baseline result or physical observation.

The final fix tracks confirmed OFF independently of segment/preload accounting flags. Charging a preload or ending a pulse does not establish the cooling baseline. A real OFF register readback establishes it once, using a refreshed clock after failure shutdown. A failed OFF readback cannot earn cooling time; repeated STOP while already OFF cannot shift the anchor or clear reservations. No new output path, runtime parameter or timer threshold is introduced.

The added tests verify:

- Exact12000-ms budget rejection begins at108000-ms rest after the final preload is disabled.
- Timer failure before segment_active, hardware update/readback failure, and STOP during preload-to-pulse handoff all use the actual shutdown time, including an injected3-ms delay.
- Failed hardware-disable readback does not count as cooling. A later confirmed OFF starts the full interval.
- Repeated STOP preserves reserved exposure, approach charge, preload credit and epoch. At108000 minus1 ms those budgets remain; only the completed uninterrupted OFF interval resets them. Cooling never starts output.

All prior BUILD tests remain: target reach true OFF without automatic repress, fresh post-pulse/cooldown and receive lease, cutoff/STOP races, MICRO/FINE preload adaptation, cumulative no-response, cross-START anchor/budget behavior and synthetic target/decay tests. All38 immutable configuration words, existing output guard and denied continuous-owner function are compared byte-for-byte against the preceding Interpulse1 ELF. [Unchanged sources/configuration](unchanged_surfaces.json), [original32 firmware paths/hashes](preserved_baseline_firmware.json). No assertions were removed or loosened.

| Final actual check | Result |
|---|---|
| Existing host / compile-policy regression |33 groups +22 policy cases PASS|
| Original locked ForceServo |15 groups at each O0/O2/Os PASS|
| Commissioning ForceServo |42 groups at each O0/O2/Os PASS|
| Range/boost/post-assist fixture |55 groups at each O0/O2/Os PASS|
| Historical RuntimeCharacterization2 |15 groups at each O0/O2/Os PASS|
| Current Build machine/executor/TIM5 |27 groups at each O0/O2/Os PASS|
| Production response parser/full Observe |46 cases PASS|
| Historical runtime capture |24 cases +6 rejected inputs PASS|
| Current Build capture |30 cases +6 rejected inputs PASS|
| Force/pressure/AUTO capture self-tests |PASS, no serial I/O|
| Schema / target data / Runtime2 / Build data |11 /15 /5 /7 PASS|
| Compile/arming gates |19 cases PASS|
| Strict verifier rejection/equality tests |36 tests PASS|
| Strict current and historical AUTO verifiers |PASS|
| ARM Release / exact independently rebuilt HEX and ELF |PASS|
| Developer objcopy cross-check |PASS; field verifier remains pure Python|

Final command: `python tools/verify_build_to_target.py --output output/BuildToTarget2_CoolingAnchorFix1/verification-release`. All23 checks passed. [verification.json](verification.json) contains actual commands, timestamps, exit codes and raw-log hashes; [software_test_output.txt](software_test_output.txt) preserves their output with only line endings/trailing whitespace normalized. Earlier focused/initial full runs remain local and are not substituted for the final run. Use a fresh output directory to rerun.

Final ARM size: text49696, data100, bss5104 bytes;49796 addressed load bytes including RAM initializers. Current paths and exact HEX/ELF hashes are in [HANDOFF.md](HANDOFF.md) and [the current firmware manifest](../../Firmware/ForceServo1.SHA256SUMS.txt). Identity F10C/46530110/profile7; Build config version3/digest2848875769. The8 s approach,12 s reservation,108 s cooling and5 s genuine no-response thresholds are unchanged.

The output preload/controller inherited from Interpulse1 is not physically qualified by these tests. No serial connection, flashing, hardware movement, real pressure/current/temperature measurements or ZIP creation occurred. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated. PHYSICAL_STATUS=NOT_RUN.
