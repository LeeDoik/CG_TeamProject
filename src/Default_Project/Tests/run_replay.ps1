# Auto-play test: runs the game in a hidden window with a replay script and prints game events.
# Usage (from the project folder, after building Debug|x64):
#   powershell -ExecutionPolicy Bypass -File Tests\run_replay.ps1 -Replay Tests\clear_all.txt
#   powershell -ExecutionPolicy Bypass -File Tests\run_replay.ps1 -Replay Tests\clear_stage1.txt -Level Levels\stage1.txt
param([Parameter(Mandatory = $true)][string]$Replay, [string]$Level = "", [int]$TimeoutSec = 90)
$proj = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $proj "x64\Debug\Default_Project.exe"
$log = Join-Path $env:TEMP "popup_replay.log"
$env:POPUP_HEADLESS = "1"
$env:POPUP_LOG = "1"
$env:POPUP_REPLAY = (Resolve-Path (Join-Path $proj $Replay)).Path
if ($Level -ne "") { $env:POPUP_LEVEL = $Level }
$p = Start-Process -FilePath $exe -WorkingDirectory $proj -PassThru -WindowStyle Hidden -RedirectStandardOutput $log
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Output "TIMEOUT" }
Remove-Item Env:POPUP_HEADLESS, Env:POPUP_LOG, Env:POPUP_REPLAY, Env:POPUP_LEVEL -ErrorAction SilentlyContinue
Get-Content $log -Encoding Default | Where-Object { $_ -notmatch "^\[\S+\] t=" }
Write-Output ("last state: " + (Get-Content $log -Encoding Default | Select-Object -Last 1))
