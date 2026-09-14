# BuildToTarget1 ? source and constraint basis

Baseline main9de33246b67603e797ff31cc8472f54b9bd0520d was clean and matched fetched origin/main at task start. The authoritative request is the user-provided `SD700_BuildToTarget1_Codex.md`; it supersedes earlier continuous/ONE Assist field instructions for this candidate.

The named dated old archive was not found. The actual local attachment `C:/Users/jian/Desktop/SD700-Servo-Press-Controller-master.zip` contains the exact requested modified files under its two nested source-directory prefixes. Python zipfile read their bytes without extracting or modifying the archive. All three SHA256 values match the request:

| Actual attachment member | SHA256 |
|---|---|
| Drivers/BSP/PRESSURE_CONTROL/pressure_control.c |920aec9a59ec60a439ae7b5624e75aad17ae76e0372c418f7484b505340cafe2|
| Drivers/BSP/PRESSURE_CONTROL/pressure_control.h |5ae26c0cb9e19804899150afa5a6ed34fb34e48589e9a103e6a31f578d97a70e|
| Drivers/BSP/IR2104/ir2104_driver.c |dcb42302f2fed5babac78a01b85dcf8ef5b3c22ada8ab8d2dc15760614d841e2|

The old checkout HEAD3ec855400a8015f5a7b6d8babd21042aaaff4175 does not identify these modified bytes. A different Downloads archive has a different pressure_control.c hash and was not used. No new ZIP/package was generated.

Actual source reviewed: pressure_control.c17 gives approach5.0;24?26 gives boost step0.3/max4.0/fine1.0;31 minimum2 ms;37 minimumOFF30 ms;75/196 nominal pulse10 ms. Lines413 onward use contact10 and micro/fine regulation once contacted. Lines467?473 scale micro0.8..3.0 against error3..50;492?497 compensate duration by base/applied command, minimum2 ms. Lines549?554 scale fine0.4..1.0. Header20?25 confirms these amplitude ranges; header9 has old default max1500 N. Driver7?14,229?249 confirm24 V command mapping and20 kHz PWM. These are software commands, not measured voltages/current.

Selected BuildToTarget1 differences and reasons:

- Keep approach5000 mapping, but segment it with normal99/hard100 ms;100 ms lies below receive lease130?maximum accepted age20 ms. Each repeat requires30 ms trueOFF and new post-end feedback. Maximum8 s cumulative hard reservations. This bounds free approach without position evidence; no claim of mechanical-stall detection.
- Far-target BUILD base3000 and boost+300 after two valid low responses, clamp4000 additive (total7000). Short compensation hard=floor(30000/command) ms, normal hard?1. At7000:3/4 ms; at3000:9/10 ms. This is a new independently timed execution contract, not repeated ONE Assist and not continuous7000.
- Retain earned far-target boost after credible rise; clear low-response count. This is an explicit strategy choice to avoid repeatedly losing breakaway authority, not proof of the physical root cause. Entering taper clears boost; near-target behavior overrides approach and pending segment authority.
- TAPER uses old800..3000 map below50 N remaining error. Fine uses400..1000 with a shorter5 ms hard duration, no old fine boost. Command?hard-duration decreases at the fine boundary even though command alone can step upward. All commands vanish on reached/negative error.
- Old feedback timeout500 ms and retry-after400 ms are not copied. Preserve modern age20/gap125/receive lease130 ms. Require a higher sequence received after actual OFF+30 ms; no during-pulse sample, duplicate, unmatched request or timeout can authorize repetition.
- Old HOLD preload0.2..0.6 V/BRAKE and reverse release are not copied. At any target this candidate disables the bridge and monitors decay, including3000 N; no automatic output restoration.

The user supplied manufacturer statements: approximately3.6 A rated,7 A stalled with roughly30 s permissible exposure,10% work/rest duty (2 min ON/18 min OFF). These are reported statements, not independently verified motor evidence. **10% work/rest duty is not10% PWM.** This build chooses shorter, conservative experimental reservations:12 s maximum total bridge-enabled hard durations and108 s uninterrupted OFF before resetting the thermal epoch (1:9). Approach is additionally limited to8 s. Count full hard time, not PWM fraction; do not refund early OFF/failures. STOP, FAULT, target changes, pulse OFF and subsequent deliberate START do not reset the ledger. Boot requires full108 s OFF to avoid cooling bypass by power cycling. This is not a validated thermal model or permission to sustain any output rating; PSU0.5 A is unchanged, not measured winding current.

No-force-response uses accumulated active BUILD/TAPER time, including inter-pulse OFF, with5000 ms maximum without fresh post-pulse net new-high progress of2 N. A1 N oscillation cannot accumulate fictitious increments; a short500?600 ms plateau or slow real incremental gains are allowed. START/STOP pause this counter without clearing it; only real net gain or completed full thermal-epoch cooling resets it. Free approach instead uses the8 s ON budget because force need not rise before contact. Detail17 is diagnostic-only in this build; no normal total5 s session/build/capture stop is restored.

All new time/amplitude/response fields are pinned in the immutable26-word-u32 Build configuration, FC03 readback, Python schema, strict ELF verifier and rejection tests. Only Target1..3000 is runtime input. The atomic plan retains its wire layout, but both old Assist/Continuous slots must be zero and continuous/Assist ownership is disabled. This makes the higher fixed segment authority explicit rather than hiding it behind a nominal continuous percent.

User-reported old practical attainment of200/500/1000 N is not independently reproduced here. No claim of old2000/3000 N, new force attainment, stable holding, mechanical stall, measured current or thermal validation. **PHYSICAL_STATUS=NOT_RUN.**
