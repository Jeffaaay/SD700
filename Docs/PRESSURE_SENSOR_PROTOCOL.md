# Pressure-sensor protocol

## Transport

The legacy pressure sensor used RS485 port 2: USART6 on PC6 TX, PC7 RX, and PC8 DE/RE. Initialization used 115200 baud, 8 data bits, no parity, one stop bit, no flow control. Phase 1 did not initialize this transport. The current pressure-only bring-up initializes it for nonblocking receive while all motor outputs remain disabled.

## Seven-byte frame

| Byte | Legacy interpretation | Validation |
| ---: | --- | --- |
| 0 | Header `0xFD` | Required |
| 1 | Pressure low byte | Used |
| 2 | Pressure high byte | Used |
| 3 | Unknown field | Included in BCC only |
| 4 | Unknown field | Included in BCC only |
| 5 | XOR/BCC of bytes 0 through 4 | Required |
| 6 | Marker `0xFE` | Required |

Pressure extraction is little-endian:

```text
raw_pressure_counts = frame[1] | (frame[2] << 8)
```

The decoded value is sensor counts, not Newtons. No reliable count-to-Newton conversion, byte-3/byte-4 semantics, byte-7 semantics, sensor sample rate, sequence number, or sensor-side timeout specification was found. A commented legacy display fragment claimed roughly 5 Hz pressure refresh and 1 Hz display refresh, but it was inactive and is not protocol evidence.

## Legacy ISR behavior removed

`Drivers/BSP/RS485/rs485.c` assembled the frame in the USART6 ISR, called blocking `HAL_UART_Receive(..., 300)`, validated byte 6 and the BCC, wrote the global application pressure, and recorded a HAL tick. That implementation mixed transport, protocol, timing, and application state and could block inside the interrupt. It was deleted after extraction.

## New pure decoder

`PressureSensor_DecodeFrame()` has no HAL, FreeRTOS, UI, motor, timestamp, or global-state dependency. It validates arguments, exact length, header, BCC, and marker before assigning the little-endian pressure. The output object is left unchanged on any failure.

Bench capture on 2026-08-23 confirmed the continuous stream repeats the exact seven-byte sequence `FD 02 01 02 01 FD FE` under an indicated load of approximately 259 N. The following `FD` is the next frame header, not an eighth field. Treating it as byte 7 consumed the next header, caused deterministic resynchronization errors, and could publish a misaligned low pressure value.

`PressureSensor_CalculateBcc()` is isolated in its own source file. A null data pointer returns zero; the decoder rejects null pointers before using it.

## Current nonblocking receiver

`Board/RS485/pressure_uart6.c` configures USART6 at 115200, 8-N-1 and holds PC8 DE/RE low for receive. The ISR path uses one-byte `HAL_UART_Receive_IT()` completion callbacks and never calls blocking `HAL_UART_Receive()`.

`Transport/Pressure/pressure_receiver.c` accepts arbitrary byte chunks, searches for the `0xFD` header, collects exactly seven bytes, reuses `PressureSensor_DecodeFrame()`, and resynchronizes after invalid frames. Its read-only snapshot contains raw counts, a local sample sequence, timestamps, and framing counters. It does not convert counts to Newtons and does not apply an overpressure threshold.

## Locked-bench control conversion

The active MCU profile explicitly converts `raw_pressure_counts` to `control_pressure_units` with an identity conversion in `Application/pressure_control.c`. The target uses those same raw-count control units. This is permitted only while `SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED` is exactly 1; a compile-time check rejects the combination otherwise.

```text
TARGET_UNIT=RAW_SENSOR_COUNTS_IN_LOCKED_BENCH_BUILD
NEWTON_CALIBRATION=NOT_IMPLEMENTED
RAW_OVERPRESSURE_PROTECTION=DISABLED_UNQUALIFIED_IN_MCU_BENCH_PROFILE
```
