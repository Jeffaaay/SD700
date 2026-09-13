# CommissioningUnlock1 - actual software results

Baseline: clean main/origin-main `5fb80d7cfc85176d9a82688ab37d22194a2577e4`.
Execution: 2026-09-12 local (2026-09-13 UTC), Windows PowerShell. No intervening
work was rolled back. This is the new supervised commissioning firmware, following
the CaptureFix1 transport/verifier repair and repository-only delivery cleanup.
Delivery is a normal pushed main commit; no ZIP is produced.

**physical test NOT RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD:
NOT_VALIDATED.** No serial port, motor connection, flashing, current measurement,
thermal measurement or physical commissioning was performed by this work.

## Change boundary

The explicit CommissioningUnlock1 compile profile permits the existing arming
gate under the existing RealBench acknowledgement/master guard. Default
ForceServo remains locked. Continuous commands are capped at +/-100 mV, including
executor admission and RAM parameter bounds. This uses the old release default
100 mV, the lower of the original press250/release100 numerical seeds. It is not
a qualified continuous rating or measured coil voltage. Supply current limit
assumptions and PWM mapping are unchanged (100 mV -> compare20 at period4799).

The profile requires contact at START and rejects legacy pulse/run entries so
the old 10000 mV initial approach cannot escape the commissioning ceiling. The
PC script defaults to target40 and admits initial raw20..30 / target<=60. No
field Target250/500/3500 N trial is requested. Firmware target275/raw abort325,
contact20, 30000 ms build deadline and 45000 ms default session remain unchanged.

Existing default timing is retained: control5 ms, sample age20 ms, feedback gap40
ms, lease50 ms and reversal OFF>=2 ms. RAM cannot enlarge the age/gap/lease
budgets or shorten that deadtime. PID and trajectory algorithms, continuous
executor ownership/update/STOP logic, TIM5 implementation, active HOLD,
overpressure and fault/no-restart behavior are not redesigned. New executor
checks only reject out-of-profile requests. Contact loss still faults/OFF.

## Executed checks

All final commands exited0. Exact commands, tool versions, individual host compile
flags, result records, log paths and log SHA256 values are in
[verification.json](verification.json). Logs and synthetic CSV/report/metadata
are retained locally under `output/CommissioningUnlock1`; generated binaries
can be regenerated. Logs are not field evidence.

The local `gcc` entry invokes Clang22.1.8 in GCC-compatible mode; ARM Release
uses Arm GNU Toolchain14.2.1. Python3.12.10 and Windows PowerShell5.1.19041.6456
ran the data/verifier and capture tests. This records the actual host compiler,
not an assumed GNU compiler based on the executable name.

| Check | Actual result |
| --- | --- |
| General existing host suite | 33 cases +22 compile-policy cases PASS |
| Existing ForceServo host profile | O0/O2/Os, 15 groups each PASS |
| Commissioning profile with production arming gate | O0/O2/Os, 21 groups each PASS |
| Compile gate policies | default locked + commissioning armed +5 rejected invalid combinations PASS |
| Production serial framing and complete capture | 28 PowerShell cases PASS |
| Existing ForceServo/pressure/AutoTarget capture self-tests | PASS |
| Python data/schema | 11 synthetic checks PASS |
| Strict current firmware verifier rejection suite | 16 tests PASS |
| Current pure-Python firmware verifier | PASS, including operation with empty PATH |
| Optional ARM objcopy cross-check | PASS, exact byte-format equality |
| Historical AutoTarget verifier | PASS; old images retained unchanged |
| ARM RealBench Release commissioning build | PASS; text29408, data100, bss3808 bytes |

The six explicit commissioning groups run at all three optimization levels:

1. Enable the continuous path through the **production** gate; require nonzero
   correct-direction PWM, active timer and hardware guard. Reject +/-101 mV and
   RAM caps101 / lease51 / gap41 / sample-age21.
2. Update40 ->80 mV in the same direction, increasing compare without another
   HAL PWM start. Existing hardware matching guard remains active.
3. STOP during100 mV active output (compare20), cancel lease, invalidate the
   generation, and reject stale updates. A concurrent STOP wins an update race.
4. TIM5 alone stops output with no main/telemetry service. Missing pressure at
   gap41 ms faults/OFF; a21 ms old sample and lease51 are rejected. The retained
   TIM5 early comparison switches OFF at49 ms for the50 ms lease.
5. After STOP, repeated fresh samples/ticks do not restart. After invalid-feedback
   fault, fresh feedback cannot restart; explicit fault reset reaches IDLE/OFF
   and still requires a new deliberate START.
6. Both reversals require OFF; at1 ms both outputs remain zero, at2 ms the next
   direction can enable. Corrupting the opposing output compare faults the
   existing hardware output guard and turns output OFF.

The retained host groups cover trajectory, final-PWM gains, anti-windup, active
HOLD, contact loss, overpressure, stale/duplicate/out-of-order feedback, timestamp
wrap, deadlines, bounded tracking, atomic parameter readback, frozen diagnostic
snapshots, congestion/STOP priority, runtime delivery and synthetic plants.
The commissioning variant checks precontact START refusal and pulse/run refusal;
the original default profile still exercises the inherited approach regressions.
Host numeric targets/plants, including250 test inputs, are not physical tests.

PowerShell tests retain all17 production framing cases (FC03 whole/fragmented,
timeouts, station/function/CRC/overlong errors,0x83 exception, FC04/05/06 regression).
The original five Observe/locked-flow cases remain ZERO START. Six new current
cases check one START with correct target/config readback; lost START echo;
running fault; and high target/no contact/high initial-pressure refusals. The
last three send ZERO START and no target write. Every reply passes production
`Read-LengthAwareResponse` / `Get-ModbusResponseLength`. Fault/timeout paths send
one final STOP/readback, produce CSV/metadata/report and never retry START.

Firmware rejection tests retain checksum, exact length, addressized load bytes,
ELF type/ABI/bounds, symbol/configuration and true hash/manifest checks. They now
pin schemaF101/build46530102/commissioning1, both100 mV caps, exact defaults and
the commissioning gate's literal-true Thumb implementation. Wrong caps, stale
identity, a locked or otherwise incorrect gate, changed timing/defaults and the
historical locked predecessor are rejected. This intentionally verifies the
new explicitly authorized candidate; it does not skip the gate/identity checks.

## Actual commands and retained failure

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build_gcc.ps1 -ForceServo -CommissioningUnlock1 -MotorMode RealBench -Configuration Release -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION -BuildDir output/CommissioningUnlock1/build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run_host_tests.ps1 -OutputDirectory output/CommissioningUnlock1/general_host
python tools/run_force_servo_tests.py --output output/CommissioningUnlock1/legacy_host
python tools/verify_capturefix1.py --commissioning --objcopy-cross-check --output output/CommissioningUnlock1/verified
```

The initial new timing assertion incorrectly expected output still active at49
ms. That test failed; its log is preserved at
`output/CommissioningUnlock1/commissioning_host.log`. Inspection confirmed the
existing1 ms early TIM5 margin, so the test now asserts active at48 /OFF at49.
The production timer was not modified to obtain PASS. The corrected standalone
run and final integrated run both passed. No hardware result is inferred.

## Firmware and physical limits

Current HEX SHA256:
`F96936FF7962ADC65737C0C6C97F5EBC49F30E8A3B33A20B75C450E3657DD46C`

Current ELF SHA256:
`0925C1CDAEEDB7DCBFCD08EC0BF5E37505ABAA2175B2B11A17112D747CB86D41`

29508 addressed load bytes compare exactly. The old locked ForceServo1 and
ConvergenceMeasure1 pairs match their entry hashes; originals and useful earlier
field/test evidence were not overwritten. The former locked manifest is retained
as `Firmware/ForceServo1_Locked.SHA256SUMS.txt`; the current manifest pins only the
new commissioning pair.

The only next field work is the [same-session A-E procedure](../../README.md):
idle/OFF, low-output operation, actual STOP, actual feedback-loss stop, then one
low-target step only if A-D pass. Hold the supply current limit fixed. Use
independent electrical timing measurement for C/D; CSV polling and host PASS
cannot demonstrate real output shutdown. Real continuous ratings, thermal duty,
stop latency, reversal behavior and pressure performance are still unverified.
