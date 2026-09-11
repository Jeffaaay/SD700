# PressBoost1 / FieldReady1 - ONE controlled static trial

Candidate: `output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.hex`.
Matching ELF is in the same directory. This is the unchanged PressBoost1
physical-test candidate. FieldReady1 required NO MCU source change or rebuild.

```text
HEX_SHA256=26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2
ELF_SHA256=DAF479B97E4BD0E286962EBF16D21FFA235C3450D4E46CA5B6CF6D3DB9725256
```

1. Operator verifies the permitted mechanical load/travel, fixture, existing
   emergency stop, supply current limit and experiment raw abort325 before
   powered use. PressBoost1 PRESS candidates reach5000 mV/10 ms; their mechanical
   safety/effectiveness are NOT validated. Verify the loaded firmware against
   the exact unchanged PressBoost1 HEX; no new firmware flash is required by this host update. No device was connected, flashed or started by the agent.
2. Close other serial clients. From extracted SD700 root, replace COM6 with the
   actual port and execute ONCE, using full static mode (no ApproachOnly):

```powershell
.\tools\capture_auto_target_static.ps1 -Port COM6 -Target 250 -MaximumSeconds 15 -ConfirmStaticTest -ConfirmMechanicalLimitChecked -ConfirmedFirmwareSha256 26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2 -OutputCsv .\output\AutoTarget\field\press-boost1-field-ready1-250.csv
```

3. Require real evidence of contact -> pressure clearly above the previous
   approximately30 plateau -> approach250 -> enter245..255 -> AUTO_HOLD.
   Observe a STATIC FIXED workpiece. Full mode sends at most one START and
   observes up to15 s, then STOP/readback; fault/communication failure or the
   existing qualified stability criterion can finish earlier. Preserve the
   existing emergency stop, current limit and raw-overpressure stop conditions.
   HOLD alone does not prove stable pressure. No success is claimed from tests.
4. Retain CSV, report and visual observation. If no pressure progress or STOP
   cannot be verified, follow the existing stop procedure; do not issue another
   automatic START or raise limits. After verified STOP, inspect the existing
   RAM diagnostic symbol with the matching ELF, without reset/download:

```gdb
p g_sd700_approach_diagnostics.press
```

Use an attach-without-reset/download debugger path, not the ordinary flash/reset
launch. Do not halt a moving mechanism for logging. The full record is176 bytes
in this ELF (old approach/deadline fields preserved); use symbol names.
`request_count/request_sequence/requested_mv/base_mv/boost_mv/duration_ms` identify
the latest accepted PRESS. `completed_request_sequence/completed_mv/end_reason`
identify the last completed PRESS, which may precede a currently active request.
Completion code2=NORMAL,4=STOP,5=BACKSTOP,6=executor error,7=safety fault,0=none.
`completed_at_ms` is Machine service time, not the exact TIM5 ISR stop tick.

`before_units/before_sequence`, `observed_peak_units`, and
`after_units/after_sequence/after_received_at_ms` refer to that completed PRESS.
Use after values ONLY if feedback_valid is true. `observed_off_fall` requires
sampled rise>=2 above before and settled drop>=2 below that sampled peak. This
can distinguish retained gain from sampled ON-rise/OFF-fall; it cannot recover
missed fast transients. The record survives STOP/fault and resets at a new START.
Only a small last-request/completion record is retained, not a pulse history.

The user-reported peak31/final30/Fault5-Detail8 alone does not prove OFF fall or
physical request execution. New diagnostics and actual traces are required;
no powered evidence was collected here and no sustained preload was restored.

## Read-only preflight and generated report

BASELINE and TARGET_READBACK each allow10 complete fenced snapshots,20 ms apart
only on incoherence (at most9 waits per phase). Every attempt repeats all four
existing reads: diagnostic fence, base status, tail, diagnostic fence. Safety,
capability and transport errors abort immediately on any attempt; ten incoherent
snapshots or an invalid target readback prevents START. No field mixing or target
rewrite. FC06 framing/CRC/function/echo checks remain. START is NEVER retried;
lost START echo still follows the existing STOP behavior. AUTO has no new retry
loop or changed sampling/exit logic. ApproachOnly remains contact-stop mode and
must NOT be used for tonight's full target trial.

The CSV/report retain PC time and device sequence/receive time. The report names:

| Report field | Meaning / limitation |
| --- | --- |
| ObservedMaximumPressure | Maximum qualified new/coherent/fresh AUTO pressure; not true instantaneous peak; excludes later STOP_READBACK |
| FirstObservedWithinTargetPlusMinus5Ms | PC observation elapsed from the single START attempt, not exact MCU entry tick; NOT_OBSERVED if absent |
| TotalObservedAutoHoldMs / LongestObservedAutoHoldMs | Sum / longest run of adjacent qualified output-OFF state7 observations; device receive gaps must be >0 and <=200 ms, sequence increasing, request unchanged |
| FinalStopReadbackState / Fault / FaultDetail / Coherent | Last STOP snapshot; state/fault/detail are UNKNOWN if absent or incoherent |
| LastCoherentObservedState / Fault / FaultDetail | Last coherent observed status; separate from a missing final STOP result |
| StopVerified | Existing STOP/output-off verification result, unchanged |
| CaptureError | Host capture failure text, distinct from numeric MCU FaultDetail |

For HOLD durations, incoherent/stale/invalid/faulted observations break spans,
duplicate polls add no time, new requests and long/backward receive gaps break
spans. Durations are observed intervals, not exact total MCU dwell time; missed
transitions remain unobserved. Legacy LongestObservedContinuousHoldMs also
requires the +/-10 pressure band and remains unchanged. Stability is still
3 s of qualified fresh HOLD/output-OFF +/-5 feedback, no new request and gaps
<=200 ms; otherwise NOT_STABLE. A short visit to HOLD is not a stable result.

The report also lists EVERY required PRESS diagnostic as RAM_NOT_READ and gives
its field under g_sd700_approach_diagnostics.press. No Modbus register provides
these RAM values; the capture script does not read/debug the device. After
verified STOP, retain the actual debugger dump beside the CSV/report; do not
replace NOT_READ with an inferred count or zero. Read the dump before reset or
another START. The fields are:

| Required evidence | Existing RAM member |
| --- | --- |
| PRESS count / latest requested mV | request_count / requested_mv |
| Base / boost used by latest accepted PRESS | base_mv / boost_mv |
| Pulse duration | duration_ms |
| Completion reason and matching request | end_reason / completed_request_sequence / completed_mv |
| Before / observed sampled peak / settled after | before_units / observed_peak_units / after_units |
| Feedback valid / sampled OFF fall | feedback_valid / observed_off_fall |
| Before/after sample IDs and receive tick | before_sequence / after_sequence / after_received_at_ms |

Boost is retained request metadata; live controller boost clears on STOP/fault.
Latest request and last completed request can differ: match the request IDs.
Use settled-after and OFF-fall only when feedback_valid=true; invalid zero-filled
fields are not pressure measurements. Do not reset/download or halt a moving
mechanism just to obtain a dump. No large logger or protocol change was added.

APPROACH remains10000/20 ms (first),10000/10 ms (recontact), contact20 and total
budget3000 ms. PRESS bands/boost remain unchanged, max5000 mV and10 ms/backstop40;
settle50, feedback timeout250, freshness200, convergence8000. RELEASE Kp8/cap800,
10 ms/no boost; Ki0, no D. Target250 remains SENSOR CONTROL UNITS, not certified
250 N. Mechanical safe limit/calibration are unconfirmed; abort325 is not a
proven mechanical ceiling. No rotating/speed modes, gain scheduling or preload.
