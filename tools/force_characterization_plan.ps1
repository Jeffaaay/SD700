# No serial connection here. Shared by the production runner and host tests.
$characterizationFields=@('target_N','assist_percent','continuous_percent') | ForEach-Object { [pscustomobject]@{name=$_} }
function New-CharacterizationPlan([double]$TargetForceN,[double]$AssistPercent,[double]$ContinuousPercent) {
    foreach ($v in @($TargetForceN,$AssistPercent,$ContinuousPercent)) {
        if ([double]::IsNaN($v) -or [double]::IsInfinity($v)) { throw 'Non-finite runtime plan; ZERO START' }
    }
    if ($TargetForceN -lt 1 -or $TargetForceN -gt 3000 -or [Math]::Floor($TargetForceN) -ne $TargetForceN -or
        $AssistPercent -lt 0 -or $AssistPercent -gt 40 -or $ContinuousPercent -lt 0 -or $ContinuousPercent -gt 10) {
        throw 'Runtime bounds: integer TargetForceN 1..3000, AssistPercent 0..40, ContinuousPercent 0..10; ZERO START'
    }
    return [pscustomobject]@{target_N=[single]$TargetForceN;assist_percent=[single]$AssistPercent;continuous_percent=[single]$ContinuousPercent}
}
function Convert-CharacterizationPercent([single]$Percent) {
    [single]$scaled=$Percent*[single]240
    return [int][Math]::Floor([single]($scaled+[single]0.5))
}
function Submit-CharacterizationPlan([scriptblock]$Exchange,$Plan) {
    $expected=New-CharacterizationPlan $Plan.target_N $Plan.assist_percent $Plan.continuous_percent
    $before=@(Read-ForceWords $Exchange 3 0x540 12)
    [uint32]$version=[uint64]$before[8]*65536+$before[9]
    if ($version -eq [uint32]::MaxValue) { throw 'Runtime plan version exhausted; ZERO START' }
    $digest=Get-ForceDigest $expected $characterizationFields
    Write-ForceWord $Exchange 0x500 0xB501
    $words=@()
    foreach ($p in $characterizationFields) {
        [uint32]$bits=[BitConverter]::ToUInt32([BitConverter]::GetBytes([single]$expected.($p.name)),0)
        $words+=@(($bits -shr 16),($bits -band 65535))
    }
    $words+=@(($version -shr 16),($version -band 65535),($digest -shr 16),($digest -band 65535))
    for ($i=0;$i -lt $words.Count;$i++) { Write-ForceWord $Exchange (0x510+$i) $words[$i] }
    Write-ForceWord $Exchange 0x501 0xC501
    $actual=@(Read-ForceWords $Exchange 3 0x540 12)
    for ($i=0;$i -lt 6;$i++) { if ($actual[$i] -ne $words[$i]) { throw 'Complete runtime plan readback mismatch; ZERO START' } }
    [uint32]$actualVersion=[uint64]$actual[8]*65536+$actual[9]
    [uint32]$actualDigest=[uint64]$actual[10]*65536+$actual[11]
    if ($actualVersion -ne $version+1 -or $actualDigest -ne $digest -or
        $actual[6] -ne (Convert-CharacterizationPercent $expected.assist_percent) -or
        $actual[7] -ne (Convert-CharacterizationPercent $expected.continuous_percent)) {
        throw 'Runtime plan digest/version/mapping mismatch; ZERO START'
    }
    return [pscustomobject]@{target_N=$expected.target_N;assist_percent=$expected.assist_percent;
        continuous_percent=$expected.continuous_percent;assist_command=$actual[6];continuous_cap=$actual[7];
        version=$actualVersion;digest=$actualDigest}
}
