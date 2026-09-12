param([string]$OutputDirectory = 'build\Host\CompletionRaces')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$compiler = (Get-Command gcc -ErrorAction Stop).Source
$outDir = Join-Path $root $OutputDirectory
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$executorSources = @(
    'Tests/Host/test_motor_executor_real.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c'
)
$fullChainSources = @(
    'Tests/Host/test_direct_pulse_full_chain.c',
    'Application/runtime.c', 'Application/machine.c',
    'Application/force_pi.c',
    'Application/bench_config.c', 'Application/pressure_control.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c', 'Tests/Host/fake_motor_stop_timer.c',
    'Protocol/Modbus/modbus_crc16.c',
    'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c'
)

foreach ($optimization in @('O0', 'O2')) {
    foreach ($variant in @('Executor', 'ScopeTest', 'RealBench')) {
        $exe = Join-Path $outDir "${variant}_${optimization}.exe"
        $arguments = @(
            '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
            "-I$root", "-$optimization",
            '-DSD700_MOTOR_REAL_HOST_TEST=1',
            '-DSD700_REAL_OUTPUT_ARMING_ENABLED=1'
        )
        if ($variant -eq 'Executor') {
            $sources = $executorSources
        }
        else {
            $sources = $fullChainSources
            if ($variant -eq 'ScopeTest') {
                $arguments += '-DSD700_MOTOR_MODE_SCOPE_TEST=1'
            }
            else {
                $arguments += '-DSD700_MOTOR_MODE_REAL_BENCH=1'
            }
        }
        $arguments += $sources | ForEach-Object { Join-Path $root $_ }
        $arguments += @('-o', $exe)
        & $compiler @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$variant $optimization compile failed ($LASTEXITCODE)"
        }
        & $exe
        if ($LASTEXITCODE -ne 0) {
            throw "$variant $optimization regression failed ($LASTEXITCODE)"
        }
        Write-Output "COMPLETION_RACES=$variant $optimization PASS"
    }
}

# Check the actual preprocessed loops for BOTH owners. ForceServo adds an early
# STOP pass before PID consumption; the legacy ordering assertion stays intact.
foreach ($servo in @(0,1)) {
    $preArgs=@('-E','-P','-DUSE_HAL_DRIVER','-DSTM32F411xE',
        "-DSD700_FORCE_SERVO_ENABLED=$servo",'-DSD700_MOTOR_MODE_REAL_BENCH=1',
        '-DSD700_REAL_BENCH_ACKNOWLEDGED=1','-DSD700_REAL_OUTPUT_ARMING_ENABLED=1')
    foreach ($inc in @('.','User','Drivers','Drivers/CMSIS/Device/ST/STM32F4xx/Include',
                      'Drivers/CMSIS/Include','Drivers/STM32F4xx_HAL_Driver/Inc')) {
        $preArgs += "-I$(Join-Path $root $inc)"
    }
    $preArgs += Join-Path $root 'User/main.c'
    $main=(& arm-none-eabi-gcc @preArgs) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Main preprocessing failed' }
    $loopAt=$main.IndexOf('for (;;)',[StringComparison]::Ordinal)
    if ($loopAt -lt 0) { throw 'Main loop was not found.' }
    $loop=$main.Substring($loopAt)
    $calls=@()
    if ($servo) { $calls+=@('ModbusUart2_ServiceMain(','ModbusUart2_ProcessPendingStop(') }
    $calls+=@('PressureUart6_ReadSnapshot(','ApplicationRuntime_ServicePressure(',
        'ApplicationRuntime_ServiceSafety(','MotorExecutor_GuardOutput(',
        'ModbusUart2_ServiceMain(','ModbusUart2_ProcessPendingStop(',
        'if (!stop_processed)','ModbusUart2_ProcessOneNormalRequest(')
    $previous=-1
    foreach ($call in $calls) {
        $position=$loop.IndexOf($call,$previous+1,[StringComparison]::Ordinal)
        if ($position -le $previous) { throw "Main-loop safety/STOP order invalid owner=$servo at $call" }
        $previous=$position
    }
    Write-Output "MAIN_OWNER_ORDER=$servo PASS"
}
Write-Output 'MAIN_PRESSURE_SAFETY_GUARD_STOP_ORDER=PASS'
Write-Output 'COMPLETION_RACE_SUITE=PASS'
