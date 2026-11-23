# PaperChild 스프라이트 렌더 확인 캡처를 다시 만든다.
#  - 먼저 Debug x64로 빌드해 둔다 (src/Default_Project/x64/Debug/Default_Project.exe)
#  - 창을 숨긴 채(POPUP_HEADLESS) 고정 dt로 autoplay_sprite.txt의 키 입력을 재생하고,
#    이 폴더에 PNG와 autoplay_log.txt를 저장한다. 실제 키보드 입력은 보내지 않는다.
# 실행: powershell -ExecutionPolicy Bypass -File art_preview/sprite_render/run_capture.ps1

$here = $PSScriptRoot
$root = Split-Path -Parent (Split-Path -Parent $here)
$proj = Join-Path $root 'src\Default_Project'
$exe = Join-Path $proj 'x64\Debug\Default_Project.exe'
if (-not (Test-Path $exe)) { Write-Error "빌드된 실행 파일이 없습니다: $exe"; exit 1 }

$env:POPUP_HEADLESS = '1'
$env:POPUP_FIXED_DT = '0.0166667'
$env:POPUP_AUTOPLAY = Join-Path $here 'autoplay_sprite.txt'
$env:POPUP_SHOT_DIR = $here

# 게임은 콘솔에 시스템 ANSI 코드 페이지(한국어 Windows는 CP949)로 출력한다.
# PowerShell 프로필이 콘솔 인코딩을 UTF-8로 바꿔 두었어도 한글이 깨지지 않게 실행하는 동안만 맞춘다.
$oldEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [Text.Encoding]::Default

Push-Location $proj	# 셰이더·에셋·씬 JSON을 프로젝트 폴더 기준 상대 경로로 읽는다
try {
	& $exe | Out-File -Encoding utf8 (Join-Path $here 'autoplay_log.txt')
	Write-Host "종료 코드: $LASTEXITCODE"
}
finally {
	Pop-Location
	[Console]::OutputEncoding = $oldEncoding
	Remove-Item Env:POPUP_HEADLESS, Env:POPUP_FIXED_DT, Env:POPUP_AUTOPLAY, Env:POPUP_SHOT_DIR -ErrorAction SilentlyContinue
}
