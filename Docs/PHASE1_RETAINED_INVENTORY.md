# Phase 1 retained inventory

`active` below means compiled into the temporary safe Keil target. `reference-only` means it cannot command hardware and is not an application dependency.

| Item | Original source file | Original symbol or location | New file | Active or reference-only | Confidence | Open question |
| --- | --- | --- | --- | --- | --- | --- |
| Safe bootstrap boundary | `User/main.c` | `main`, `systemInit` | `User/main.c`, `Board/Motor/motor_hw_early_force_disable.c`, `Application/safe_boot.c` | Active | High for software sequence | External SD pull-downs absent |
| Permanent safe idle | none | new Phase 1 safety skeleton | `Application/safe_idle.c` | Active | High | Watchdog ownership deferred |
| Forced-disabled motor initialization | archived IR2104/TIM legacy sources | `IR2104_Init`, `motor_init` | `Board/Motor/motor_hw_init_disabled.c` | Active | High for software; schematic-confirmed map | Assembled board not probed |
| Immediate motor disable | IR2104 emergency/stop paths | `SD1(0)`, `SD2(0)`, both CCR3 = 0 | `Board/Motor/motor_hw_force_disable.c` | Active, register-only | High for register actions | Hardware power-on gap remains |
| Critical exception disable | none in Phase 1 | NMI and Cortex-M fault handlers | `User/stm32f4xx_it.c` | Active | High for static call path | Hardware response not bench-tested |
| Motor PWM map | `Drivers/BSP/TIM/tim.h` | TIM2_CH3 PB10; TIM3_CH3 PB0 | `Board/Motor/motor_hw_map.h` | Reference-only constants used by safe init | High | Timer-clock comment conflict |
| Driver shutdown map/polarity | archived TIM source; IR2104 mode evidence; schematic | SD1 PB1, SD2 PB2; zero disabled | `Board/Motor/motor_hw_map.h` | Constants used by safe init | `SCHEMATIC_CONFIRMED` | `ASSEMBLED_BOARD_REVISION_NOT_PROBED` |
| Motor direction relationship | `IR2104_Update`; manual/one-touch control | positive TIM3/down, negative TIM2/up | `Board/Motor/motor_hw_map.h`; `HARDWARE_IO_MAP.md` | Reference-only | High | Do not reinterpret before hardware review |
| PWM frequency/supply references | IR2104 static initializer | 20 kHz, ARR 4799, 24 V | `motor_hw_map.h`; `HARDWARE_IO_MAP.md` | Reference-only; ARR used only for disabled timer configuration | Medium-high | Verify HSE/timer clock and supply |
| RS485 port 1 map | `Drivers/BSP/RS485/rs485.h/.c` | USART2, PA1/PA2/PA3, AF7 | `Board/RS485/rs485_board_map.h` | Reference-only | High | None for pin extraction |
| RS485 port 2 map | same | USART6, PC8/PC6/PC7, AF8 | `Board/RS485/rs485_board_map.h` | Reference-only | High | None for pin extraction |
| Legacy UART format | both `rs485_*_init` functions | 115200, 8N1, no flow control | `rs485_board_map.h`; protocol doc | Reference-only | High | Sensor timing/sample rate unknown |
| Pressure frame format | `RS485_2_UX_IRQHandler` | 7-byte `0xFD`/BCC/`0xFE` frame | `pressure_sensor_protocol.h`; protocol doc | Active | Bench-confirmed continuous stream | Bytes 3 and 4 unknown |
| Pressure XOR/BCC | `Check_BCC` | XOR across requested bytes | `pressure_sensor_bcc.c` | Active pure function | High | None |
| Pressure decoder | USART6 ISR extraction | validation and little-endian pressure | `pressure_sensor_decode_frame.c` | Active pure function | Bench-confirmed frame boundary | Bytes 3 and 4 remain uninterpreted |
| Modbus function codes | USART2 ISR switch | 0x01, 0x03, 0x04, 0x05, 0x06 | `modbus_protocol_constants.h` | Reference-only constants | High | Final parser deferred |
| Modbus coils/registers | USART2 parser and `RS485_RxCallback` | addresses 0x0000..0x0005 and holding 0x0000..0x0003 | `modbus_register_map.h`; map doc | Reference-only | High for recognition | Several semantics are buggy/unfinished |
| CRC-16/MODBUS | `crc16_modbus` | init FFFF, polynomial A001, empty -> 0 | `modbus_crc16.c` | Active pure function, unused by safe application | High | Empty/null result deliberately documented |
| Station-ID behavior | `show.c` and USART2 ISR | default 1, mutable EEPROM-loaded byte | Modbus doc/constants | Reference-only | High | Legal range and address-zero policy unknown |
| Pressure thresholds/limits | `pressure_control.h/.c`, `show.h`, main dispatcher | 1500, 50, 10, 1, ~2 N | `legacy_control_reference.h`; legacy evidence doc | Reference-only | High | Safety approval/tuning deferred |
| Approach and pulse values | pressure-control constants/formulas | 5/2 V approaches, micro/fine pulse ranges | reference header/evidence doc | Reference-only | High | Hardware validation required |
| Pulse/feedback timing | pressure-control constants and task schedule | 2/10/30/150/400/500 ms conflicts | reference header/evidence doc | Reference-only | High | Timing architecture deferred |
| Adaptive boost behavior | pressure-control static helpers | 0.3/2.0/1.0/0.5/3.5 V and reverse x0.5 | reference header/evidence doc | Reference-only | High | Qualification and bounds deferred |
| Legacy field experience | pressure-control mode functions | approach, settle, fresh-frame, brake, hold behavior | `LEGACY_CONTROL_EVIDENCE.md` | Reference-only | Medium-high | Each behavior has explicit Phase 2 decision |
| MCU/clock conflicts | Keil, README, removed EIDE, schematic | F411/F412 and 8/12 MHz discrepancies | `HARDWARE_IO_MAP.md` | Reference-only | F411RCT6 and 8 MHz `SCHEMATIC_CONFIRMED` | Assembled board revision not probed |
| Current-sense conflict | archived current/ADC sources and schematic | executable PA4/CH4 vs wrong PB1/CH9 comments | `HARDWARE_IO_MAP.md` | Reference-only | PA4/ADC1_CH4 `SCHEMATIC_CONFIRMED` | Scaling and physical board not probed |
| Hardware schematic | source root PDF | original binary | `Reference/Hardware/` | Reference-only | Byte-identical copy; reviewed | Assembled board revision not probed |
| Legacy BSP archive | remaining `Drivers/BSP/` files | 42 files / 391382 bytes | `Reference/LegacySource/Drivers/BSP/` | Reference-only | Byte-identical tree manifest | Must never be treated as an approved API |
| INA240 data sheet | source root `ina240.pdf` | original binary | `Reference/Hardware/ina240.pdf` | Reference-only | Byte-identical copy | Fitted gain/shunt still unknown |
| SD700 rollback analysis report | workspace sibling DOCX | original binary | `Reference/Reports/` | Reference-only | Byte-identical copy | Human report review pending |
| Pressure experiment datasets | package-wide extension search | none found | `Reference/PressureData/README.md` | Reference-only | High for searched package | Obtain actual curves if they exist elsewhere |

## Reference-file SHA-256

| Retained file | Bytes | SHA-256 |
| --- | ---: | --- |
| `Reference/Hardware/2026-03-19_SCH_servo_press.pdf` (filename normalized after Phase 1) | 524200 | `5CAB6B544274EFB15C05BAB29AAFF6012CC9F0F484C9B6C6CC2EB614E6B6EDC2` |
| [Reference/Hardware/ina240.pdf](../Reference/Hardware/ina240.pdf) | 2247687 | `3ACA6049D67D03B39A2D7583157737D5EC0E67884C2D16A8E23A4FE6B815E6B2` |
| `Reference/Reports/SD700_press_rollback_analysis_report.docx` | 5759 | `024480286CE8C9DD47D58CA700EE199559503B392032B56941E75D122AE97254` |

The retained binary bytes were preserved; filenames were normalized separately. The archived legacy BSP relative-path/content manifest SHA-256 is `46AA422EB3AC6CD52A51B30A821A3240E3D5C092172CC2971F157726D654CAC7` before and after relocation.

`PRESSURE_EXPERIMENT_DATA_STATUS=NOT_FOUND`

RepositoryCleanup1 note: the original-source column above preserves Phase1 provenance. The identical root `ina240.pdf` duplicate was removed; use the [retained Reference copy](../Reference/Hardware/ina240.pdf). Its original bytes and SHA-256 above are unchanged. Phase1 `active` describes the historical target, not the current field candidate.
