param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$OutputDirectory = '',
    [ValidateRange(1, 300)][int]$TimeoutSec = 60
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $project "x64\$Configuration\Default_Project.exe"
$replay = Join-Path $PSScriptRoot 'capture_transition.txt'
if (!(Test-Path -LiteralPath $exe)) { throw "Build x64/$Configuration first: $exe" }
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $project ('Screenshots\baseline-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw "Use a new output directory: $OutputDirectory" }
$null = New-Item -ItemType Directory -Path $OutputDirectory
$settings = @{
    POPUP_HEADLESS = '1'; POPUP_MUTE = '1'; POPUP_LOG = '1'
    POPUP_FIXED_DT = '0.016666666666666666'; POPUP_CHARACTER = '1'
    POPUP_LEVEL = 'Levels/tutorial.txt'; POPUP_REPLAY = $replay
    POPUP_CAPTURE = '4.5,5.25,5.75'; POPUP_CAPTURE_METADATA = '1'
    POPUP_CAPTURE_DIR = ''
}
$saved = @{}
foreach ($name in $settings.Keys) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$runs = @()
try {
    foreach ($name in $settings.Keys) {
        [Environment]::SetEnvironmentVariable($name, $settings[$name], 'Process')
    }
    foreach ($run in @('reference', 'repeat')) {
        $dir = Join-Path $OutputDirectory $run
        $null = New-Item -ItemType Directory -Path $dir
        [Environment]::SetEnvironmentVariable('POPUP_CAPTURE_DIR', $dir, 'Process')
        $process = Start-Process -FilePath $exe -WorkingDirectory $project -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
        if (!$process.WaitForExit($TimeoutSec * 1000)) {
            Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
            throw "Capture timed out: $run"
        }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Game exited with code $($process.ExitCode). See $dir" }
        $manifest = Join-Path $dir 'capture_manifest.jsonl'
        if (!(Test-Path -LiteralPath $manifest)) { throw "No capture manifest: $dir" }
        $shots = @(Get-Content -LiteralPath $manifest | ForEach-Object { $_ | ConvertFrom-Json })
        if ($shots.Count -ne 3) { throw "Expected 3 captures, got $($shots.Count): $dir" }
        if ($shots[0].state -ne 'Playing' -or $shots[0].mode -ne '2D' -or [Math]::Abs($shots[0].viewT) -gt 0.001) {
            throw 'First capture is not a stable 2D view.'
        }
        if ($shots[1].state -ne 'Transition' -or $shots[1].viewT -le 0 -or $shots[1].viewT -ge 1) {
            throw 'Second capture is not a transition frame.'
        }
        if ($shots[2].state -ne 'Playing' -or $shots[2].mode -ne '3D' -or [Math]::Abs($shots[2].viewT - 1) -gt 0.001) {
            throw 'Third capture is not a stable 3D view.'
        }
        $expectedTimes = @(4.5, 5.25, 5.75)
        $hashes = @()
        for ($i = 0; $i -lt 3; $i++) {
            if ([Math]::Abs($shots[$i].time - $expectedTimes[$i]) -gt 1e-6) { throw 'Capture time mismatch.' }
            if ($shots[$i].width -ne 1920 -or $shots[$i].height -ne 1080) { throw 'Capture resolution mismatch.' }
            $hashes += (Get-FileHash -LiteralPath (Join-Path $dir $shots[$i].file) -Algorithm SHA256).Hash
        }
        $runs += [PSCustomObject]@{ name = $run; shots = $shots; hashes = $hashes }
    }
} finally {
    foreach ($name in $saved.Keys) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}
$identical = $true
for ($i = 0; $i -lt 3; $i++) {
    if ($runs[0].hashes[$i] -ne $runs[1].hashes[$i]) { $identical = $false }
}
$report = [ordered]@{
    schemaVersion = 1
    scenario = 'tutorial-stationary-2d-transition-3d'
    configuration = $Configuration
    fixedDt = 1.0 / 60.0
    seed = 0
    revision = (git -C $project rev-parse HEAD)
    workingTree = @(git -C $project status --porcelain)
    executableSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    replaySha256 = (Get-FileHash -LiteralPath $replay -Algorithm SHA256).Hash
    identical = $identical
    runs = $runs
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'comparison.json') -Encoding UTF8
if (!$identical) { throw "Repeated captures differ. See $OutputDirectory\comparison.json" }
Write-Output "PASS: all three repeated captures are byte-identical. Output: $OutputDirectory"
