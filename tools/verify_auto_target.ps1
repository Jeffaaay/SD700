param([switch]$SkipBuilds, [switch]$SkipHost)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$out = Join-Path $root 'output/AutoTarget'
New-Item -ItemType Directory -Force $out | Out-Null
$shellExe = (Get-Command powershell.exe).Source
function Run-Logged {
    param([string]$Name, [string[]]$Arguments, [bool]$ExpectedSuccess=$true)
    $log = Join-Path $out "$Name.log"
    $ErrorActionPreference = 'Continue'
    & $shellExe -NoProfile -ExecutionPolicy Bypass @Arguments *> $log
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if (($code -eq 0) -ne $ExpectedSuccess) {
        Get-Content $log -Tail 30 | Write-Output
        throw "$Name unexpected exit code $code"
    }
    Write-Output "$Name=PASS (exit=$code expected_success=$ExpectedSuccess)"
}
if (-not $SkipHost) {
Run-Logged 'host_suite' @('-File', "$PSScriptRoot/run_host_tests.ps1", '-OutputDirectory', 'output/AutoTarget/tests/host')
Run-Logged 'completion_races' @('-File', "$root/Tests/Host/run_completion_race_regressions.ps1", '-OutputDirectory', 'output/AutoTarget/tests/races')
Run-Logged 'auto_tests' @('-File', "$PSScriptRoot/run_auto_target_tests.ps1")
Run-Logged 'physical_output_lock' @('-File', "$root/Tests/Host/check_physical_output_lock.ps1")
Run-Logged 'auto_capture_self_test' @('-File', "$PSScriptRoot/capture_auto_target_static.ps1", '-SelfTest')
} else { Write-Output 'HOST_TESTS=NOT_RUN_THIS_INVOCATION (SkipHost; prior logs retained)' }
foreach ($mode in @('Locked','RealCompileCheck','ScopeTest','RealBench')) {
    $a=@('-File', "$PSScriptRoot/build_gcc.ps1", '-AutoTarget', '-MotorMode', $mode)
    # Each is invalid: wrong mode or RealBench missing required acknowledgement.
    Run-Logged "build_reject_$mode" $a $false
}
if (-not $SkipBuilds) {
    foreach ($mode in @('Locked','RealCompileCheck','ScopeTest','RealBench','AutoTarget')) {
        foreach ($config in @('Debug','Release')) {
            $actualMode = if ($mode -eq 'AutoTarget') {'RealBench'} else {$mode}
            $a=@('-File', "$PSScriptRoot/build_gcc.ps1", '-MotorMode', $actualMode,
                '-Configuration', $config, '-BuildDir', 'output/AutoTarget/build')
            if ($mode -eq 'ScopeTest') { $a += @('-ScopeTestAck','MOTOR_POWER_DISCONNECTED') }
            if ($actualMode -eq 'RealBench') { $a += @('-RealBenchAck','I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION') }
            if ($mode -eq 'AutoTarget') { $a += '-AutoTarget' }
            Run-Logged "build_${mode}_$config" $a
        }
    }
} else { Write-Output 'ARM_BUILD_MATRIX=NOT_RUN (SkipBuilds)' }
Write-Output 'POWERED_STATIC_TEST=NOT_RUN; INPUT_CURVES=SYNTHETIC_INPUT'
