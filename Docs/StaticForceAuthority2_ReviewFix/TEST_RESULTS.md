# StaticForceAuthority2 ReviewFix test results

**PHYSICAL_STATUS=NOT_RUN.** Executed2026-09-14 UTC from clean/fetched baseline
b9df15185526be1ecf096a270c5e4da6b9708e3f. Exact commands, compiler arguments, UTC
start times, exit codes and original raw-log hashes are in
[verification.json](verification.json). [software_test_output.txt](software_test_output.txt)
contains actual output with only newline/trailing-whitespace normalization.
Original independent review reproduction is preserved unchanged.

| Actual execution | Result |
| --- | --- |
| Supplied reproduction, baseline O0/O2/Os |Gap reproduced in all3; normal handoff89/CCR18 then37 at101 ms continued at93/CCR19 with fault0 |
| Correct-stop regression before fix |Expected assertion failure on baseline O0, exit3221226505; preserved |
| Same independent correct-stop regression after fix |O0/O2/Os PASS;89/CCR18 handoff retained; first37 at101 ms now Fault5/Detail19, command0/CCR0 |
| Existing host regressions / compile policy |33 /22 PASS |
| Plain / live / synthetic ForceServo at O0/O2/Os |15 /42 /55 groups each PASS |
| Production arming/override gates |9 PASS |
| Production framing and complete capture flows |46 PASS, including post-assist frozen event and exact README command |
| Force, pressure and AUTO capture SelfTest |PASS |
| Schema/data / report / verifier rejections |11 /15 /25 PASS |
| Strict pure Python firmware / objcopy / historical AUTO |PASS |
| ARM RealBench ForceServo Release |PASS: text41052, data100, bss4456;41152 addressed load bytes |
| Source/history audit |16 protected source files and86 historical files byte-identical |

The recorded full suite was
`python -B tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/StaticForceAuthority2_ReviewFix/final-checks`.
All13 components passed. Legacy/plain tests have separate recorded commands.
Independent repro builds reuse the range-fixture sources/includes/defines and
replace only the top-level test translation unit with the supplied reproduction
or its correct-stop regression. No independent reproduction drove hardware.

Six added production-path groups cover active and first-after excessive rise,
equal and above threshold, below-threshold continuation,101 ms feedback, one-shot
attribution, duplicate/order/age/invalid frames, STOP-before/after response and
prior-fault preservation, raw325, contact loss, missing-frame and lease expiry,
same-frame early handoff, sub-grid after evaluation, pre-end receive timestamps,
and timestamp/sequence wraparound. Existing55-group suite includes prior boost
rise/deadline/handoff, pending timer races, PI/HOLD, no-restart and protection tests.

Abnormal first response is asserted to leave control sequence/receive timestamp
unchanged, never increment the executor request sequence, and never write PWM
above the handoff value at any instrumented DSB. The independent hard deadline
stays admission+12 ms. Normal handoff retains admission receive+130 ms; duplicate
and pre-end frames do not move TIM5 compare. Missing feedback stops on the
original gap/lease. Only a qualifying below-threshold fresh control frame may
renew its own normal receive-anchored lease. After that one evaluation, pressure
rising above the old assist threshold does not cause a stale-event fault.

Wire/report tests distinguish active sampled peak27 from response peak37 and
late observational after data from a completed protection evaluation. Real
length-aware FC03/04/05/06/exception parsing, partial snapshots, STOP priority,
true timeout evidence, disabled catalog and unknown START echo remain exercised.
F107 rejects the old F106 image. All existing exact config/profile/catalog/hash,
ELF bounds/types/addressed bytes, HEX checksum and arming checks remain; no
assertion was removed or weakened to get PASS. Offline verification passes with
empty PATH; objcopy is a development cross-check only.

No source changes to PID, trajectory, executor, TIM5, PWM, physical output gate,
profile limits or pressure/STOP architecture. The machine separates normal peak
completion from a single pending response check; firmware/schema identity and
reporting were updated. The default locked build and actual enabled live
profile4=720/100/no assist/5 s were preserved. All new20/30/40% assists and normal
2400 remain disabled. No new current, duration, cooling or N rating was inferred.

HEX SHA256: `7346FA1A6B4D6DE045DAC1D03A13B46BC15F0F41053F1C7B146143E3FAFEF2D3`

ELF SHA256: `D0B1961A06EC5263F0C222151C3410CAFED7B69D84B5A5D6E5FD2DD34D2641A7`

The synthetic3/9/12 ms, total12 ms, OFF1000 ms and response/excessive thresholds
2/10 are regression inputs, not physical commissioning ratings. Missing reviewed
experimental profile values/sources are listed separately in HANDOFF.md. Missing
old firmware identity does not block this completed software correction. No
repeat3% performance test is requested;0.5 A unchanged. No serial connection,
flash, real START or physical test was run. HARDWARE_STOP_VALIDATION / STATIC_250 /
ROTATING_LOAD remain unvalidated. Git main is the delivery; no ZIP.
