param([ValidateSet(30, 60)][int]$FrameRate = 60)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $project 'x64\Debug\Default_Project.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Build Debug/x64 first.' }
$dir = Join-Path $project ('Screenshots\occlusion-' + $FrameRate + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $dir
$culture = [Globalization.CultureInfo]::InvariantCulture
$times = @(4.5, 5.2, (5.0 + [Math]::Ceiling(0.25 * $FrameRate) / $FrameRate), 5.4, 5.8, 7.2, 7.3, 7.8, 8.1)
$settings = @{
    POPUP_HEADLESS = '1'; POPUP_MUTE = '1'; POPUP_LOG = '1'; POPUP_CHARACTER = '1'
    POPUP_LEVEL = 'Tests/occlusion_level.txt'
    POPUP_REPLAY = (Join-Path $PSScriptRoot 'occlusion_replay.txt')
    POPUP_FIXED_DT = (1.0 / $FrameRate).ToString('R', $culture)
    POPUP_CAPTURE = (($times | ForEach-Object { $_.ToString('R', $culture) }) -join ',')
    POPUP_CAPTURE_METADATA = '1'; POPUP_CAPTURE_DIR = $dir
}
$saved = @{}
foreach ($name in $settings.Keys) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    foreach ($name in $settings.Keys) { [Environment]::SetEnvironmentVariable($name, $settings[$name], 'Process') }
    $process = Start-Process -FilePath $exe -WorkingDirectory $project -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    if (!$process.WaitForExit(90000)) {
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        throw "Occlusion capture timed out: $dir"
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Game exited with code $($process.ExitCode): $dir" }
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
$shots = @(Get-Content -LiteralPath (Join-Path $dir 'capture_manifest.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
if ($shots.Count -ne $times.Count) { throw 'Missing occlusion captures.' }
for ($i = 0; $i -lt $shots.Count; $i++) {
    $shot = $shots[$i]
    if (!(Test-Path -LiteralPath (Join-Path $dir $shot.file))) { throw 'Missing image.' }
    if ([Math]::Abs($shot.time - $times[$i]) -gt 1e-6) { throw 'Capture time mismatch.' }
    if ($shot.solidAlpha.Count -ne 3) { throw 'Build the instrumented game and use the regression fixture.' }
    if ($shot.solidAlpha[0] -lt 0.999 -or $shot.solidAlpha[2] -lt 0.999) { throw 'An unrelated solid faded.' }
    if ($shot.viewT -ge 0.5 -and ($shot.playerOnTop -or $shot.solidAlpha[1] -gt 0.301)) {
        throw "Occluding roof is too opaque during depth rendering: $($shot.file)"
    }
}
if ($shots[0].solidAlpha[1] -lt 0.999 -or !$shots[0].playerOnTop) { throw 'Initial 2D rendering changed.' }
if ($shots[1].solidAlpha[1] -ge 0.8) { throw 'The blocker did not fade before the depth handoff.' }
if ($shots[4].mode -ne '3D' -or $shots[7].mode -ne '2D') { throw 'Round-trip transition failed.' }
if ($shots[8].solidAlpha[1] -lt 0.98) { throw 'The roof did not become opaque again.' }
$report = [ordered]@{
    passed = $true; frameRate = $FrameRate
    executableSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    fixtureSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'occlusion_level.txt')).Hash
    replaySha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'occlusion_replay.txt')).Hash
    shots = $shots
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $dir 'verification.json') -Encoding UTF8
Write-Output "PASS: $FrameRate Hz, blocker handoff and opacity restoration. Output: $dir"
