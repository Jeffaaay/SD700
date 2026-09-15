# BuildToTarget2_RiseDiagnostic1

Baseline: clean main/origin/main `8514376320fab15221c6808edd96d3a072505eee`. Identity F10C /46530112 /profile7. Build configuration remains version4, digest2927201258; all38 words and the telemetry layout are unchanged.

The only control change removes the independent BUILD fault for a qualified post-pulse rise>=25 N. Validated before/after force, request and sample identity remain diagnostic evidence. Target-OFF and absolute overforce are still processed first. Existing post-pulse request/freshness/cooldown acceptance and boost/progress accounting are unchanged. The retained `excessive_rise_N=25` config word is historical metadata, not an active BUILD trip.

No changes to5000 MICRO cap,10 ms normal/11 ms hard timing, FINE amplitude, preload300/drop strictly>2 adds100/cap600, APPROACH, PID, PSU limit, STOP, sensor validity/freshness, receive lease, hard cutoff, exposure/no-response budgets or108 s verified-OFF cooling. ELF comparison confirms byte-identical Build/default/profile/catalog/characterization/legacy-machine configuration and output guard/continuous-owner denial.

Actual validation: one O2 affected-test invocation PASS, one ARM Release PASS, one official current strict verifier PASS (49724 addressed load bytes, exact identity/config/guard/hashes). [Actual O2 output](O2_RESULTS.txt).

```powershell
python tools/run_force_servo_tests.py --commissioning --characterization --build-to-target --optimization O2 --test-argument=--rise-diagnostic --output output/BuildToTarget2_RiseDiagnostic1/host
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -BuildToTarget2 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/BuildToTarget2_RiseDiagnostic1/arm
python tools/verify_force_servo_firmware.py
```

All three exit codes0. Target250: post-pulse10->35,10->36,10->249 retain valid diagnostics and admit the next bounded pulse without Fault. At250: true OFF and decay monitoring. At3000/3001 with Target250: overpressure Fault and OFF. The seven affected groups also cover STOP races, bad/duplicate/stale feedback, lease and independent cutoff, no automatic restart, no-response/noise persistence, exposure and exact108000 ms cooling after budget rejection.

NOT_RUN: O0/Os matrix, historical full regression, unrelated capture/schema/report tests, verifier rejection-test suite and optional objcopy cross-check. Necessary identity/load-size expectations in retained tests were updated, not claimed executed. No cleanup, ZIP, serial connection, flashing or hardware action. All36 previous tracked HEX/ELF paths and hashes remain intact.

[HEX](../../output/BuildToTarget2_RiseDiagnostic1/firmware/SD700_ForceServo1_BuildToTarget2_RiseDiagnostic1_RealBench_Release.hex) SHA256 `CB919CB13E8AAB010FAC433EE9DE85C93AE3635295A6F9B77B228AB9CF8B5142`

[ELF](../../output/BuildToTarget2_RiseDiagnostic1/firmware/SD700_ForceServo1_BuildToTarget2_RiseDiagnostic1_RealBench_Release.elf) SHA256 `247393E14446BD0C772FD58F482FCF652542DFBF0662C9AB8B6F89EFDFF6D7DE`

[Current manifest](../../Firmware/ForceServo1.SHA256SUMS.txt). Prior manifest preserved as Firmware/BuildToTarget2_Pulse10msFix1.SHA256SUMS.txt.

**PHYSICAL_STATUS=NOT_RUN.** Synthetic PASS does not prove physical250 N success. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated.
