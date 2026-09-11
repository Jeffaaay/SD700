# SD700 Servo Pressure Holding Controller

> **Phase 1.1 status:** The material below describes the legacy baseline and is not the active firmware architecture. The active Keil target is an STM32F411RCT6 safe-boot/idle skeleton with no motor-motion API, FreeRTOS task, PID, UI, or active RS485 transport. Legacy BSP files are isolated under `Reference/LegacySource/Drivers/BSP/` and must not be copied into active code. See `Docs/PHASE1_CLEANUP_REPORT.md`, `Docs/HARDWARE_IO_MAP.md`, and `Docs/OPEN_QUESTIONS_FOR_STATE_DESIGN.md` for the authoritative current status.

Firmware for the **SD700 industrial servo pressure holding controller**, built on STM32F412 with FreeRTOS. The system provides precise pressure control through a multi-stage PID algorithm driving a dual H-bridge motor driver, with RS485/Modbus RTU remote control capability.

## Features

- **Multi-stage pressure control**: 4-stage progressive algorithm (Coarse → Micro → Fine → Hold) for fast convergence with ±5N steady-state accuracy
- **Dual RS485 Modbus RTU**: One channel for HMI/PLC command interface, one for pressure sensor data
- **Motor current sensing**: Real-time overcurrent protection via INA240 current sense amplifier
- **3 operation modes**:
  - Pressure Control (auto PID, jog up/down, fast up/down)
  - One-Touch Up/Down (simple raise/lower)
  - System Settings (station ID, stored in EEPROM)
- **SPI LCD display**: Real-time status display with Chinese UI
- **Independent watchdog**: Hardware-level system recovery
- **FreeRTOS real-time OS**: Deterministic 10ms control loop

## Hardware

| Component | Specification |
|-----------|--------------|
| MCU | STM32F412VGT6 (ARM Cortex-M4, 96MHz) |
| Motor Driver | Dual IR2104 half-bridge gate driver |
| Current Sensor | INA240 (20V/V gain, 1mΩ shunt) |
| Communication | 2× RS485 (MAX485) at 115200 baud |
| Display | SPI LCD (ST7789, 160×128) |
| Storage | AT24Cxx I2C EEPROM |
| User Input | 4 keys (Back/Up/Down/Set) with long-press |
| Watchdog | IWDG (~4s timeout) |
| Power | 24V DC input |

## Software Architecture

### FreeRTOS Tasks

| Task | Priority | Period | Responsibility |
|------|----------|--------|----------------|
| `control_task` | 12 | 10ms | Motor control (manual jog, one-touch, auto-pressure PID) |
| `show_task` | 11 | 10ms | Key scanning, UI refresh, RS485 Modbus response, current sensing, watchdog feed |

### Pressure Control State Machine

```
                    ┌──────────────────────────────────────┐
                    │           TARGET SET                  │
                    └─────────────┬────────────────────────┘
                                  ▼
              ┌─────────────────────────────────────┐
              │  S2: COARSE MODE (|error| > 1000N)  │
              │  Direct PID voltage control         │
              └─────────────────┬───────────────────┘
                                ▼
              ┌─────────────────────────────────────┐
              │  S3: MICRO MODE  (|error| > 50N)    │
              │  Timed pulse drive                  │
              └─────────────────┬───────────────────┘
                                ▼
              ┌─────────────────────────────────────┐
              │  S4: FINE MODE   (|error| > 5N)     │
              │  Short pulse, reduced voltage       │
              └─────────────────┬───────────────────┘
                                ▼
              ┌─────────────────────────────────────┐
              │  S5: HOLD MODE   (|error| ≤ 5N)     │
              │  Motor off, drift monitoring        │
              └─────────────────────────────────────┘
```

### Modbus Register Map

**Function Code 0x01 — Read Coils (Status)**

| Coil Address | Description |
|-------------|-------------|
| 0x00 | Pressure auto-run active |
| 0x01 | Auto-run in progress |
| 0x02 | One-touch up active |
| 0x03 | One-touch down active |
| 0x04 | Jog/fast up active |
| 0x05 | Jog/fast down active |

**Function Code 0x03 — Read Holding Register**

| Register | Description |
|----------|-------------|
| 0x00 | Target pressure (0–1500N) |

**Function Code 0x04 — Read Input Register**

| Register | Description |
|----------|-------------|
| 0x00 | Actual pressure (0xFFFF if sensor disconnected) |

**Function Code 0x05 — Write Single Coil**

| Address | Value | Description |
|---------|-------|-------------|
| 0x00 | 0xFF00 | Switch to pressure control mode |
| 0x00 | 0x0000 | Switch to one-touch mode |
| 0x01 | 0xFF00/0x0000 | Start/stop auto pressure control |
| 0x02 | 0xFF00/0x0000 | One-touch up start/stop |
| 0x03 | 0xFF00/0x0000 | One-touch down start/stop |
| 0x04 | 0xFF00/0x0000 | Jog/fast up start/stop |
| 0x05 | 0xFF00/0x0000 | Jog/fast down start/stop |

**Function Code 0x06 — Write Single Register**

| Register | Range | Description |
|----------|-------|-------------|
| 0x00 | 0–1500 | Set target pressure (N) |

## Directory Structure

```
├── User/                            Application layer
│   ├── main.c                       Main entry, FreeRTOS tasks, Modbus handler
│   ├── freertos_demo.c/h            Legacy FreeRTOS demo (inactive)
│   ├── FreeRTOSConfig.h             FreeRTOS kernel configuration
│   ├── stm32f4xx_hal_conf.h         HAL peripheral configuration
│   └── stm32f4xx_it.c/h            Interrupt handlers
│
├── Drivers/
│   ├── BSP/                         Board Support Package
│   │   ├── PRESSURE_CONTROL/        Multi-mode pressure PID controller
│   │   ├── IR2104/                  Dual H-bridge PWM motor driver
│   │   ├── CURRENT/                 INA240 motor current sensing
│   │   ├── RS485/                   Dual RS485 + Modbus RTU protocol
│   │   ├── SHOW/                    LCD UI state machine (3 interfaces)
│   │   ├── LCD/                     SPI LCD low-level driver (ST7789)
│   │   ├── EXTI/                    4-key input with debounce & long-press
│   │   ├── ADC/                     ADC for current/pressure sensing
│   │   ├── 24CXX/                   EEPROM for persistent settings
│   │   ├── PID/                     Generic PID controller (legacy)
│   │   ├── LED/ BEEP/               GPIO indicator peripherals
│   │   ├── TIM/ TIMER/              Timer & PWM configuration
│   │   └── WDG/                     Independent watchdog (IWDG)
│   ├── CMSIS/                       ARM CMSIS headers & startup
│   ├── STM32F4xx_HAL_Driver/        ST HAL library
│   └── SYSTEM/                      System clock, delay, USART utilities
│
├── Middlewares/
│   ├── FreeRTOS/                    FreeRTOS v10.x kernel
│   ├── MALLOC/                      Custom memory allocator
│   └── XMRAM/                       External RAM library
│
├── EIDE/                            Embedded IDE project files
├── Projects/MDK-ARM/                Keil MDK project files
└── Output/                          Build output (hex, map, objects)
```

## Build

### Prerequisites

- **Keil MDK-ARM v5** (with STM32F4 device pack) or
- **VS Code** with [Embedded IDE (EIDE)](https://marketplace.visualstudio.com/items?itemName=CL.eide) extension
- **ARM GCC Toolchain** (for EIDE builds)

### Build with Keil MDK

1. Open `Projects/MDK-ARM/press_control_f412.uvprojx`
2. Select target `press_control_f412_FreeRTOS`
3. Build → Output at `Output/press_control_f411.hex`

### Build with EIDE

1. Open `EIDE/press_control_f412.code-workspace` in VS Code
2. EIDE sidebar → Build
3. Output at `EIDE/build/`

### Flash

Use ST-Link or J-Link to flash the `.hex` file to the STM32F412 target.

## Pin Assignment

| Function | Pin | Peripheral |
|----------|-----|-----------|
| RS485-1 TX | PA2 | USART2 |
| RS485-1 RX | PA3 | USART2 |
| RS485-1 RE | PA1 | GPIO |
| RS485-2 TX | PC6 | USART6 |
| RS485-2 RX | PC7 | USART6 |
| RS485-2 RE | PC8 | GPIO |
| KEY1 (Back) | PC9 | EXTI |
| KEY2 (Up) | PA8 | EXTI |
| KEY3 (Down) | PA9 | EXTI |
| KEY4 (Set) | PA10 | EXTI |
| Motor PWM 1 | TIM2_CH3 | PWM |
| Motor PWM 2 | TIM3_CH3 | PWM |
| Current ADC | ADC1_CH4 | ADC |
| I2C SDA/SCL | — | Software I2C |
| LCD SPI | — | SPI |

## License

Proprietary — Bayro Technology. Based on ALIENTEK STM32F4 platform drivers.
