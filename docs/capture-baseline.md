# 전환 화면 기준 캡처

같은 입력을 같은 시간 간격으로 두 번 실행하고, 2D / 전환 중간 / 3D 이미지가 일치하는지 검사한다. Game Development Studio 설치 없이 기존 OpenGL 실행 파일로 동작한다.

## 실행

Visual Studio에서 `Debug | x64`를 빌드한 뒤 저장소 루트의 PowerShell에서 실행한다.

```powershell
& .\src\Default_Project\Tests\capture_baseline.ps1
```

Release 빌드를 검사하려면 `-Configuration Release`를 추가한다. 출력 폴더를 지정하려면 `-OutputDirectory`에 아직 존재하지 않는 경로를 전달한다. 기존 기준 이미지는 덮어쓰지 않는다.

## 촬영 조건

| 항목 | 값 |
|---|---|
| 레벨 | tutorial |
| 캐릭터 | 첫 번째 시안 |
| 화면 크기 | 1920 × 1080 |
| 시간 간격 | 프레임당 1/60초 |
| 난수 시드 | 0 |
| 조작 | 5초에 E 키로 3D 전환 |
| 촬영 | 4.5초: 2D / 5.25초: 전환 중간 / 5.75초: 3D |

실행 결과는 `src/Default_Project/Screenshots/baseline-날짜-시간/`에 저장한다.

- `reference/shot_1.bmp` ~ `shot_3.bmp`: 기준 화면
- `repeat/shot_1.bmp` ~ `shot_3.bmp`: 같은 조건의 반복 화면
- 각 폴더의 `capture_manifest.jsonl`: 촬영 시간, 전환 값, 플레이어 위치, 화면 크기, OpenGL 렌더러
- `comparison.json`: 실행 파일·입력 스크립트·이미지 SHA-256, Git 커밋과 변경 상태, 반복 일치 결과
- 각 폴더의 `stdout.log`, `stderr.log`: 실행 로그

이미지 수·촬영 시각·화면 상태·해상도를 검사한 뒤, 세 이미지의 SHA-256이 모두 같아야 PASS로 끝난다. 시간 초과, 비정상 종료, 이미지 누락, 반복 불일치는 실패로 처리한다. 스크립트가 설정한 환경 변수는 성공·실패와 관계없이 원래 값으로 복구한다.

## 구현 범위와 한계

`POPUP_FIXED_DT`를 지정한 실행에서만 고정 시간 간격과 난수 시드를 사용한다. 이동 계산, 입력 재생, 캡처 예약이 같은 게임 시간을 참조한다. 이 모드에서 입력 재생을 사용하면 실제 키보드·마우스 입력을 무시한다. 창 닫기는 가능하다.

일반 실행은 기존 실시간 모드를 유지한다. `POPUP_CAPTURE_METADATA=1`은 캡처 메타데이터를 추가로 기록한다.

## 검증 기록 (2026-11-11)

- NVIDIA GeForce RTX 4070 Laptop GPU, OpenGL 3.3.0 / 드라이버 621.01에서 실행했다.
- 4.5초·5.25초·5.75초에 각각 2D·전환 50%·3D 상태가 기록됐으며, 세 이미지 모두 두 실행의 SHA-256이 일치했다.
- 결과 폴더: `src/Default_Project/Screenshots/baseline-20261111-163424-468/`. `preview.png`는 세 장면을 위에서 아래로 모은 검토용 이미지다. 캡처 결과는 Git 추적 대상이 아니다.
- 고정 시간 모드를 사용하지 않는 기존 `clear_tutorial.txt` 재생도 튜토리얼을 통과해 `stage1`으로 진행했다.
- 최초 캡처 검증 당시 기본 Debug 링크는 `LNK1101: MSPDB140.DLL` 오류로 실패했다. 위 캡처 결과는 임시로 PDB 생성을 끈 실행 파일에서 얻었다.

## Debug 빌드 도구 수정

이 환경에서는 MSVC 14.44.35228의 링커와 PDB 관련 파일 버전이 일치했지만, `Hostx86/x64` 링커는 작은 테스트 프로그램의 PDB 생성에서도 30초 안에 완료되지 않았다. 같은 테스트를 `Hostx64/x64` 링커로 실행하면 EXE와 PDB가 생성됐고, 프로젝트 전체도 새 중간 파일 폴더에서 Debug 빌드에 성공했다.

`Default_Project.vcxproj`에서 x64 플랫폼의 기본 `PreferredToolArchitecture`를 `x64`로 지정했다. 명시적으로 전달한 도구 아키텍처는 존중하며, Win32 설정은 변경하지 않는다. 디버그 정보 생성과 증분 링크는 기존 설정을 유지한다.

수정 후 임시 빌드 옵션 없이 일반 `Debug | x64` 빌드와 재실행이 성공했다. 기본 출력 폴더의 EXE와 PDB 식별 정보도 일치했다. 이 실행 파일로 캡처 검사를 다시 실행해 세 이미지의 반복 일치를 확인했다. 결과는 `src/Default_Project/Screenshots/baseline-20261111-192448-963/`에 있다. 기존 C4819 및 LNK4098 경고는 이번 수정 범위에 포함하지 않았다.

Visual Studio 2022용 명령줄 빌드는 저장소 루트에서 다음과 같이 실행한다.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  '.\src\Default_Project\Default_Project.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /p:VisualStudioVersion=17.0
```

## 해석 시 주의점

이 검사는 동일 빌드·GPU·드라이버에서 화면을 반복 생성할 수 있는지 확인하는 검사다. 다른 GPU 사이의 픽셀 일치, 그림의 미적 완성도, 전체 게임 클리어, 실제 성능 향상을 증명하지 않는다. 고정 시간 모드는 실제 프레임 시간 측정에 사용하지 않는다. 기준 이미지를 교체하기 전에는 사람이 세 화면을 확인해야 한다.
