# SD700 EIDE VSCode Toolchain

Open `SD700-EIDE.code-workspace` with VSCode. The EIDE extension should show `press_control_f411` in the EIDE project panel. The EIDE project file is `.eide/eide.yml`.

For the verified VSCode build task, run:

```text
Terminal -> Run Build Task -> EIDE Build Debug
```

Outputs are generated under:

```text
EIDE/build/Debug
EIDE/build/Release
```

The build tasks call the root GCC build script:

```text
../tools/build_gcc.ps1
```

Required local tools:

- Arm GNU Toolchain
- VSCode C/C++ extension
- Cortex-Debug extension if you want ST-Link debugging
- OpenOCD in PATH, or configured through `cortex-debug.openocdPath`

This workspace uses `interface/stlink-hla.cfg` because the connected ST-Link V2
loses the target with OpenOCD's newer dapdirect driver. This HLA mode has been
verified to detect the STM32F411 Cortex-M4 core.
