# BuildToTarget2_Interpulse1 handoff

**PHYSICAL_STATUS=NOT_RUN.** Based on clean main/origin/main5b41ad8dab6af67998ea40c0cb1bf1e141ecf54a. This is the old-source MICRO/FINE interpulse port, not a redesigned controller. GitHub main is the source of truth; normal commit/push and live ls-remote comparison complete delivery. No ZIP, serial connection, flashing or machine motion.

Current identity: F10C /4653010F /profile7; Build config version3 /digest2848875769. Old controller source and line references are pinned in [SOURCE_REFERENCE.json](SOURCE_REFERENCE.json). The user explicitly supplied the Desktop ZIP; it is not silently identified as the absent dated ZIP.

HEX: `output/BuildToTarget2_Interpulse1/firmware/SD700_ForceServo1_BuildToTarget2_Interpulse1_RealBench_Release.hex`

SHA256: `BF145FFC2D3CCB7A3A9454C1F1F0EC23852D7DA6810B62D9F0C05D9F69CD978B`

ELF: `output/BuildToTarget2_Interpulse1/firmware/SD700_ForceServo1_BuildToTarget2_Interpulse1_RealBench_Release.elf`

SHA256: `2A387A04708E543D47C79554E966ACF9E77D3A47712E365AB4FC10D791DC5F53`

Forward MICRO/FINE pulse ends now retain300 command initially, bounded200..600. Accepted post-forward droop strictly>2 N adds100, capped600; MICRO/FINE share the rule. Preload persists across START/cooling like the old static variable. Existing pulse/coarse boost and duration formulas are unchanged. The at-least30 ms cooldown and fresh ordered post-pulse feedback gate remain; no fallback retry on timeout.

All output still passes through the single MotorExecutor. TIM5 retains the original pulse hard deadline until the bounded preload compare is verified. It then continues under the original receive lease and prepaid exposure deadline, preserving its counter and pending faults. A failed transition, STOP, bad/stale feedback, overforce or cutoff disables output and revokes the owner. Reverse/legacy/COARSE completions cannot enter the forward-preload branch; no Build reverse owner was added. Final target reach remains real bridge OFF for every Target1..3000, no final BRAKE/preload/HOLD/repress/automatic RELEASE; sampling continues for force decay.

Exposure remains finite: approach8 s and40M command-ms; pulse hard reservations plus full enabled preload wall time within12 s. Unused prepaid preload credit remains inside the cumulative reserved total, can fund later preload gaps, and is never a refund or new safety budget. Only108 s uninterrupted verified OFF resets the thermal epoch. Cumulative no-response5 s and the accepted-new-START anchor correction remain. These are experimental constraints, not measured heat/current or a continuous rating.

One HEX supports integer Target1..3000. Only Target/START/STOP are field controls; locked Kp/Ki/Kd10/0/0, existing0.5 A PSU setting,20 kHz PWM and all original output/safety constants remain. Preload, current command, timing and budget fields appear in CSV/metadata/report. [README](../../README.md) contains the single current field command and [TEST_RESULTS.md](TEST_RESULTS.md) the actual software evidence. No automatic repeat. Hardware stopping, force attainment/retention, electrical/thermal limits and rotating load remain unvalidated.
