# Phase 1 cleanup report

## Baseline audit

| Field | Value |
| --- | --- |
| `SOURCE_WORKSPACE` | `C:\Users\jian\Desktop\SD700-Servo-Press-Controller-master\SD700-Servo-Press-Controller-master` |
| `CLEAN_WORKSPACE` | `C:\Users\jian\Desktop\SD700-Servo-Press-Controller-master\SD700-Servo-Press-Controller-CLEAN` |
| `GIT_REPOSITORY_PRESENT` | `NO` |
| `GIT_BRANCH` | `N/A` |
| `GIT_COMMIT` | `N/A` |
| `WORKTREE_STATUS` | `N/A_NOT_A_GIT_REPOSITORY` |
| `PROJECT_FILE` | `Projects/MDK-ARM/press_control_f411.uvprojx` |
| `MCU_TARGET_FROM_PROJECT` | `STM32F411RCTx` |
| `BASELINE_BUILD_RESULT` | `NOT_RUN_TOOLCHAIN_UNAVAILABLE` |
| Source file count / manifest SHA-256 | `945` / `0F7BE32C279E520B56599713548F961B7CB101E5B159335FE4D029B098F2ED6C` |

No Keil UV4, ARMCC, ARMCLANG, GCC, Clang, MSVC, or other C compiler was available. No toolchain was installed. Existing generated output was inventoried but was not treated as a new baseline build.

After all clean-workspace changes, the source was rehashed at 945 files and the same manifest SHA-256 shown above. `SOURCE_LEFT_UNCHANGED=YES`. No `伺服保压6` directory was present in the task workspace, no reference-only project was opened or written, and all writes/deletions were confined to the new clean sibling. `REFERENCE_PROJECT_LEFT_UNCHANGED=YES`.

## Packaging filename normalization addendum

After Phase 1 closed, the retained schematic copy was renamed from its garbled filename to `Reference/Hardware/2026-03-19_SCH_servo_press.pdf` for Windows Explorer, Git, and script compatibility. This was a filename-only packaging change: its SHA-256 remains `5CAB6B544274EFB15C05BAB29AAFF6012CC9F0F484C9B6C6CC2EB614E6B6EDC2`. The Phase 1 source-integrity statement above remains the recorded result at the end of Phase 1; the current package path manifest differs because of this later user-requested rename.

## Phase 1.1 safety cleanup

Phase 1.1 corrected the P0 software safe-boot defect without adding a motor-motion API or state-machine implementation:

- `MotorHw_EarlyForceDisable()` is the first statement in `main()`, before `HAL_Init()` and clock setup. It uses CMSIS register writes only.
- PB1/PB2 output latches are cleared through BSRR before MODER is changed. Both pins use internal pull-downs, not pull-ups.
- `MotorHw_ForceDisableImmediate()` writes SD1/SD2 low, writes TIM2/TIM3 CCR3 to zero, and clears both CC3E bits without HAL, heap, RTOS, mutex, queue, or application-state dependencies.
- NMI, HardFault, MemManage, BusFault, and UsageFault call the immediate disable path before entering a non-returning halt.
- `SafeIdle_Run()` is declared and defined with CMSIS `__NO_RETURN`.
- The 42 remaining legacy BSP files moved byte-for-byte from `Drivers/BSP/` to `Reference/LegacySource/Drivers/BSP/`. Their relative-path/content manifest SHA-256 before and after the move is `46AA422EB3AC6CD52A51B30A821A3240E3D5C092172CC2971F157726D654CAC7`.
- The independently reviewed clean package contained 522 files. Phase 1.1 adds only `motor_hw_early_force_disable.c/.h`, so the current clean tree contains 524 files; relocating the BSP archive did not change its file count.
- The legacy README now begins with an authoritative warning that its architecture description is historical and that archived BSP code is not approved active code.

The retained schematic revision confirms `STM32F411RCT6`, an 8 MHz HSE crystal, PA4/ADC1_CH4 current sense, PB1 as SD1, and PB2 as SD2. These are `SCHEMATIC_CONFIRMED`; the assembled board revision has not been electrically probed: `ASSEMBLED_BOARD_REVISION_NOT_PROBED`.

The schematic shows no external pull-down on SD1 or SD2. Software now minimizes the unsafe interval, but software cannot define those nets during power-on reset, pre-`main` startup, debugger halt before the early call, or a failure that prevents code execution. `SOFTWARE_ONLY_SAFE_BOOT_IS_COMPLETE_HARDWARE_GUARANTEE=NO`. Motor enable remains prohibited until the hardware safety gap and the other motion-safety prerequisites are resolved.

```text
PHASE1_1_STATIC_RESULT=PASS
SAFE_BOOT_SOFTWARE_SEQUENCE=PASS
FULL_FIRMWARE_BUILD=NOT_VERIFIED
READY_TO_DESIGN_STATE_TABLE=YES
READY_TO_ENABLE_MOTOR=NO
```

The active Keil project selects STM32F411RCTx, `STM32F411xE`, F411 startup, 256 KiB IROM, and 128 KiB IRAM. The README and removed EIDE configuration claimed STM32F412 variants and different memory. The conflict is documented rather than resolved. The safe build retains the Keil target, startup, linker ranges, and clock call.

## Before and after

Before, the source root logically contained:

```text
Drivers/                    vendor, system, and coupled BSP/application modules
EIDE/                       conflicting F412 project plus 261-file CMSIS cache
Middlewares/                FreeRTOS, allocator, XMRAM
Output/                     173 generated build artifacts
Projects/MDK-ARM/           F411 Keil project, debug/user cache
User/                       mega-main, ISR file, inactive FreeRTOS demo
README.md and two PDFs
```

After, the clean workspace adds the requested boundaries:

```text
Application/                safe boot and permanent idle only
Board/Motor/                disabled-only hardware code and passive map
Board/RS485/                passive map only
Protocol/PressureSensor/    pure BCC and frame decoder
Protocol/Modbus/            passive constants/map and pure CRC
Config/                     reference-only legacy values
Docs/                       seven Phase 1 evidence/cleanup documents
Reference/                  evidence, data-status note, and isolated legacy source
Tests/Host/                 two pure-code C test programs
Drivers/, Middlewares/      vendor and preserved-unreviewed low-level sources
Projects/MDK-ARM/           authoritative updated F411 Keil project
User/                       13-line main and minimal interrupt handlers
```

`Output/`, all of `EIDE/`, Keil DebugConfig, and the Keil GUI user cache are absent.

## Files changed

Comparison against the source workspace by relative path and SHA-256 produced:

- Files added: 33 after completion of all seven documents. These are the 19 Application/Board/Protocol source/header files, one reference header, two host tests, seven documents, three copied binary evidence files, and the pressure-data status note.
- Files deleted: 456. This comprises the 12 legacy custom source/header files, generated/IDE artifacts, and the original schematic pathname counted as deleted after filename normalization. This corrects the earlier 455 summary and matches the independently reviewed 945-to-522 manifest comparison.
- Files modified: 3: `User/main.c`, `User/stm32f4xx_it.c`, and `Projects/MDK-ARM/press_control_f411.uvprojx`.

The three modified files above describe the Phase 1 reviewed comparison boundary. Phase 1.1 subsequently changed the safety sources/project metadata and documentation listed in its addendum; it did not rewrite firmware control behavior or add motion.

Explicitly deleted custom modules/files:

```text
Drivers/BSP/SHOW/show.c
Drivers/BSP/SHOW/show.h
Drivers/BSP/PID/pid.c
Drivers/BSP/PID/pid.h
Drivers/BSP/PRESSURE_CONTROL/pressure_control.c
Drivers/BSP/PRESSURE_CONTROL/pressure_control.h
Drivers/BSP/RS485/rs485.c
Drivers/BSP/RS485/rs485.h
Drivers/BSP/IR2104/ir2104_driver.c
Drivers/BSP/IR2104/ir2104_driver.h
User/freertos_demo.c
User/freertos_demo.h
```

## Active Keil build graph after cleanup

The Phase 1.1 project has 37 unique, existing file entries: one F411 startup, `main.c`, the minimal exception file, CMSIS system source, `sys.c`, 24 vendor HAL sources retained from the project, two Application sources, three disabled-only Motor sources, and three pure Protocol sources. FreeRTOS, XMRAM, every legacy BSP module, delay, IWDG BSP, UART/RS485, UI, PID, and pressure control are absent from the active build graph.

The target name `FreeRTOS` remains historical project metadata because changing the configured target was prohibited; no FreeRTOS source, include path, task, scheduler call, or port handler remains in the build.

## Build and test results

| Check | Result |
| --- | --- |
| Baseline firmware build | `NOT_RUN_TOOLCHAIN_UNAVAILABLE` |
| Post-clean firmware build | `NOT_RUN_TOOLCHAIN_UNAVAILABLE` |
| Keil XML parse | PASS |
| Project file path consistency | PASS: 37/37 exist, zero duplicates |
| Include path consistency | PASS: all configured include directories exist |
| Unresolved custom quoted headers | PASS: zero |
| Duplicate active custom external symbols | PASS: zero among 28 scanned definitions |
| New custom C size rule | PASS: largest active custom C file has 54 nonblank/noncomment lines |
| Host C tests | PASS: independently compiled and run with GCC during package review |
| Independent test-vector calculation | PASS: CRC `123456789` = `0x4B37`; legacy request CRC = `0x038B` |

The host test sources cover valid/invalid pressure frames, little-endian extraction, unchanged output on failure, null pointers, the standard CRC vector, empty input, a known legacy request, and null CRC input. Independent GCC execution reported both tests PASS. The Phase 1.1 environment still has no GCC, Clang, MSVC, ARMCC, ARMCLANG, or Keil executable, so it did not locally rerun either host tests or the full firmware build. `LOCAL_HOST_TEST_RERUN=NOT_RUN_TOOLCHAIN_UNAVAILABLE`; `FULL_FIRMWARE_BUILD=NOT_VERIFIED`.

`STATIC_PROJECT_CHECK=PASS`

## Required static scan

Scope: active custom C/header code only (User bootstrap/interrupts, Application, Board, Protocol, and retained `sys` clock support). Vendor HAL/CMSIS, documents, tests, and reference-only `Config/legacy_control_reference.h` are excluded where the requirement says active application code.

| Scan | Count |
| --- | ---: |
| Legacy state/control symbols | 0 |
| Fake key assignments | 0 |
| Blocking HAL UART receive/transmit calls | 0 |
| Application watchdog feed sites | 0 |
| Task-based pulse paths | 0 |
| Active PID references | 0 |
| Active fake mechanical-model references | 0 |
| Mutable application `extern` globals | 0 |
| Active motor-driver enable paths | 0 |
| Active nonzero PWM command paths | 0 |
| Active brake command paths | 0 |
| Active automatic-control paths | 0 |
| Protocol -> LCD/UI/Motor dependencies | 0 |
| Motor hardware -> LCD/UI dependencies | 0 |
| Application -> legacy reference header dependencies | 0 |

No USART IRQ exists in the active custom code. Therefore the ISR-specific blocking UART count is also zero.

## Proof of safe boot only

The only call chain from `main` is:

```text
main
  -> MotorHw_EarlyForceDisable
       -> enable GPIOB/TIM2/TIM3 clocks with register writes
       -> latch PB1/PB2 low before selecting output mode
       -> select internal pull-downs
       -> force CCR3=0 and clear both CC3E bits
  -> HAL_Init
  -> existing sys_stm32_clock_init(96,4,2,4)
  -> SafeBoot_Initialize
       -> MotorHw_InitDisabled
            -> force PB1/PB2 low
            -> configure PWM hardware with pulse 0
            -> never start PWM
            -> force CCR3=0 and clear both CC3E bits again
  -> SafeIdle_Run
       -> MotorHw_ForceDisable
            -> MotorHw_ForceDisableImmediate
       -> permanent __WFI loop
```

There is no function that accepts direction, voltage, duty, pulse, brake, or enable requests. The safe application does not initialize UART, RS485, ADC, current sense, keys, LCD, EEPROM, FreeRTOS, automatic control, or IWDG. No source-level motor command path remains in the active build.

## Isolated legacy source

The following BSP families are classified `PRESERVED_UNREVIEWED_BSP` and now exist only under `Reference/LegacySource/Drivers/BSP/`: 24CXX, ADC, BEEP, CAN, CANopen, CURRENT, ENCODER, EXTI, IIC, KEY, LCD, LED, MOTOR, TIM, TIMER, and WDG. No `Drivers/BSP/` directory remains. In particular, the archived TIM compare setters and MOTOR code are not linked into the temporary application and must not be treated as approved Phase 2 APIs. Preserved delay, USART, MALLOC, FreeRTOS, and XMRAM sources remain outside the active Keil graph but were not relocated in this narrowly scoped Phase 1.1 change.

## Evidence files

Three relevant binaries were copied without conversion. Their SHA-256 hashes are in `PHASE1_RETAINED_INVENTORY.md`. No CSV/XLS/XLSX/LOG/JSON/MAT pressure experiment dataset was present.

`PRESSURE_EXPERIMENT_DATA_STATUS=NOT_FOUND`

## Baseline inventory appendix

### All baseline custom C/H files (68)

```text
Drivers/BSP/24CXX/24cxx.c, 24cxx.h
Drivers/BSP/ADC/adc.c, adc.h
Drivers/BSP/BEEP/beep.c, beep.h
Drivers/BSP/CAN/can.c, can.h
Drivers/BSP/CANOPEN/canopen.c, canopen.h, canopen_app.c, canopen_app.h, canopen_conf.h
Drivers/BSP/CURRENT/current_sense.c, current_sense.h
Drivers/BSP/ENCODER/encoder.c, encoder.h
Drivers/BSP/EXTI/exti.c, exti.h
Drivers/BSP/IIC/myiic.c, myiic.h
Drivers/BSP/IR2104/ir2104_driver.c, ir2104_driver.h
Drivers/BSP/KEY/key.c, key.h
Drivers/BSP/LCD/lcd.c, lcd.h, lcd_font.h, lcd_spi.c, lcd_spi.h
Drivers/BSP/LED/led.c, led.h
Drivers/BSP/MOTOR/motor_control.c, motor_control.h
Drivers/BSP/PID/pid.c, pid.h
Drivers/BSP/PRESSURE_CONTROL/pressure_control.c, pressure_control.h
Drivers/BSP/RS485/rs485.c, rs485.h
Drivers/BSP/SHOW/show.c, show.h
Drivers/BSP/TIM/tim.c, tim.h
Drivers/BSP/TIMER/atim.c, atim.h, btim.c, btim.h, gtim.c, gtim.h
Drivers/BSP/WDG/wdg.c, wdg.h
Drivers/SYSTEM/delay/delay.c, delay.h
Drivers/SYSTEM/sys/sys.c, sys.h
Drivers/SYSTEM/usart/usart.c, usart.h
Middlewares/MALLOC/malloc.c, malloc.h
User/freertos_demo.c, freertos_demo.h, FreeRTOSConfig.h, main.c,
User/stm32f4xx_hal_conf.h, stm32f4xx_it.c, stm32f4xx_it.h
```

### Baseline Keil project entries (58)

- Startup: F411XE startup.
- User/system: main, interrupt file, CMSIS system source, delay, sys.
- HAL: base, cortex, GPIO, RCC/RCC_EX, USART, PWR/PWR_EX, UART, DMA/DMA_EX, TIM/TIM_EX, SRAM, LL FSMC, FLASH/FLASH_EX, ADC/ADC_EX, I2C/I2C_EX, SPI, IWDG, CAN.
- BSP: LED, EXTI, IIC, 24CXX, LCD, LCD SPI, WDG, RS485, SHOW, ENCODER, PID, IR2104, TIM, ADC, CURRENT, PRESSURE_CONTROL.
- Middleware: XMRAM header/library; FreeRTOS croutine, event_groups, list, queue, stream_buffer, tasks, timers, heap_4, and RVDS CM4F port.
- A `readme.txt` entry existed but its referenced root file was missing.

### Baseline custom C sources not included in Keil (12)

```text
Drivers/BSP/BEEP/beep.c
Drivers/BSP/CAN/can.c
Drivers/BSP/CANOPEN/canopen.c
Drivers/BSP/CANOPEN/canopen_app.c
Drivers/BSP/KEY/key.c
Drivers/BSP/MOTOR/motor_control.c
Drivers/BSP/TIMER/atim.c
Drivers/BSP/TIMER/btim.c
Drivers/BSP/TIMER/gtim.c
Drivers/SYSTEM/usart/usart.c
Middlewares/MALLOC/malloc.c
User/freertos_demo.c
```

Generated-output inventory: 173 files under `Output/`, including objects, dependency/CRF files, map/list/scatter/link files, AXF, HEX, and HTML build reports. IDE inventory: 261 cached CMSIS files, seven EIDE project/configuration files, one Keil debug configuration, and one Keil GUI user cache. Source evidence inventory: README, two PDFs, and the supplemental workspace DOCX; no pressure curve dataset.

## Unresolved issues

The 21 questions in `OPEN_QUESTIONS_FOR_STATE_DESIGN.md` remain intentionally unresolved after three schematic facts were closed. No final state machine, event model, context, command router, fault manager, DMA transport, pulse timer, control algorithm, or motor-motion API was invented.
