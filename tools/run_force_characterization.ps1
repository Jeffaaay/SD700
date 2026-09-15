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
$firmware=Join-Path $root 'output/BuildToTarget2_RiseDiagnostic1/firmware/SD700_ForceServo1_BuildToTarget2_RiseDiagnostic1_RealBench_Release.hex'
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
    $OutputCsv=Join-Path $root ('output/field/build-to-target2-start-anchor-fix1-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)+'.csv')
}
foreach ($p in @($OutputCsv,[IO.Path]::ChangeExtension($OutputCsv,'.report.txt'),[IO.Path]::ChangeExtension($OutputCsv,'.metadata.json'))) {
    if (Test-Path -LiteralPath $p) { throw "Output exists: $p" }
}
$confirmStart={param($p)
    Write-Host @"
=== SD700 BuildToTarget2_RiseDiagnostic1 SUPERVISED EXPERIMENT ===
Target:              $($p.target_N) N (only writable control)
Initial force:       0 N allowed; contact10 N; already reached target takes priority
APPROACH:            base5000 + coarse0..3500 command (~20.83..35.42% PWM)
COARSE adaptation:  >=200 ms fresh OFF check; <1 N movement -> +500; otherwise reset
APPROACH timing:    normal99 / hard100 ms segments, never unbounded continuous
MICRO:              error-scaled base800..3000 + boost0..2000; maximum5000
FINE (error<=3 N):  base400..1000 + boost0..1000; maximum2000
Pulse adaptation:  two <1 N movements -> +300; abs(movement)>=1 N resets boost
Pulse timing:      normal10 ms (boost independent); TIM5 hard11 ms
Post-pulse rise:   diagnostic only; Target OFF and absolute overforce stay active
MICRO/FINE gap:     forward preload initially300, range200..600; >2 N droop +100
Preload adaptation: fresh post-forward pulse only, cap600, retained across START
Next pulse:         cooldown>=30 ms + a fresh post-segment sensor frame
APPROACH gap:       true OFF; no forward preload after a reverse request
Exposure ceiling:   APPROACH8 s; pulse hard reservations + preload time <=12 s
Approach amplitude: 40,000,000 command-ms maximum (8 s at5000); not measured heat
Full reset cooling: 108 s uninterrupted OFF; also required after boot
No-force-response:  5 s active BUILD/TAPER without >=2 N net new-high progress
At ANY target:      immediate bridge OFF, then monitor decay until manual STOP
Overall run limit:  NONE; at target no active HOLD/preload or automatic release/repress
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
        -OutputCsv $OutputCsv -ActualHash $actualHash -MaximumSeconds 0 -Target ([int]$TargetForceN) -TargetN -ProfileId 7 `
        -CharacterizationPlan $plan -ConfirmStart $confirmStart -CurrentLimitSetting $psuRecord `
        -InitialGap $gap -FieldNotes $notes -ConfirmedFirmwareSha256 $actualHash -RepositoryCommit $repositoryCommit
} finally {
    if ($null -ne $serial) { try { $serial.Close() } finally { $serial.Dispose() } }
}
