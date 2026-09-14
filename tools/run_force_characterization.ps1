[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Port,
    [Parameter(Mandatory=$true)][double]$TargetForceN,
    [Parameter(Mandatory=$true)][double]$AssistPercent,
    [Parameter(Mandatory=$true)][double]$ContinuousPercent,
    [string]$OutputCsv
)
$ErrorActionPreference='Stop'
$options=@{Port=$Port;TargetForceN=$TargetForceN;AssistPercent=$AssistPercent;ContinuousPercent=$ContinuousPercent;OutputCsv=$OutputCsv}
. "$PSScriptRoot/capture_force_servo.ps1" -LibraryOnly -Characterization
foreach ($item in $options.GetEnumerator()) { Set-Variable -Name $item.Key -Value $item.Value }
$plan=New-CharacterizationPlan $TargetForceN $AssistPercent $ContinuousPercent
$root=(Resolve-Path "$PSScriptRoot/..").Path
$firmware=Join-Path $root 'output/StaticForceRuntimeCharacterization2/firmware/SD700_ForceServo1_StaticForceRuntimeCharacterization2_RealBench_Release.hex'
& python "$PSScriptRoot/verify_force_servo_firmware.py"
if ($LASTEXITCODE -ne 0) { throw 'Strict repository HEX/ELF verification failed; no serial connection' }
$actualHash=(Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash
$repositoryCommit=(& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Repository commit identity unavailable' }
if ([Console]::IsInputRedirected) { throw 'Interactive supervision required; batch START is unavailable' }
Write-Host "FLASHED HEX must match $actualHash"
Write-Host 'Single supervised experiment. Existing E-stop, permitted travel/load, guard and 0.5 A PSU setting required.'
$confirmed=Read-Host 'Confirm this HEX is flashed, PSU remains 0.5 A, and supervision/clearance/E-stop are ready: type YES'
if ($confirmed -cne 'YES') { throw 'Confirmation missing; ZERO START' }
$psuEntered=Read-Host 'Record actual PSU current-limit setting in A (must remain 0.5)'
[double]$psuAmps=0
if (-not [double]::TryParse($psuEntered,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$psuAmps) -or $psuAmps -ne 0.5) {
    throw 'This experiment requires the unchanged 0.5 A PSU setting; ZERO START'
}
$psuRecord="$psuEntered A PSU setting; operator-entered and confirmed unchanged; winding current NOT MEASURED"
$gap=Read-Host 'Record actual initial gap (include units)'
if ([string]::IsNullOrWhiteSpace($gap)) { throw 'Initial gap required; ZERO START' }
$notes=Read-Host 'Brief initial field conditions (optional)'
if (-not $OutputCsv) {
    $OutputCsv=Join-Path $root ('output/field/static-force-runtime-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.csv')
}
foreach ($p in @($OutputCsv,[IO.Path]::ChangeExtension($OutputCsv,'.report.txt'),[IO.Path]::ChangeExtension($OutputCsv,'.metadata.json'))) {
    if (Test-Path -LiteralPath $p) { throw "Output exists: $p" }
}
$confirmStart={param($p)
    Write-Host @"
=== SD700 SUPERVISED FORCE CHARACTERIZATION ===
Target Force:        $($p.target_N) N
Assist output:       $($p.assist_percent) %
Assist command:      $($p.assist_command)
Continuous ceiling: $($p.continuous_percent) %
Continuous command: $($p.continuous_cap)
Assist rise:         1 ms
Normal handoff:      2 ms
Hard assist cutoff:  4 ms
Overall run limit:   NONE; below3000 N stays in active HOLD until manual STOP
Target3000 N:        first valid reach => immediate OFF, no HOLD
Initial force:       0 N allowed; normal continuous approach, contact at20 N gates one assist
Re-arm lockout:      5000 ms (NOT validated thermal cooling time)
Kp/Ki/Kd:            10 / 0 / 0 (LOCKED)
PSU current setting: 0.5 A (operator-confirmed, unchanged)
Use S/Escape or external STOP immediately for abnormal motion/noise/current/heat.
"@
    $answer=Read-Host 'Press ENTER for ONE supervised START; any text cancels'
    return $answer -ceq ''
}
$serial=$null
try {
    $serial=New-Object IO.Ports.SerialPort $Port,115200,None,8,One
    $serial.Handshake=[IO.Ports.Handshake]::None; $serial.ReadTimeout=100; $serial.WriteTimeout=100
    $serial.DtrEnable=$false; $serial.RtsEnable=$false; $serial.Open()
    $transport={param([byte[]]$Request,[int]$TimeoutMs) Invoke-SerialExchange -Serial $serial -Request $Request -TimeoutMs $TimeoutMs}
    Invoke-ForceCapture -TransportExchange $transport -Watch ([Diagnostics.Stopwatch]::StartNew()) `
        -SleepMilliseconds {param($ms) Start-Sleep -Milliseconds $ms} -Mode SingleStart `
        -StopRequested { if ([Console]::KeyAvailable) { return [Console]::ReadKey($true).Key -in @([ConsoleKey]::S,[ConsoleKey]::Escape) }; return $false } `
        -OutputCsv $OutputCsv -ActualHash $actualHash -MaximumSeconds 0 -Target ([int]$TargetForceN) -TargetN -ProfileId 5 `
        -CharacterizationPlan $plan -ConfirmStart $confirmStart -CurrentLimitSetting $psuRecord `
        -InitialGap $gap -FieldNotes $notes -ConfirmedFirmwareSha256 $actualHash -RepositoryCommit $repositoryCommit
} finally {
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
