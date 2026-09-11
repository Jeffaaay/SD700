param([string]$OutputDirectory = 'build/Host/Section9A')
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path "$PSScriptRoot\..").Path
$compiler = (Get-Command gcc -ErrorAction Stop).Source
$outDir = Join-Path $root $OutputDirectory
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Invoke-BuildPolicyCompile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Defines,
        [Parameter(Mandatory = $true)][bool]$ShouldPass,
        [string]$Source = 'Tests/Host/test_motion_build_policy_compile.c'
    )

    $arguments = @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
        '-fsyntax-only', "-I$root"
    )
    $arguments += $Defines | ForEach-Object { "-D$_" }
    $arguments += Join-Path $root $Source
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = @(& $compiler @arguments 2>&1)
    $compilerExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorActionPreference
    $passed = $compilerExitCode -eq 0
    if ($passed -ne $ShouldPass) {
        $output | Write-Output
        throw "$Name compile result was $passed; expected $ShouldPass"
    }
    Write-Output "COMPILE_POLICY_RESULT=$Name PASS"
}

Invoke-BuildPolicyCompile -Name 'scope_missing_c_ack' `
    -Defines @('SD700_MOTOR_MODE_SCOPE_TEST=1') `
    -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'scope_zero_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=0'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'scope_invalid_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=2'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'real_bench_missing_c_ack' `
    -Defines @('SD700_MOTOR_MODE_REAL_BENCH=1') `
    -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'real_bench_zero_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=0'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'real_bench_invalid_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=2'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'scope_mismatched_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=1'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'real_bench_mismatched_c_ack' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=1'
    ) -ShouldPass $false
Invoke-BuildPolicyCompile -Name 'scope_explicit_host' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    ) -ShouldPass $true
Invoke-BuildPolicyCompile -Name 'real_bench_explicit_host' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    ) -ShouldPass $true

$motorRealConfigCompileSource =
    'Tests/Host/test_motor_real_config_compile.c'
Invoke-BuildPolicyCompile -Name 'arming_without_mode' `
    -Defines @('SD700_REAL_OUTPUT_ARMING_ENABLED=1') `
    -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'host_zero_arming_without_mode' `
    -Defines @(
        'SD700_MOTOR_REAL_HOST_TEST=0',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'host_one_arming_without_mode' `
    -Defines @(
        'SD700_MOTOR_REAL_HOST_TEST=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $true -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'target_rejects_host_exception' `
    -Defines @(
        'SD700_MOTOR_REAL_HOST_TEST=1',
        'STM32F411xE=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'scope_acknowledged_armed' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $true -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'real_bench_acknowledged_armed' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $true -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'scope_acknowledged_unarmed' `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_SCOPE_TEST_ACKNOWLEDGED=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=0'
    ) -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'real_bench_acknowledged_unarmed' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_REAL_BENCH_ACKNOWLEDGED=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=0'
    ) -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'real_compile_check_armed' `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    ) -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'real_compile_check_mode_zero' `
    -Defines @('SD700_MOTOR_MODE_REAL_COMPILE_CHECK=0') `
    -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'scope_mode_zero' `
    -Defines @('SD700_MOTOR_MODE_SCOPE_TEST=0') `
    -ShouldPass $false -Source $motorRealConfigCompileSource
Invoke-BuildPolicyCompile -Name 'real_bench_mode_zero' `
    -Defines @('SD700_MOTOR_MODE_REAL_BENCH=0') `
    -ShouldPass $false -Source $motorRealConfigCompileSource
Write-Output 'ACK_POLICY_COMPILE_TESTS=PASS'
Write-Output 'HOST_ZERO_ARMING_BYPASS_TEST=PASS'
Write-Output 'TARGET_HOST_MACRO_REJECTION=PASS'

function Invoke-HostTest {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Sources,
        [string[]]$Defines = @(),
        [string[]]$ExtraFlags = @(),
        [string[]]$ExtraIncludes = @(),
        [ValidateRange(1, 100)][int]$RunCount = 1
    )

    Write-Output "HOST_TEST=$Name"
    $executable = Join-Path $outDir "$Name.exe"
    $arguments = @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
        "-I$root"
    )
    $arguments += $ExtraIncludes | ForEach-Object {
        "-I$(Join-Path $root $_)"
    }
    $arguments += $ExtraFlags
    $arguments += $Defines | ForEach-Object { "-D$_" }
    $arguments += $Sources | ForEach-Object { Join-Path $root $_ }
    $arguments += @('-o', $executable)

    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name compilation failed with exit code $LASTEXITCODE"
    }
    for ($run = 1; $run -le $RunCount; ++$run) {
        & $executable
        if ($LASTEXITCODE -ne 0) {
            throw "$Name run $run failed with exit code $LASTEXITCODE"
        }
    }
    Write-Output "HOST_TEST_RESULT=$Name PASS"
}

$forcePiSources = @(
    'Tests/Host/test_force_pi.c',
    'Application/force_pi.c'
)
$machineForcePiSources = @(
    'Tests/Host/test_machine_force_pi.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Tests/Host/fake_motor_executor.c'
)
foreach ($optimization in @('O0', 'O2')) {
    Invoke-HostTest -Name "test_force_pi_$optimization" `
        -Sources $forcePiSources -ExtraFlags @("-$optimization")
    Invoke-HostTest -Name "test_machine_force_pi_locked_$optimization" `
        -Sources $machineForcePiSources -ExtraFlags @("-$optimization")
    foreach ($mode in @('REAL_COMPILE_CHECK', 'SCOPE_TEST', 'REAL_BENCH')) {
        Invoke-HostTest -Name "test_machine_force_pi_${mode}_$optimization" `
            -Sources $machineForcePiSources -ExtraFlags @("-$optimization") `
            -Defines @("SD700_MOTOR_MODE_$mode=1", 'SD700_MOTOR_REAL_HOST_TEST=1')
    }
}

Invoke-HostTest -Name 'test_pressure_sensor_protocol' -Sources @(
    'Tests/Host/test_pressure_sensor_protocol.c',
    'Protocol/PressureSensor/pressure_sensor_bcc.c',
    'Protocol/PressureSensor/pressure_sensor_decode_frame.c'
)
Invoke-HostTest -Name 'test_modbus_crc16' -Sources @(
    'Tests/Host/test_modbus_crc16.c',
    'Protocol/Modbus/modbus_crc16.c'
)
Invoke-HostTest -Name 'test_pressure_receiver' -Sources @(
    'Tests/Host/test_pressure_receiver.c',
    'Transport/Pressure/pressure_receiver.c',
    'Protocol/PressureSensor/pressure_sensor_bcc.c',
    'Protocol/PressureSensor/pressure_sensor_decode_frame.c'
)
Invoke-HostTest -Name 'test_motor_executor_locked' -Sources @(
    'Tests/Host/test_motor_executor.c',
    'Board/Motor/motor_executor_locked.c',
    'Tests/Host/fake_motor_hw.c'
)
Invoke-HostTest -Name 'test_benchmark0' -Sources @(
    'Tests/Host/test_benchmark0.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Tests/Host/fake_motor_executor.c'
)
Invoke-HostTest -Name 'test_modbus_rtu' -Sources @(
    'Tests/Host/test_modbus_rtu.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Tests/Host/fake_motor_executor.c',
    'Protocol/Modbus/modbus_crc16.c',
    'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c',
    'Transport/Modbus/modbus_rtu_link.c'
)
Invoke-HostTest -Name 'test_modbus_uart_ingress_concurrency' -Sources @(
    'Tests/Host/test_modbus_uart_ingress_concurrency.c',
    'Transport/Modbus/modbus_uart_ingress.c'
) -ExtraFlags @('-pthread') -RunCount 20
Write-Output 'CONCURRENCY_STRESS_REPEATS=20'
Write-Output 'CONCURRENCY_STRESS_RESULT=PASS'
Invoke-HostTest -Name 'test_runtime_integration' -Sources @(
    'Tests/Host/test_runtime_integration.c',
    'Application/runtime.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Application/bench_config.c',
    'Application/pressure_control.c',
    'Board/Motor/motor_executor_locked.c',
    'Tests/Host/fake_motor_hw.c',
    'Transport/Pressure/pressure_receiver.c',
    'Protocol/PressureSensor/pressure_sensor_bcc.c',
    'Protocol/PressureSensor/pressure_sensor_decode_frame.c',
    'Protocol/Modbus/modbus_crc16.c',
    'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c'
)
Invoke-HostTest -Name 'test_motor_executor_real' -Sources @(
    'Tests/Host/test_motor_executor_real.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c'
) -Defines @(
    'SD700_MOTOR_REAL_HOST_TEST=1',
    'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
)
Invoke-HostTest -Name 'test_motor_executor_real_compile_check' -Sources @(
    'Tests/Host/test_motor_executor_real_compile_check.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c'
) -Defines @(
    'SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1',
    'SD700_REAL_OUTPUT_ARMING_ENABLED=0'
)
Invoke-HostTest -Name 'test_machine_real_integration' -Sources @(
    'Tests/Host/test_machine_real_integration.c',
    'Application/runtime.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Application/bench_config.c',
    'Application/pressure_control.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c'
) -Defines @(
    'SD700_MOTOR_REAL_HOST_TEST=1',
    'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
)

$directSources = @(
    'Tests/Host/test_direct_pulse_commands.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Tests/Host/fake_motor_executor.c'
)
Invoke-HostTest -Name 'test_direct_pulse_locked' -Sources $directSources
Invoke-HostTest -Name 'test_direct_pulse_real_compile_check' `
    -Sources $directSources `
    -Defines @('SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1')
Invoke-HostTest -Name 'test_direct_pulse_scope_test' `
    -Sources $directSources `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    )
Invoke-HostTest -Name 'test_direct_pulse_real_bench' `
    -Sources $directSources `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    )

$modbusDirectSources = @(
    'Tests/Host/test_modbus_direct_commands.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Tests/Host/fake_motor_executor.c',
    'Protocol/Modbus/modbus_crc16.c',
    'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c'
)
Invoke-HostTest -Name 'test_modbus_direct_locked' `
    -Sources $modbusDirectSources
Invoke-HostTest -Name 'test_modbus_direct_real_compile_check' `
    -Sources $modbusDirectSources `
    -Defines @('SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1')
Invoke-HostTest -Name 'test_modbus_direct_scope_test' `
    -Sources $modbusDirectSources `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    )
Invoke-HostTest -Name 'test_modbus_direct_real_bench' `
    -Sources $modbusDirectSources `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_MOTOR_REAL_HOST_TEST=1'
    )

$fullChainDirectSources = @(
    'Tests/Host/test_direct_pulse_full_chain.c',
    'Application/runtime.c',
    'Application/machine.c',
    'Application/force_pi.c',
    'Application/bench_config.c',
    'Application/pressure_control.c',
    'Board/Motor/motor_executor_real.c',
    'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c',
    'Protocol/Modbus/modbus_crc16.c',
    'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c'
)
Invoke-HostTest -Name 'test_direct_pulse_full_chain_scope_test' `
    -Sources $fullChainDirectSources `
    -Defines @(
        'SD700_MOTOR_MODE_SCOPE_TEST=1',
        'SD700_MOTOR_REAL_HOST_TEST=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    )
Invoke-HostTest -Name 'test_direct_pulse_full_chain_real_bench' `
    -Sources $fullChainDirectSources `
    -Defines @(
        'SD700_MOTOR_MODE_REAL_BENCH=1',
        'SD700_MOTOR_REAL_HOST_TEST=1',
        'SD700_REAL_OUTPUT_ARMING_ENABLED=1'
    )

Invoke-HostTest -Name 'test_motor_stop_timer_tim5' -Sources @(
    'Tests/Host/test_motor_stop_timer_tim5.c',
    'Board/Motor/motor_stop_timer_tim5.c',
    'Tests/Host/fake_stm32_hal.c'
) -ExtraIncludes @('Tests/Host/Shim')
Invoke-HostTest -Name 'test_motor_hw_real_diagnostics' -Sources @(
    'Tests/Host/test_motor_hw_real_diagnostics.c',
    'Board/Motor/motor_hw_real.c',
    'Tests/Host/fake_stm32_hal.c'
) -ExtraIncludes @('Tests/Host/Shim')

& (Join-Path $root 'Tests\Host\check_direct_command_ownership.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Direct command ownership check failed.'
}

$windowsPowerShell = (Get-Command powershell.exe -ErrorAction Stop).Source
& $windowsPowerShell -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $root 'tools\capture_pressure_response.ps1') -SelfTest
if ($LASTEXITCODE -ne 0) {
    throw 'Pressure response capture self-test failed.'
}

Write-Output 'HOST_TEST_SUITE=PASS'
