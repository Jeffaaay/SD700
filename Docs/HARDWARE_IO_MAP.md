# Hardware and I/O map

## Evidence policy

This map records the compiled Keil implementation and the retained 2026-03-19 schematic revision. Schematic conclusions are marked `SCHEMATIC_CONFIRMED`, but the physical controller has not been probed, so `ASSEMBLED_BOARD_REVISION_NOT_PROBED` remains a separate qualification.

## MCU and clock evidence

| Evidence | Observed value | Assessment |
| --- | --- | --- |
| `Projects/MDK-ARM/press_control_f411.uvprojx` device | `STM32F411RCTx` | Authoritative active project |
| Keil preprocessor define | `STM32F411xE` | Consistent with active project |
| Keil startup | `startup_stm32f411xe.s` | Consistent with active project family |
| Keil IROM / IRAM | 256 KiB flash / 128 KiB RAM | Consistent with the selected F411 RC target |
| 2026-03-19 schematic | STM32F411RCT6; 8 MHz HSE crystal | `SCHEMATIC_CONFIRMED`; `ASSEMBLED_BOARD_REVISION_NOT_PROBED` |
| Keil CPU descriptor | `CLOCK(12000000)` | Conflicts with the schematic-confirmed 8 MHz HSE and firmware's 96 MHz runtime configuration; stale project metadata |
| Compiled clock call in legacy `User/main.c` | `sys_stm32_clock_init(96, 4, 2, 4)` | Retained unchanged in the safe bootstrap |
| Clock implementation | HSE -> PLL, AHB /1, APB1 /2, APB2 /1, flash latency 3 | With the schematic-confirmed 8 MHz HSE this yields 96 MHz SYSCLK, 48 MHz APB1, and 96 MHz APB1 timer clock |
| README | `STM32F412VGT6`, 96 MHz, project `press_control_f412.uvprojx` | Conflicts with the active Keil project and retained schematic revision |
| Removed EIDE configuration | F412Vx define and F412VX startup, 512 KiB flash / 256 KiB RAM | Conflicts with Keil and the retained schematic revision; not a build authority |
| `Reference/LegacySource/Drivers/BSP/TIM/tim.c` comment | APB1 42 MHz / timer 84 MHz | Conflicts with the compiled 96 MHz clock call; archived evidence only |

## Motor PWM and driver shutdown

| Function | Peripheral | Pin | Compiled configuration | Confidence |
| --- | --- | --- | --- | --- |
| PWM leg 1 | TIM2 channel 3, AF1 | PB10 | PWM1, active high | High |
| PWM leg 2 | TIM3 channel 3, AF2 | PB0 | PWM1, active high | High |
| IR2104 shutdown/enable 1 (`SD1`) | GPIO output | PB1 | `0` disables, `1` releases shutdown | `SCHEMATIC_CONFIRMED`; assembled board not probed |
| IR2104 shutdown/enable 2 (`SD2`) | GPIO output | PB2 | `0` disables, `1` releases shutdown | `SCHEMATIC_CONFIRMED`; assembled board not probed |

Legacy PWM reference: 20 kHz, prescaler 0, ARR 4799, and 24 V motor supply. ARR 4799 is consistent with a 96 MHz timer clock. The timer source comment that says 84 MHz is inconsistent and is retained only as a conflict.

For each accepted PRESS or RELEASE request, the real-output path first force-disables the bridge, commits zero compare to both preloaded channel-3 outputs with an update event, and starts both zero-duty PWM carriers while SD1/SD2 remain low. It verifies both channels and counters running at zero duty, raises both SD pins, and only then writes the requested active compare. PRESS keeps TIM2 CCR3 at zero and writes TIM3 CCR3; RELEASE keeps TIM3 CCR3 at zero and writes TIM2 CCR3. Both carrier channels remain enabled during the request, but at most one CCR3 may be nonzero.

Boot, STOP, normal completion, and every fault retain the complete force-disable state: SD1/SD2 low, both CCR3 values zero, both CC3E bits clear, and both timer counters stopped. The active dual-carrier lifecycle is not a brake mode and is never retained after a request.

This start-sequence correction follows field isolation of differing U6/U7 driver-output behavior in CLEAN(11). It does not establish a root cause or prove motor motion. The next physical step is a motor-disconnected driver-output re-test of SD1/SD2, both PWM inputs, both IR2104 outputs, and M+ minus M-.

Phase 1.1 makes register-level disable the first statement in `main()`, before HAL and clock initialization. It enables the GPIOB clock, writes the PB1/PB2 output latches low through BSRR, selects internal pull-downs, and only then selects output mode. The later HAL initializer also uses `GPIO_PULLDOWN`, never starts PWM, and repeats the zero-CCR/disabled-CC3E state.

### External pull-down limitation

The retained schematic shows direct PB1-to-SD1 and PB2-to-SD2 nets and does not show external pull-down resistors. Reset-default input state therefore does not provide a hardware-defined disabled level during initial power-on, pre-`main` startup, debugger halt before the early call, or failure to execute firmware. The Phase 1.1 register sequence minimizes the software-controlled window but cannot close this hardware gap. `SOFTWARE_ONLY_SAFE_BOOT_IS_COMPLETE_HARDWARE_GUARANTEE=NO`.

NMI, HardFault, MemManage, BusFault, and UsageFault use `MotorHw_ForceDisableImmediate()` before halting. That routine has register writes only: SD1/SD2 low, both CCR3 registers zero, and both CC3E bits clear.

## Verified legacy motor direction

| Legacy command sign | Electrical relationship | Physical legacy meaning |
| --- | --- | --- |
| Positive | TIM2_CH3 = 0; TIM3_CH3 = duty | Press / down |
| Negative | TIM2_CH3 = duty; TIM3_CH3 = 0 | Release / up |

Evidence: `IR2104_Update()` selected the timer relationship, while `PressureControl_ManualControl()` mapped down to positive voltage and up to negative voltage. The mapping is passive evidence in `Board/Motor/motor_hw_map.h`; Phase 1 exposes no direction or motion function.

The legacy brake request enabled both shutdown pins and set both PWM compares to zero. Its dynamic-braking current path, MOSFET heating, duration limit, and INA240 visibility have not been measured. No brake API exists in the safe application, and the fault action remains force-disable rather than indefinite brake.

## RS485 mappings

| Port | UART | DE/RE | TX | RX | Legacy UART format |
| --- | --- | --- | --- | --- | --- |
| RS485 port 1, Modbus/HMI | USART2 | PA1 | PA2 AF7 | PA3 AF7 | 115200, 8 data bits, no parity, 1 stop bit, no flow control |
| RS485 port 2, pressure sensor | USART6 | PC8 | PC6 AF8 | PC7 AF8 | 115200, 8 data bits, no parity, 1 stop bit, no flow control |

Compiled DE/RE behavior was low for receive and high for transmit. The mapping is retained in `Board/RS485/rs485_board_map.h`; neither UART nor either transceiver is initialized by the safe application.

## Current-sense conflict

| Evidence | Mapping |
| --- | --- |
| 2026-03-19 schematic | I_OUT on PA4 / ADC1_CH4 |
| `Reference/LegacySource/Drivers/BSP/CURRENT/current_sense.h` executable channel constant | `ADC_CHANNEL_4` |
| `Reference/LegacySource/Drivers/BSP/ADC/adc.c` executable GPIO initialization | GPIOA pin 4 |
| Comments in both current-sense and ADC code | Claim PB1 and/or ADC1 channel 9 |
| Motor compiled map | PB1 is IR2104 `SD1` |
| README | ADC1_CH4, without a pin |

PA4 / ADC1 channel 4 is `SCHEMATIC_CONFIRMED`; the old PB1/channel-9 comments are incorrect for this schematic revision. The assembled board is not probed, and current-sense scaling/thresholds remain unqualified. Current-sense code is archived as `PRESERVED_UNREVIEWED_BSP` and is not in the active build.

## Unverified assumptions

- Physical upper and lower travel limits are not established.
- Encoder wiring and operational meaning have not been validated.
- E-stop input wiring is not established in the retained firmware.
- Motor coast, stop, and brake electrical behavior are not independently verified.
- The schematic settles the MCU package, crystal frequency, ADC input, and shutdown-net mapping for revision 2026-03-19; the assembled PCB revision and electrical behavior remain unprobed.
