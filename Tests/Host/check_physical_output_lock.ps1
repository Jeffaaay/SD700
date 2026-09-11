$ErrorActionPreference = 'Stop'

$workspace = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..\..'))
$projectPath = Join-Path $workspace 'Projects\MDK-ARM\press_control_f411.uvprojx'
[xml]$project = Get-Content -LiteralPath $projectPath
$projectDirectory = Split-Path -Parent $projectPath
$sourcePaths = @(
    $project.Project.Targets.Target.Groups.Group.Files.File.FilePath |
        ForEach-Object {
            [System.IO.Path]::GetFullPath((Join-Path $projectDirectory $_))
        } |
        Where-Object {
            ($_ -like '*.c') -and ($_ -notlike '*\Drivers\STM32F4xx_HAL_Driver\*')
        }
)
$activeFileNames = @(
    $project.Project.Targets.Target.Groups.Group.Files.File.FileName
)

$lockHeader = Get-Content -LiteralPath (
    Join-Path $workspace 'Application\bench_config.h') -Raw
if ($lockHeader -notmatch '#define\s+SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED\s+1') {
    throw 'Physical-output lock definition is missing or not 1.'
}
if ($lockHeader -notmatch '#if\s+SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED\s*!=\s*1') {
    throw 'Required physical-output lock compile-time assertion is missing.'
}

$motorExecutors = @($activeFileNames | Where-Object {
    $_ -like 'motor_executor*.c'
})
if (($motorExecutors.Count -ne 1) -or
    ($motorExecutors[0] -ne 'motor_executor_locked.c')) {
    throw ('Active project does not contain only the locked MotorExecutor: ' +
           ($motorExecutors -join ', '))
}

$mainText = Get-Content -LiteralPath (Join-Path $workspace 'User\main.c') -Raw
if ($mainText -notmatch
    'MotorHw_EarlyForceDisable\s*\(\s*\)\s*;\s*HAL_Init\s*\(\s*\)\s*;') {
    throw 'main() does not force-disable immediately before HAL_Init().'
}

$pwmStartHits = @()
$blockingReceiveHits = @()
$channelEnableHits = @()
$sdHighHits = @()
$nonzeroCcrHits = @()
$dynamicBrakeHits = @()

foreach ($path in $sourcePaths) {
    $text = Get-Content -LiteralPath $path -Raw
    $compact = $text -replace '\s+', ' '

    $pwmStartHits += @(Select-String -LiteralPath $path -Pattern 'HAL_TIM_PWM_Start')
    $blockingReceiveHits += @(
        Select-String -LiteralPath $path -Pattern 'HAL_UART_Receive\s*\('
    )
    $channelEnableHits += @(
        Select-String -LiteralPath $path -Pattern 'TIM[23]->CCER\s*\|=.*CC3E'
    )
    $dynamicBrakeHits += @(
        Select-String -LiteralPath $path -Pattern 'dynamic[_ ]?brake' -CaseSensitive:$false
    )

    if (($compact -match 'HAL_GPIO_WritePin\([^;]*(GPIOB|MOTOR_HW_SD)[^;]*GPIO_PIN_SET') -or
        ($compact -match 'GPIOB->ODR')) {
        $sdHighHits += $path
    }
    foreach ($match in [regex]::Matches(
        $text,
        'GPIOB->BSRR\s*=\s*([^;]+);')) {
        if ($match.Groups[1].Value -notmatch
            'MOTOR_HW_SD_GPIO_PIN_MASK\s*<<\s*16U') {
            $sdHighHits += ($path + ': ' + $match.Value)
        }
    }

    foreach ($match in [regex]::Matches(
        $text,
        'TIM[23]->CCR3\s*=\s*([^;]+);')) {
        if ($match.Groups[1].Value.Trim() -notin @('0', '0U', '0UL')) {
            $nonzeroCcrHits += ($path + ': ' + $match.Value)
        }
    }
}

if ($sdHighHits.Count -ne 0) {
    throw ('SD1/SD2 high-write prohibition failed: ' + ($sdHighHits -join ', '))
}
if ($nonzeroCcrHits.Count -ne 0) {
    throw ('Nonzero motor CCR write prohibition failed: ' +
           ($nonzeroCcrHits -join ', '))
}
if ($pwmStartHits.Count -ne 0) {
    throw 'PWM-start prohibition failed.'
}
if ($channelEnableHits.Count -ne 0) {
    throw 'PWM-channel-enable prohibition failed.'
}
if ($blockingReceiveHits.Count -ne 0) {
    throw 'Blocking UART receive prohibition failed.'
}
if ($dynamicBrakeHits.Count -ne 0) {
    throw 'Dynamic-brake prohibition failed.'
}

Write-Output 'PHYSICAL_OUTPUT_COMPILE_LOCK=PASS'
Write-Output 'LOCKED_MOTOR_EXECUTOR_ONLY=PASS'
Write-Output 'MAIN_EARLY_FORCE_DISABLE_FIRST=PASS'
Write-Output 'NO_SD1_SD2_HIGH_WRITE=PASS'
Write-Output 'NO_NONZERO_MOTOR_CCR_WRITE=PASS'
Write-Output 'NO_PWM_CHANNEL_ENABLE=PASS'
Write-Output 'NO_PWM_START=PASS'
Write-Output 'NO_DYNAMIC_BRAKE=PASS'
Write-Output 'NO_BLOCKING_UART_RECEIVE_IN_ISR=PASS'
