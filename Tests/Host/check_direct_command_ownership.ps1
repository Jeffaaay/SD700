$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$ownedRoots = @('Application', 'Transport', 'Protocol')
$sourceFiles = foreach ($directory in $ownedRoots) {
    Get-ChildItem -LiteralPath (Join-Path $root $directory) -Recurse -File |
        Where-Object { $_.Extension -in @('.c', '.h') }
}

$forbiddenPatterns = @(
    '#include\s+"Board/Motor/motor_hw_real\.h"',
    '#include\s+"Board/Motor/motor_stop_timer\.h"',
    '\bHAL_TIM_PWM_[A-Za-z0-9_]*\s*\(',
    '\bTIM[23]\s*->',
    '\bGPIOB\s*->',
    '\bCCR3\s*=',
    '\bPB(?:0|1|2|10)\b'
)

foreach ($file in $sourceFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($pattern in $forbiddenPatterns) {
        if ($text -match $pattern) {
            throw "Direct hardware ownership violation: $($file.FullName) pattern=$pattern"
        }
    }
}

$semantic = Get-Content -LiteralPath (
    Join-Path $root 'Transport\Modbus\modbus_semantic_map.c') -Raw
$machine = Get-Content -LiteralPath (
    Join-Path $root 'Application\machine.c') -Raw
if (($semantic -notmatch 'CMD_DIRECT_PRESS_PULSE') -or
    ($semantic -notmatch 'CMD_DIRECT_RELEASE_PULSE')) {
    throw 'Modbus semantic direct commands are missing.'
}
if ($machine -notmatch 'MotorExecutor_StartPulse\s*\(') {
    throw 'Machine does not route direct commands through MotorExecutor_StartPulse.'
}

Write-Output 'DIRECT_COMMAND_PATH=Modbus->Machine->MotorExecutor_StartPulse'
Write-Output 'NO_DIRECT_HW_WRITES=PASS'
