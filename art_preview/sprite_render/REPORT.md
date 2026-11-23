# PaperChild 스프라이트 게임 연결 (텍스처 렌더러)

브랜치: `sprite-resize`. 플레이어를 빨간 큐브 대신 PaperChild 스프라이트로 그린다. 이동·점프·충돌 코드(PlayerController, RigidBody, ColliderManager)는 바꾸지 않았다.

## 바꾼 파일과 구조

| 파일 | 내용 |
|---|---|
| `dependencies/include/stb/stb_image.h` (v2.34), `stb_image_write.h` (v1.19), `dependencies/licenses/stb.txt` | 단일 헤더 라이브러리. GL 라이브러리처럼 저장소에 넣었고 `OpenGL.props`의 include 경로로 바로 쓰인다. 팀원 PC에 따로 설치할 것은 없다. |
| `Utility/StbImage.cpp` | stb 구현 코드를 만드는 단 한 곳. PNG만 켜고, 라이브러리 경고는 숨긴다. |
| `Data_Structure/Texture.h/.cpp` (새 클래스) | PNG → RGBA8 텍스처. 경로별로 한 번만 읽어 캐시한다. `GL_CLAMP_TO_EDGE`, `GL_LINEAR`. |
| `Shader/Sprite_vertex.glsl`, `Sprite_fragment.glsl` | `sampler2D` 텍스처 셰이더. `flipX`(UV 좌우 반전), `tint`, `alphaCutoff`(알파 테스트). 조명은 받지 않는다. |
| `Component/SpriteRenderer.h/.cpp` (새 컴포넌트) | UV가 있는 (0,0)~(1,1) 공용 쿼드를 그림 비율(3:4) 크기로 키워 그린다. JSON 속성: `texture`, `pivot`, `worldSize`/`height`, `flipX`, `billboardY`, `alphaCutoff`, `tint`. 다른 종이 소품에도 쓸 수 있다. |
| `Script/PlayerSpriteAnimator.h/.cpp` (새 스크립트) | 9장 로드, manifest의 pivot과 크기 적용, 플레이어 상태별 프레임 선택. |
| `Utility/DebugAutoplay.h/.cpp` (테스트용) | 환경 변수를 줄 때만 켜지는 숨긴 창 자동 재생·PNG 캡처. 평소 실행에는 영향이 없다. |
| `main.cpp` (CP949 그대로, 26줄 삽입만) | `Sprite` 셰이더 등록, DrawScene에서 SpriteRenderer가 있는 오브젝트를 불투명 물체 다음에 그리도록 `stable_partition`, 자동 재생 훅(설정 읽기·창 숨김, 고정 dt, 키 입력, 캡처, 프레임 끝). |
| `Game_Scene.json` | Player: `MeshRenderer3D`(빨간 큐브) 제거, `BoxCollider` size 1 → 2, `SpriteRenderer`·`PlayerSpriteAnimator` 추가 |
| `AllComponent.h`, `AllDataStructure.h`, `.vcxproj`, `.vcxproj.filters` | 새 파일 등록 |

새 C++ 파일은 UTF-8 BOM으로 저장했다. BOM이 없으면 MSVC가 한글 주석을 CP949로 읽어 C4819 경고를 낸다. 셰이더는 BOM 없이 저장했다.

### 충돌 크기를 그대로 둔 방법

`BoxCollider`는 같은 오브젝트에 있는 `MeshRenderer3D` 메시(큐브, -1~1)를 기준으로 상자를 잡고, 메시가 없으면 -0.5~0.5를 쓴다. 빨간 큐브를 빼면 상자가 절반으로 줄기 때문에 size를 2로 바꿨다. 월드 크기는 전과 같은 **0.7 × 1.2 × 0.5칸**이다. 충돌·이동 코드는 그대로다(`collider_overlay.png`, 로그의 "충돌 상자 높이 1.2칸").

## 텍스처·UV·투명 처리

- **위/아래 뒤집기는 한 번만**: `Texture::Load`에서 `stbi_set_flip_vertically_on_load(1)`. 그 뒤로 UV (0,0)은 그림 왼쪽 아래다. 쿼드는 위치 = UV이고 셰이더는 위/아래를 건드리지 않는다. manifest의 pivot(왼쪽 위 기준 픽셀)은 그림이 아니라 좌표만 `v = 1 - y/512`로 바꿨다.
- **좌우 반전**: 셰이더에서 `u → 1 - u`. 반전하면 그림 속 발 위치도 `1 - pivot.u`로 옮겨 가므로 쿼드 기준점도 같이 바꿔, 방향을 바꿔도 발이 제자리에 있다.
- **블렌딩**: `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA`(straight alpha). 스프라이트를 그릴 때만 켜고, 끝나면 원래 상태로 돌린다.
- **투명 픽셀이 뒤 물체를 가리지 않게** 두 가지를 같이 쓴다.
  1. 알파 테스트: 알파 0.1 미만은 `discard`해서 색도 깊이도 쓰지 않는다.
  2. 그리기 순서: 반투명 스프라이트는 불투명 물체를 다 그린 뒤에 그린다. 원래 정렬에서는 Player(z=0)가 같은 z의 Ground보다 먼저 그려져, 가장자리 반투명 픽셀이 깊이를 쓰면 그 자리에 땅이 안 그려질 수 있었다.
- **가장자리 검은 테두리 방지**: PNG의 투명 픽셀 RGB가 검정(0~5)이라 `GL_LINEAR` 보간 때 테두리가 어두워진다. 로드할 때 투명 텍셀의 RGB만 이웃 그림 색으로 4칸 채운다(alpha bleeding). 알파는 그대로다.
- **필터**: 지시대로 `GL_LINEAR`. 화면에서는 512px 그림이 2D 약 110px, 3D 약 85px로 줄어든다. `GL_LINEAR_MIPMAP_LINEAR`와 비교해 보니 밉맵 쪽이 특히 3D에서 흐려서 쓰지 않았다(`filter_compare_linear_vs_mipmap.png`).
- **컬링**: 스프라이트를 그릴 때만 `GL_CULL_FACE`를 끈다. 임시로 쿼드를 180° 돌려 뒷면을 카메라 쪽으로 향하게 했을 때, 컬링을 켜면 사라지고 지금 코드에서는 보였다(`backface_check.png`, 임시 코드는 되돌림).

## 크기와 기준점

- manifest `pivot_output_top_left` (187.55, 483.79) → pivot UV **(0.488, 0.055)**. 9장이 같은 캔버스와 같은 pivot을 쓴다.
- 기준점(anchor): 충돌 상자 바닥 가운데. 발이 충돌 상자 바닥에 닿는다.
- 크기: 발(pivot)부터 원본 캔버스 맨 위(`content_rectangle` 윗변, y=8)까지가 충돌 상자 높이 1.2칸이 되도록 맞췄다. 그림 전체는 **0.968 × 1.291칸**이고, 머리 꼭대기는 상자 위보다 조금 낮다. idle 그림 폭은 약 0.79칸이라 상자(0.7)보다 살짝 넓다(모자·팔).
- Transform의 크기(0.35, 0.6, 0.25)는 충돌 상자용이라 SpriteRenderer는 Transform의 회전·크기를 쓰지 않는다.

## 3D 표시 방식: 세로로 선 종이 인형 평면 (빌보드 아님)

스프라이트는 XY 평면에 세로로 선 종이 한 장이고, 앞면은 +z(2D 카메라 쪽)를 본다. 카메라를 따라 돌지 않는다.

1. **팝업북 콘셉트**: 책을 펼치면 종이 그림이 세워지는 것이 팝업북이다. 3D로 바꾸면 세상은 입체가 되지만 아이는 그대로 종이 인형으로 서 있어서 "평면 그림이 일어선 세계"가 드러난다.
2. **전환이 매끄럽다**: 2D에서는 평면이 시선과 수직이라 그림이 그대로 보인다. E 전환 중에는 카메라 회전과 투영 보간만 일어나고 스프라이트가 따로 돌지 않아서, 그림이 튀지 않고 서서히 원근이 붙는다(`09_transition_2d_to_3d_mid.png`).
3. **발이 땅에 붙는다**: 3D 카메라는 40° 내려다본다. 카메라를 완전히 바라보는 빌보드는 뒤로 기울어져 발이 땅에서 뜨거나 땅에 파묻히고, 앞뒤 물체와 깊이가 엉킨다. 세로 평면은 발이 충돌 상자 바닥에 붙고 계단·벽과 깊이 순서가 자연스럽다.
4. 지금 카메라는 좌우로 돌지 않으므로 세로축 빌보드와 결과가 같다. 나중에 카메라가 돌게 되면 JSON `"billboardY": true`로 바꿀 수 있다.

단점: 위에서 내려다보면 세로 길이가 약 cos 40° ≈ 0.77배로 짧아 보인다(`10_3d_idle.png`). 다른 상자들도 같은 비율로 줄어들므로 장면 안에서는 어색하지 않다.

## 상태별 프레임 (PlayerSpriteAnimator)

| 상태 | 조건 | 그림 |
|---|---|---|
| 가만히 | 땅 위, 수평 속도 ≤ 0.1 | idle |
| 걷기 | 땅 위, 수평 속도 > 0.1 (3D의 W/S 포함) | walk_01→02→03→04, 8fps 반복, 걷기 시작하면 01부터 |
| 점프 상승 | 공중, 위로 속도 > 0 | jump_rise |
| 점프 하강 | 공중, 속도 ≤ 0 (발판에서 떨어진 뒤 0.06초부터) | jump_fall |
| 방향 | 수평 속도 < -0.1이면 좌우 반전, > 0.1이면 원래대로, 멈추면 마지막 방향 유지 | UV 반전 |
| 시점 전환 중 | PlayerController가 멈춰 있는 동안 | 직전 그림 유지 |
| hurt / clear | 해당 게임 상태가 아직 없다 | 불러만 둔다. 연결 지점은 `SetOverrideState(Hurt/Clear)`, `ClearOverrideState()` (헤더 주석) |

값은 PlayerController·RigidBody에서 읽기만 한다. 컴포넌트 순서를 PlayerController·RigidBody 다음으로 두어 같은 프레임의 속도를 읽는다.

## 확인 결과

- **빌드**: VS 18 MSBuild `Default_Project.vcxproj /p:Configuration=Debug /p:Platform=x64`, 전체 재빌드 성공(exit 0). 작업 전 커밋(3c027f8)을 따로 내보내 같은 방식으로 재빌드한 결과와 비교했다. 경고를 파일·코드별로 묶으면 둘 다 33종으로 같다(C4819, C4244, C4267, LNK4098). 새 파일에서 나온 경고는 없다. 경고 줄 수만 187 → 207로 늘었는데, 새 cpp 파일이 BOM 없는 기존 헤더를 include해서 같은 C4819가 더 찍히기 때문이다. 중간 커밋들도 각각 따로 빌드해 확인했다.
- **실제 게임 화면 캡처**: 숨긴 창으로 고정 dt 1/60초 자동 재생을 돌렸다. `autoplay_sprite.txt`의 키 입력이 실제 InputManager → PlayerController로 들어간다. 실제 키보드 입력을 보내거나 화면에 창을 띄우지 않았다. 같은 스크립트를 두 번 돌렸을 때 위치와 프레임이 같았다.

| 파일 | 장면 |
|---|---|
| `01_2d_idle_right.png` | 2D 가만히 |
| `02`~`05_2d_walk_0N_left.png` | 2D 왼쪽 걷기 walk_01~04 (좌우 반전) |
| `06_2d_idle_left.png` | 2D 왼쪽을 본 채 정지. 뒤의 보라 상자가 투명 부분으로 보인다 |
| `07_2d_jump_rise.png`, `08_2d_jump_fall.png` | 2D 점프 상승·하강 |
| `09_transition_2d_to_3d_mid.png` | E 전환 중간 (viewT 0.53) |
| `10_3d_idle.png` | 3D 가만히 |
| `11`~`14_3d_walk_0N.png` | 3D 걷기 walk_01~04 (오른쪽 + 안쪽) |
| `15_3d_idle_front_of_blue_block.png` | 3D, 파란 상자 앞 |
| `16_3d_jump_rise.png`, `17_3d_jump_fall.png` | 3D 점프 상승·하강 |
| `18_2d_over_blue_block.png` | 3D → 2D로 돌아와 뒤쪽(z=-2.5) 파란 상자와 겹친 화면. 투명 픽셀이 뒤 물체를 가리지 않는다 |
| `contact_sheet_ingame.png` | 위 18장의 플레이어 주변을 모은 검토용 한 장 (프레임 번호·그림 이름 표시) |
| `collider_overlay.png` | 충돌 상자 디버그 선(임시로 `isDebug` 켬)과 스프라이트 겹침 |
| `backface_check.png` | 뒷면 컬링 확인 (임시 코드) |
| `filter_compare_linear_vs_mipmap.png` | GL_LINEAR / 밉맵 비교 (임시 코드) |
| `edge_overlap_step_2d.png` | 2D에서 계단에 붙어 섰을 때 모자 끝이 계단 앞면에 가려지는 모습 (아래 한계 9번) |
| `autoplay_log.txt` | 실행 로그. 그림이 바뀔 때마다 한 줄 |

로그 기준 걷기 그림은 f61 → 69 → 76 → 84 → 91에서 바뀐다(7~8프레임 간격 = 0.125초, 8fps).

### 다시 만들기

```powershell
# Debug x64 빌드 후
powershell -ExecutionPolicy Bypass -File art_preview/sprite_render/run_capture.ps1
```

자동 재생 환경 변수: `POPUP_HEADLESS`, `POPUP_FIXED_DT`, `POPUP_AUTOPLAY`, `POPUP_SHOT_DIR` (`Utility/DebugAutoplay.h` 주석 참고). 이 변수가 없으면 평소처럼 창을 띄우고 실시간으로 돈다.

## 남은 한계와 팀원과 정할 점

1. **역할 분담**: 제안서상 UV·텍스처·알파 셰이더는 팀원이 직접 맡기로 한 부분이다. 이번 코드는 AI가 임시로 작성했고 `docs/AI_활용_기록.md`에 적어 두었다. 담당 팀원이 다시 쓸지, 검토해서 설명할 수 있게 할지 정해야 한다.
2. **조명**: 스프라이트는 조명을 받지 않고 그림 색 그대로 나온다. 종이에 빛과 그림자를 줄지는 조명 셰이더 담당과 정한다.
3. **3D에서 짧아 보임**: 세로 평면이라 3D에서 약 0.77배로 짧아 보인다. 더 크게 보이게 하려면 3D에서만 살짝 뒤로 기울이거나 크기를 키울 수 있다.
4. **필터와 반짝임**: GL_LINEAR만 쓰면 4~6배로 줄어들어, 움직일 때 종이 질감이 반짝일 수 있다. 정지 화면은 밉맵보다 선명하다. 실제로 플레이해 보고 거슬리면 밉맵 + LOD bias나 이방성 필터를 검토한다(이때는 alpha bleeding을 4칸보다 넓게 해야 한다).
5. **반투명 정렬**: 스프라이트끼리는 월드 z 순서로만 정렬한다. 지금 카메라는 항상 -z를 보므로 맞지만, 종이 소품이 늘고 카메라가 돌면 카메라 거리 기준으로 정렬해야 한다.
6. **걷기 판정**: PlayerController가 정한 속도로 판단해서 벽에 막혀 있어도 걷는 그림이 나온다(흔한 처리지만 원하면 실제 이동량으로 바꿀 수 있다).
7. **hurt / clear**: 그림만 불러 두었다. 2단계에서 피격·클리어 상태가 생기면 `SetOverrideState`로 연결한다.
8. **DebugAutoplay**: main.cpp에 `autoplay` 호출을 몇 줄 넣었다(모두 `//[Test]` 주석이 붙어 있다). 환경 변수가 없으면 아무 일도 하지 않는다. 팀 코드에 두기 싫으면 빼도 게임에는 영향이 없다.
9. **2D에서 계단에 붙으면 그림 끝이 가려진다**: 종이 평면은 충돌 상자 깊이 가운데(z=0)에 있고, 계단 앞면은 z=+1에 있다. 그래서 계단에 붙어 서면 충돌 상자 밖으로 나온 모자·옷 끝(약 0.05칸)이 계단에 가려진다(`edge_overlap_step_2d.png`). 3D로 보면 맞는 가림이지만 2D에서는 어색할 수 있다. 2D에서는 항상 앞에 보이게 하려면 2D일 때만 종이를 카메라 쪽으로 당기는 방법이 있다. 직교 투영이라 z를 옮겨도 크기는 그대로고, 전환 중에는 viewT에 맞춰 제자리로 돌아오게 하면 된다. 어느 쪽으로 할지 정해야 한다.
