# 시점 전환 중 캐릭터 가림 수정

## 원인과 변경

2D→3D 전환이 50%에 도달하면 캐릭터를 맨 앞에 그리던 처리가 꺼진다. 기존에는 같은 시점부터 장애물을 천천히 투명하게 만들어, 캐릭터 머리가 잠시 가려졌다.

- 전환 앞부분에서 도착할 3D 카메라와 캐릭터 사이의 장애물을 미리 흐리게 한다.
- 깊이 판정을 사용하는 동안 실제로 캐릭터를 가리는 장애물은 불투명도가 0.3을 넘지 않도록 한다.
- 가림이 해소되면 시간 간격을 반영한 지수 보간으로 불투명도를 복구한다. 위에 붙은 잔디도 같은 값을 따른다.
- 캡처 메타데이터에 캐릭터 우선 그리기 여부와 발판별 불투명도를 기록한다.

충돌 판정과 전환 시간은 변경하지 않았다.

## 재현과 검사

`Tests/occlusion_level.txt`의 머리 위 장애물 아래에서 2D→3D→2D 왕복 전환을 재생한다. 프로젝트 루트에서 Debug/x64 빌드 후 실행한다.

```powershell
& .\src\Default_Project\Tests\check_occlusion.ps1 -FrameRate 60
& .\src\Default_Project\Tests\check_occlusion.ps1 -FrameRate 30
```

두 검사 모두 전환 직전의 사전 처리, 깊이 판정 첫 프레임의 불투명도, 2D 복귀 후 불투명도 복구, 관련 없는 발판의 불투명도 유지를 통과했다. 각 검사에서 9장씩 캡처했으며 결과와 실행 파일 해시는 해당 폴더의 `verification.json`에 기록된다.

- 60Hz: `src/Default_Project/Screenshots/occlusion-60-20261111-225308-994/`
- 30Hz: `src/Default_Project/Screenshots/occlusion-30-20261111-225324-252/`
- 수정 전: `src/Default_Project/Screenshots/occlusion-before-20261111/`

60Hz 폴더의 `handoff-comparison.png`에서 전환 50% 시점의 머리 가림이 완화된 것을 직접 확인했다. 자동 검사는 렌더링 상태와 불투명도 검증이며, 모든 장면의 시각적 가독성을 보장하는 픽셀 판정은 아니다.

Visual Studio 2022의 Debug/x64 빌드도 성공했다. 고정 시간 없이 기존 `clear_tutorial.txt`를 재생해 튜토리얼을 완료하고 HP 3으로 첫 스테이지에 진입하는 것을 확인했다. 기존 C4819 및 LNK4098 경고는 남아 있다.
