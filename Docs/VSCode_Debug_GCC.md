# VSCode GCC debug setup

This project now has a self-contained VSCode build/debug entry for the active STM32F411RCTx firmware target.

## Requirements

- Arm GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`)
- VSCode C/C++ extension
- VSCode Cortex-Debug extension
- OpenOCD for ST-Link debugging

The build script first searches `PATH`, then falls back to:

```text
C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin
```

## Build

In VSCode, run:

```text
Terminal -> Run Build Task -> Build firmware (GCC Debug)
```

or from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_gcc.ps1 -Configuration Debug
```

Outputs:

```text
build/Debug/press_control_f411.elf
build/Debug/press_control_f411.hex
build/Debug/press_control_f411.map
```

## Debug

Install OpenOCD and make `openocd.exe` visible to Cortex-Debug, either by adding it to `PATH` or by setting `cortex-debug.openocdPath` in VSCode settings.

Then use:

```text
Run and Debug -> Debug STM32F411 (ST-Link/OpenOCD)
```

The launch config builds first, flashes through ST-Link/OpenOCD, and stops at `main`.

## Notes

- The GCC source list mirrors the active Keil target in `Projects/MDK-ARM/press_control_f411.uvprojx`.
- The GCC build uses the repository's GCC startup file instead of the Keil ARMASM startup file.
- The linker script matches the Keil scatter memory map: 256 KiB flash at `0x08000000`, 128 KiB RAM at `0x20000000`.
