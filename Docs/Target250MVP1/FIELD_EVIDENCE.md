# CommissioningUnlock1 field report supplied by the user

This records the user's field evidence, not measurements performed by this
implementation session. The raw CommissioningUnlock1 CSV/report files were not
attached to these requests and were not located in the workspace inventory.
No replacement raw files, scope traces, RAM readings or current measurements
have been fabricated. Synthetic host/capture files remain explicitly synthetic.

Reported firmware: commit `3e4208df808fa7c78731ed589e0cf13a63da81a9`, HEX
`F96936FF7962ADC65737C0C6C97F5EBC49F30E8A3B33A20B75C450E3657DD46C`.

Stage A passed: IDLE, fault0, output_off1, M+=0 V and M-=0 V, no spontaneous motion.
Stage B/C failed before motion: Target40, initial light contact raw22..23,24 V
connected,0.5 A current limit, one START attempt. The response was function0x85,
exception0x04. No measurable output/motion, no RUN rows; only PREFLIGHT and
STOP_READBACK. D/E were not executed. This does not qualify an active hardware STOP.

The earlier supplied timing detail was:

- BC PREFLIGHT now214345, latest_received214321, latest_raw23: age24 ms.
- A-retry latest-sample ages about31,50,44,49,50,56,74 ms.
- Receiver/delivery interval extrema about101 ms, with advancing sequences.
- Old age/gap/lease settings20/40/50 ms.

The old source checks `Machine_IsPressureFresh` during START and returns
COMMAND_NOT_READY if the sample is older than20 ms. The existing Modbus mapping
turns NOT_READY and several other outcomes into0x04. A stale START is the leading
explanation supported by source and the24 ms snapshot. The snapshot is not the
exact START-time state, so it cannot prove the sole physical/software cause.
The new host test reproduces these timestamps and pressure23 through the actual
Modbus server and machine, requiring an8-byte FC05 success echo, OFF pending,
then exactly one continuous session on the next valid frame.

The UART ISR timestamps complete accepted pressure frames with HAL_GetTick.
`PressureReceiver_PublishValidFrame` stores cumulative RX interval extrema;
`ApplicationRuntime_ServicePressure` delivers each new snapshot once, and the
machine records delivered RX timestamp intervals. Main has a boot-only1000 ms
delay before reinitializing the receiver, not a100 ms steady polling delay.
Thus the extrema alone do not establish the normal steady cadence. Repeated
ages above40/50 ms at later observations nevertheless cannot be dismissed as
boot-only evidence. Those observations motivate a bounded125 ms active gap and
130 ms independent lease; a newly delivered sample must still be at most20 ms old.

Target250MVP1 retains the100 mV numerical cap and the user's0.5 A field limit.
Earlier5000 mV/10 ms PRESS and10000 mV approach pulse records do not establish a
higher safe continuous rating. Command mapping, supply limit and historical
pulses are not a continuous thermal qualification. New saturation telemetry
must show whether this retained ceiling binds the Target250 attempt. It cannot
by itself establish a physical force ceiling.

The latest user instruction supersedes the proposed CommissioningStartFix1
diagnostic milestone: implement Target250 with one pending flag, existing
controller and one useful supervised field run. No new pending-state framework
or detailed rejection taxonomy was added. **Target250MVP1 FIELD_STATUS=NOT_RUN.**
