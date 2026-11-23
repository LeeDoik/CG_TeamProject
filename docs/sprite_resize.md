# PaperChild 게임용 리사이즈

작업 브랜치: `sprite-resize` (이후 LeeDoik_2에 포함). 이 단계에서는 게임 로직, 셰이더, 프로젝트 설정을 바꾸지 않고 그림 파일만 준비했다. 준비한 그림을 게임에 연결한 내용은 [sprite_renderer.md](sprite_renderer.md)에 있다.

## 코드 조사 (리사이즈 당시)

- `Data_Structure/Vertex.h`: position/color/normal만 있고 UV가 없다.
- `Utility/ObjImporter.cpp`: OBJ의 위치와 법선만 읽고 이미지/UV를 로드하지 않는다.
- `Data_Structure/Material.cpp`, `Shader/Standard_fragment.glsl`: uniform 색상과 조명을 사용하며 텍스처 샘플러가 없다.
- `main.cpp`: OpenGL 3.3 core, 깊이 테스트/컬링 활성화. 이미지 디코더, 텍스처 업로드, 알파 블렌딩 설정이 없다.
- `Game_Scene.json`: Player는 Red 재질의 Cube, scale [0.35, 0.6, 0.25]. 스프라이트 쿼드 크기, 기대 이미지 경로, 프레임 전환 기능이 정의되지 않았다.

그래서 이 단계의 결과는 연결용 에셋 준비까지였다. 텍스처 로딩과 스프라이트 렌더러는 다음 단계([sprite_renderer.md](sprite_renderer.md))에서 추가했다.

## 출력 규격과 처리

9장 모두 **384×512, RGBA 8비트, straight-alpha PNG**. 원본 1086×1448 전체 캔버스를 비율 그대로 372×496으로 축소하고 좌우 6px, 상하 8px 투명 여백을 추가했다. 포즈별 자동 자르기/재중앙정렬은 하지 않았다. 공통 좌표를 유지하여 점프 포즈가 바닥으로 강제 정렬되거나 걷기 중 기준점이 달라지지 않는다.

원본은 픽셀아트가 아닌 종이 질감 그림이므로 premultiplied-alpha Lanczos 축소 후 straight alpha로 저장했다. 이는 경계의 색 번짐을 줄이며 nearest-neighbor보다 질감 보존에 적합하다. 원본 프리뷰에 표시된 기준점 (530,1389)을 동일 변환한 좌표는 manifest에 기록했다. 원본 idle의 아주 옅은 가장자리 픽셀도 임의로 제거하지 않았다.

384×512는 원본의 3:4 비율을 유지하며, 대상 OpenGL 3.3에서 POT 강제 규격을 만들 필요가 없다. 9장 RGBA 비압축 메모리는 약 6.75MiB(원본 약 54MiB), mipmap 제외. 런타임 지원이 없는 atlas를 임의로 추가하지 않고 독립 프레임을 제공한다. **새 AI 생성 0장, 기존 원본 축소 및 패딩 9장**이다.

원본 루트: 로컬 작업 폴더의 `src/Default_Project/Assets/Characters/PaperChild/` (1086×1448 원본 9장). 저장소에는 원본 중 `paper_child_idle_right.png`만 main 커밋 db4a809로 올라갔고, 그 파일의 SHA-256(`964fbea1…`)은 `manifest.json`에 적힌 idle 원본 해시와 같다. 게임에는 아래 384×512 출력본을 쓴다.

출력 루트: `src/Default_Project/Assets/Characters/PaperChild/`

| 원본 → 출력 (이름 동일) | 원본 | 출력 |
|---|---|---|
| paper_child_idle_right.png | 1086×1448 | 384×512 |
| paper_child_walk_01_right.png | 1086×1448 | 384×512 |
| paper_child_walk_02_right.png | 1086×1448 | 384×512 |
| paper_child_walk_03_right.png | 1086×1448 | 384×512 |
| paper_child_walk_04_right.png | 1086×1448 | 384×512 |
| paper_child_jump_rise_right.png | 1086×1448 | 384×512 |
| paper_child_jump_fall_right.png | 1086×1448 | 384×512 |
| paper_child_hurt_right.png | 1086×1448 | 384×512 |
| paper_child_clear_right.png | 1086×1448 | 384×512 |

원본과 함께 받은 참고 프리뷰 `paper_child_pose_lineup.png`(1902×356)와 `paper_child_walk_8fps.gif`(360×480)는 게임 텍스처로 쓰지 않았고 저장소에도 넣지 않았다. 원본들은 수정하지 않았다.

## 확인 및 재현

- `tools/resize_paper_child.py`: Pillow 사용, 출력 크기/모드/알파/여백 검사 및 원본 SHA-256 불변 확인. 9장 통과.
- `manifest.json`: 원본/출력 SHA-256, alpha bbox, 공통 pivot, 변환 규격.
- 검토용 이미지: 스크립트가 체크무늬 배경 모음(`contact_sheet.png`, 1152×1632)과 걷기 애니메이션(`walk_8fps.webp`, 384×512, 4프레임, 프레임당 125ms)을 `art_preview/sprite_resize/`에 만든다. 이 폴더는 로컬 검토용이라 `.gitignore`로 제외했다. 모음 이미지로 모자/얼굴/옷 색감과 포즈가 유지되고 새 잘림이 없는 것을 확인했다. 걷기 애니메이션은 게임 파일이 아니며 실제 게임 재생 검증으로 보지 않는다.
- **Debug x64 빌드 성공(exit 0)**: VS 18 Community MSBuild로 `src/Default_Project/Default_Project.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo` 실행. C4819 인코딩, C4244/C4267 형 변환, LNK4098 런타임 라이브러리 경고가 있었다. 빌드를 위해 소스를 바꾸지 않았다.
- 이 단계에서는 게임에 텍스처 렌더링 경로가 없어 플레이어는 여전히 빨간 큐브였다. 게임 안에서 스프라이트가 보이는 확인은 [sprite_renderer.md](sprite_renderer.md)에 있다.

재현 명령(Pillow 필요, `--source`는 1086×1448 원본 9장이 있는 폴더):

```powershell
python -X utf8 tools/resize_paper_child.py --source '<원본 PaperChild 폴더>'
```

## 후속 연결 (sprite_renderer.md에서 구현)

리사이즈 당시 제안한 연결 방법이다. PNG 디코더와 RGBA 텍스처 업로드, UV가 있는 3:4 쿼드, sampler2D 셰이더를 추가해야 한다. 이미지 좌표의 위/아래와 OpenGL UV 방향을 한 번만 뒤집고, `GL_CLAMP_TO_EDGE`, `GL_LINEAR`, straight-alpha용 `SRC_ALPHA/ONE_MINUS_SRC_ALPHA`를 적용한다. 투명 픽셀의 depth write 처리와 3D 뷰의 빌보드/컬링 정책도 정해야 한다. manifest의 공통 pivot을 쿼드 원점으로 사용하고 월드 크기는 플레이어 충돌체에 맞춰 별도로 정한다. 걷기 프레임은 01→02→03→04, 8fps를 초기값으로 권장하며 왼쪽은 UV 반전으로 처리할 수 있다.

지금은 `Script/PlayerSpriteAnimator`가 실행할 때 `manifest.json`의 `canvas`, `pivot_output_top_left`, `content_rectangle`을 읽어 발 기준점과 크기를 정한다.
