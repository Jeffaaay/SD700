[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Port,
    [Parameter(Mandatory=$true)][double]$TargetForceN,
    [string]$OutputCsv
)
$ErrorActionPreference='Stop'
$options=@{Port=$Port;TargetForceN=$TargetForceN;OutputCsv=$OutputCsv}
. "$PSScriptRoot/capture_force_servo.ps1" -LibraryOnly -BuildToTarget
foreach ($item in $options.GetEnumerator()) { Set-Variable -Name $item.Key -Value $item.Value }
$plan=New-CharacterizationPlan $TargetForceN 0 0
$root=(Resolve-Path "$PSScriptRoot/..").Path
$firmware=Join-Path $root 'output/BuildToTarget1/firmware/SD700_ForceServo1_BuildToTarget1_RealBench_Release.hex'
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
    $OutputCsv=Join-Path $root ('output/field/build-to-target1-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.csv')
}
foreach ($p in @($OutputCsv,[IO.Path]::ChangeExtension($OutputCsv,'.report.txt'),[IO.Path]::ChangeExtension($OutputCsv,'.metadata.json'))) {
    if (Test-Path -LiteralPath $p) { throw "Output exists: $p" }
}
$confirmStart={param($p)
    Write-Host @"
=== SD700 BuildToTarget1 SUPERVISED EXPERIMENT ===
Target:              $($p.target_N) N (only writable control)
Initial force:       0 N allowed; contact10 N; low target takes priority
APPROACH:            command5000 (~20.83% PWM), normal99 / hard100 ms segments
BUILD:               command3000 + bounded boost0..4000; ceiling7000 (~29.17%)
BUILD duration:      hard floor(30000/command) ms (4..10); normal hard-1 ms
TAPER (error<=50 N): command800..3000; hard10 ms; no boost
Fine (error<=3 N):   command400..1000; hard5 ms; no boost
Every segment:      true OFF>=30 ms + a fresh post-segment sensor frame
Exposure ceiling:   APPROACH8 s; all segments12 s (charged hard time, no refunds)
Full reset cooling: 108 s uninterrupted OFF; also required after boot
No-force-response:  5 s active BUILD/TAPER without >=2 N net new-high progress
At ANY target:      immediate bridge OFF, then monitor decay until manual STOP
Overall run limit:  NONE; no active HOLD, preload or automatic release/repress
Kp/Ki/Kd:           10 / 0 / 0 (LOCKED; PID diagnostic, bounded build profile drives)
PSU setting:        0.5 A unchanged; no measured winding current or thermal rating
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
        -OutputCsv $OutputCsv -ActualHash $actualHash -MaximumSeconds 0 -Target ([int]$TargetForceN) -TargetN -ProfileId 6 `
        -CharacterizationPlan $plan -ConfirmStart $confirmStart -CurrentLimitSetting $psuRecord `
        -InitialGap $gap -FieldNotes $notes -ConfirmedFirmwareSha256 $actualHash -RepositoryCommit $repositoryCommit
} finally {
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
