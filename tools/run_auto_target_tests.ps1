param([string]$OutputDirectory = 'output/AutoTarget/tests')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$outDir = Join-Path $root $OutputDirectory
New-Item -ItemType Directory -Force $outDir | Out-Null
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$sources = @('Tests/Host/test_auto_target.c', 'Application/machine.c',
    'Application/force_pi.c', 'Application/auto_target_config.c',
    'Application/bench_config.c', 'Application/pressure_control.c', 'Application/runtime.c',
    'Board/Motor/motor_executor_real.c', 'Tests/Host/fake_motor_hw_real.c',
    'Tests/Host/fake_motor_stop_timer.c', 'Transport/Modbus/modbus_semantic_map.c',
    'Transport/Modbus/modbus_rtu_server.c', 'Protocol/Modbus/modbus_crc16.c')
$autoDefines = @('SD700_AUTO_TARGET_ENABLED=1', 'SD700_MOTOR_MODE_REAL_BENCH=1',
    'SD700_REAL_BENCH_ACKNOWLEDGED=1', 'SD700_REAL_OUTPUT_ARMING_ENABLED=1')
foreach ($opt in @('O0','O2')) {
    $exe = Join-Path $outDir "auto_target_$opt.exe"
    $arguments = @('-std=c11','-Wall','-Wextra','-Werror','-pedantic',"-I$root", "-$opt",
        '-DSD700_MOTOR_REAL_HOST_TEST=1')
    $arguments += $autoDefines | ForEach-Object { "-D$_" }
    $arguments += $sources | ForEach-Object { Join-Path $root $_ }
    $arguments += @('-o', $exe)
    & $gcc @arguments
    if ($LASTEXITCODE -ne 0) { throw "AutoTarget $opt compile failed" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "AutoTarget $opt test failed" }
    Write-Output "AUTO_TARGET_HOST=$opt PASS"
}

$cases = @(
    @{Name='target_confirmed'; Defines=$autoDefines + 'STM32F411xE'; Pass=$true},
    @{Name='host_confirmed'; Defines=$autoDefines + 'SD700_MOTOR_REAL_HOST_TEST=1'; Pass=$true},
    @{Name='locked'; Defines=@('SD700_AUTO_TARGET_ENABLED=1'); Pass=$false},
    @{Name='scope'; Defines=@('SD700_AUTO_TARGET_ENABLED=1','SD700_MOTOR_MODE_SCOPE_TEST=1','SD700_SCOPE_TEST_ACKNOWLEDGED=1','SD700_REAL_OUTPUT_ARMING_ENABLED=1'); Pass=$false},
    @{Name='compile_check'; Defines=@('SD700_AUTO_TARGET_ENABLED=1','SD700_MOTOR_MODE_REAL_COMPILE_CHECK=1'); Pass=$false},
    @{Name='missing_ack'; Defines=@('SD700_AUTO_TARGET_ENABLED=1','SD700_MOTOR_MODE_REAL_BENCH=1','SD700_REAL_OUTPUT_ARMING_ENABLED=1','STM32F411xE'); Pass=$false},
    @{Name='host_missing_ack'; Defines=@('SD700_AUTO_TARGET_ENABLED=1','SD700_MOTOR_MODE_REAL_BENCH=1','SD700_REAL_OUTPUT_ARMING_ENABLED=1','SD700_MOTOR_REAL_HOST_TEST=1'); Pass=$false},
    @{Name='unarmed'; Defines=@('SD700_AUTO_TARGET_ENABLED=1','SD700_MOTOR_MODE_REAL_BENCH=1','SD700_REAL_BENCH_ACKNOWLEDGED=1'); Pass=$false},
    @{Name='invalid_value'; Defines=@('SD700_AUTO_TARGET_ENABLED=2'); Pass=$false},
    @{Name='mixed_modes'; Defines=$autoDefines + 'SD700_MOTOR_MODE_SCOPE_TEST=1'; Pass=$false},
    @{Name='target_host_bypass'; Defines=$autoDefines + @('STM32F411xE','SD700_MOTOR_REAL_HOST_TEST=1'); Pass=$false}
)
foreach ($case in $cases) {
    $arguments = @('-std=c11','-Wall','-Wextra','-Werror','-pedantic','-fsyntax-only',"-I$root")
    $arguments += $case.Defines | ForEach-Object { "-D$_" }
    $arguments += Join-Path $root 'Tests/Host/test_motion_build_policy_compile.c'
    $ErrorActionPreference = 'Continue'
    $details = @(& $gcc @arguments 2>&1)
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $details | Out-File (Join-Path $outDir "policy_$($case.Name).log")
    if (($code -eq 0) -ne $case.Pass) { throw "Unexpected compile result: $($case.Name)" }
    Write-Output "AUTO_TARGET_POLICY=$($case.Name) PASS (compile exit=$code expected_pass=$($case.Pass))"
}
Write-Output 'AUTO_TARGET_TEST_SUITE=PASS SYNTHETIC_INPUT'
