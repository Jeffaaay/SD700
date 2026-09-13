param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [ValidateSet("Locked", "RealCompileCheck", "ScopeTest", "RealBench")]
    [string]$MotorMode = "Locked",
    [string]$BuildDir = "build",
    [string]$ScopeTestAck = "",
    [string]$RealBenchAck = "",
    [switch]$AutoTarget,
    [switch]$ForceServo,
    [switch]$Target250Authority1
)

$ErrorActionPreference = "Stop"
if ($Target250Authority1 -and -not $ForceServo) { throw 'Target250Authority1 requires -ForceServo' }
if ($ForceServo -and ($AutoTarget -or $MotorMode -ne 'RealBench')) {
    throw 'ForceServo requires RealBench and excludes AutoTarget'
}
if ($ForceServo) {
    if ($Target250Authority1) { Write-Host 'TARGET250_AUTHORITY1; SHORT_SUPERVISED_EXPERIMENT; PRESS_CAP_720_RELEASE_CAP_100; PHYSICAL_TEST_NOT_RUN' }
    else { Write-Host 'FORCE_SERVO1_PHYSICAL_OUTPUT_LOCKED; COMMISSIONING_NOT_TUNED' }
}


if ($AutoTarget -and $MotorMode -ne 'RealBench') {
    throw 'AutoTarget requires -MotorMode RealBench and its acknowledgement'
}

if ($MotorMode -eq "ScopeTest") {
    if ($ScopeTestAck -cne "MOTOR_POWER_DISCONNECTED") {
        throw "ScopeTest requires -ScopeTestAck MOTOR_POWER_DISCONNECTED"
    }
    Write-Host "SCOPE_TEST_ONLY"
    Write-Host "MOTOR_POWER_MUST_BE_PHYSICALLY_DISCONNECTED"
    Write-Host "AUTOMATIC_CLOSED_LOOP_DISABLED"
    Write-Host "DIRECT_PULSE_ONLY"
}

if ($MotorMode -eq "RealBench") {
    if ($RealBenchAck -cne "I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION") {
        throw "RealBench requires -RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION"
    }
    Write-Host "REAL_BENCH_ONLY"
    Write-Host "REAL_MOTOR_MAY_MOVE"
    Write-Host "LOW_ENERGY_SUPPLY_ESTOP_AND_CLEARANCE_REQUIRED"
    if ($ForceServo) {
        if ($Target250Authority1) { Write-Host 'TARGET250; NEXT_FRESH_SAMPLE_START; CONTINUOUS_ONLY; EXISTING_HW_GUARD_REQUIRED' }
        else { Write-Host 'FORCE_SERVO_CONTROLLER_IMPLEMENTED; ALL_TARGET_OUTPUT_LOCKED' }
    } elseif ($AutoTarget) {
        Write-Host 'AUTO_TARGET_OPERATOR_START_ENABLED; BOOT_SAFE_TO_IDLE'
        Write-Host 'P_ONLY_INITIAL_VALUES_UNVALIDATED; POWERED_TEST_NOT_RUN'
    } else {
        Write-Host "AUTOMATIC_CLOSED_LOOP_DISABLED"
        Write-Host "DIRECT_PULSE_ONLY"
    }
}

function Find-Tool($name, $fallback) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    if ($fallback -and (Test-Path -LiteralPath $fallback)) {
        return $fallback
    }
    throw "Cannot find $name. Install Arm GNU Toolchain or add it to PATH."
}

function Invoke-Checked($exe, $arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$exe failed with exit code $LASTEXITCODE"
    }
}

$gccFallback = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-gcc.exe"
$objcopyFallback = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-objcopy.exe"
$sizeFallback = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-size.exe"
$nmFallback = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-nm.exe"

$gcc = Find-Tool "arm-none-eabi-gcc" $gccFallback
$objcopy = Find-Tool "arm-none-eabi-objcopy" $objcopyFallback
$size = Find-Tool "arm-none-eabi-size" $sizeFallback
$nm = Find-Tool "arm-none-eabi-nm" $nmFallback

$root = (Resolve-Path "$PSScriptRoot\..").Path
$modeBuildDir = if ($ForceServo) {
    Join-Path $BuildDir 'RealBench_ForceServo'
} elseif ($AutoTarget) {
    Join-Path $BuildDir 'RealBench_AutoTarget'
} elseif ($MotorMode -eq "Locked") {
    $BuildDir
} else {
    Join-Path $BuildDir $MotorMode
}
$outDir = Join-Path (Join-Path $root $modeBuildDir) $Configuration
$objDir = Join-Path $outDir "obj"
New-Item -ItemType Directory -Force -Path $outDir, $objDir | Out-Null

$defines = @(
    "USE_HAL_DRIVER",
    "STM32F411xE"
)
if ($MotorMode -eq "RealCompileCheck") {
    $defines += @(
        "SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1",
        "SD700_REAL_OUTPUT_ARMING_ENABLED=0"
    )
}
if ($MotorMode -eq "ScopeTest") {
    $defines += @(
        "SD700_MOTOR_MODE_SCOPE_TEST=1",
        "SD700_SCOPE_TEST_ACKNOWLEDGED=1",
        "SD700_REAL_OUTPUT_ARMING_ENABLED=1"
    )
}
if ($MotorMode -eq "RealBench") {
    $defines += @(
        "SD700_MOTOR_MODE_REAL_BENCH=1",
        "SD700_REAL_BENCH_ACKNOWLEDGED=1",
        "SD700_REAL_OUTPUT_ARMING_ENABLED=1"
    )
}
if ($AutoTarget) { $defines += 'SD700_AUTO_TARGET_ENABLED=1' }
if ($ForceServo) { $defines += 'SD700_FORCE_SERVO_ENABLED=1' }
if ($Target250Authority1) { $defines += 'SD700_FORCE_SERVO_COMMISSIONING=1' }

$includes = @(
    ".",
    "User",
    "Drivers",
    "Drivers/CMSIS/Device/ST/STM32F4xx/Include",
    "Drivers/CMSIS/Include",
    "Drivers/STM32F4xx_HAL_Driver/Inc"
)

$sources = @(
    "Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s",
    "User/main.c",
    "User/stm32f4xx_it.c",
    "Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c",
    "Drivers/SYSTEM/sys/sys.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_usart.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c",
    "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c",
    "Application/safe_boot.c",
    "Application/safe_idle.c",
    "Application/bench_config.c",
    "Application/auto_target_config.c",
    "Application/pressure_control.c",
    "Application/machine.c",
    "Application/force_pi.c",
    "Application/runtime.c",
    "Board/Motor/motor_hw_early_force_disable.c",
    "Board/Motor/motor_hw_init_disabled.c",
    "Board/Motor/motor_hw_force_disable.c",
    "Board/RS485/pressure_uart6.c",
    "Board/RS485/modbus_uart2.c",
    "Transport/Pressure/pressure_receiver.c",
    "Transport/Modbus/modbus_semantic_map.c",
    "Transport/Modbus/modbus_rtu_server.c",
    "Transport/Modbus/modbus_rtu_link.c",
    "Transport/Modbus/modbus_uart_ingress.c",
    "Protocol/PressureSensor/pressure_sensor_bcc.c",
    "Protocol/PressureSensor/pressure_sensor_decode_frame.c",
    "Protocol/Modbus/modbus_crc16.c",
    "tools/syscalls.c"
)

if ($MotorMode -eq "Locked") {
    $sources += "Board/Motor/motor_executor_locked.c"
} else {
    $sources += @(
        "Board/Motor/motor_real_gate.c",
        "Board/Motor/motor_hw_real.c",
        "Board/Motor/motor_stop_timer_tim5.c",
        "Board/Motor/motor_executor_real.c"
    )
}

if ($ForceServo) {
    $sources += @('Application/force_servo.c', 'Application/force_servo_machine.c',
                  'Transport/Modbus/force_servo_protocol.c')
}

$cpuFlags = @("-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard")
$commonFlags = @("-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Wno-unused-parameter")
$debugFlags = if ($Configuration -eq "Debug") { @("-Og", "-g3") } else { @("-O2", "-g") }
$defineFlags = $defines | ForEach-Object { "-D$_" }
$includeFlags = $includes | ForEach-Object { "-I$(Join-Path $root $_)" }

$objects = @()
foreach ($src in $sources) {
    $srcPath = Join-Path $root $src
    if (!(Test-Path -LiteralPath $srcPath)) {
        throw "Missing source: $src"
    }

    $objName = ($src -replace '[:\\/]', '_') -replace '\.(c|s|S)$', '.o'
    $objPath = Join-Path $objDir $objName
    $objects += $objPath

    $args = @()
    $args += $cpuFlags
    $args += $commonFlags
    $args += $debugFlags
    $args += $defineFlags
    $args += $includeFlags
    $args += @("-MMD", "-MP", "-c", $srcPath, "-o", $objPath)
    Invoke-Checked $gcc $args
}

$elf = Join-Path $outDir "press_control_f411.elf"
$hex = Join-Path $outDir "press_control_f411.hex"
$map = Join-Path $outDir "press_control_f411.map"
$ldScript = Join-Path $root "linker/STM32F411RCTx_FLASH.ld"

$linkArgs = @()
$linkArgs += $cpuFlags
$linkArgs += @("-T$ldScript", "-Wl,-Map=$map", "-Wl,--gc-sections", "-Wl,--no-warn-rwx-segments", "--specs=nano.specs", "--specs=nosys.specs")
if ($ForceServo) { $linkArgs += @('-Wl,-u,g_force_servo_contract','-Wl,-u,g_force_servo_default_config') }
$linkArgs += $objects
$linkArgs += @("-Wl,--start-group", "-lc", "-lm", "-Wl,--end-group", "-o", $elf)
Invoke-Checked $gcc $linkArgs

if ($MotorMode -ne "Locked") {
    $nmOutput = @(& $nm $elf)
    if ($LASTEXITCODE -ne 0) {
        throw "$nm failed with exit code $LASTEXITCODE"
    }
    $tim5Symbols = @($nmOutput | Where-Object {
        $_ -match '\s+[A-Za-z]\s+TIM5_IRQHandler\s*$'
    })
    $strongTim5Symbols = @($tim5Symbols | Where-Object {
        $_ -match '^\s*[0-9A-Fa-f]+\s+T\s+TIM5_IRQHandler\s*$'
    })
    if ($strongTim5Symbols.Count -ne 1) {
        $found = if ($tim5Symbols.Count -eq 0) {
            "missing"
        } else {
            $tim5Symbols -join "; "
        }
        throw "TIM5_IRQHandler must be a strong text symbol; found: $found"
    }
    Write-Host "TIM5_SYMBOL=T TIM5_IRQHandler"
}

Invoke-Checked $objcopy @("-O", "ihex", $elf, $hex)
Invoke-Checked $size @($elf)

$elfSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash.ToLowerInvariant()
$hexSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $hex).Hash.ToLowerInvariant()

Write-Host "Built $elf"
Write-Host "Built $hex"
Write-Host "MotorMode=$MotorMode"
Write-Host "AutoTarget=$AutoTarget"
Write-Host "ELF_SHA256=$elfSha256"
Write-Host "HEX_SHA256=$hexSha256"
