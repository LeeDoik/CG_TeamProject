# PaperChild 게임용 리사이즈

브랜치: `sprite-resize`. 게임 로직, 셰이더, 프로젝트 설정은 변경하지 않았다.

## 코드 조사

- `Data_Structure/Vertex.h`: position/color/normal만 있고 UV가 없다.
- `Utility/ObjImporter.cpp`: OBJ의 위치와 법선만 읽고 이미지/UV를 로드하지 않는다.
- `Data_Structure/Material.cpp`, `Shader/Standard_fragment.glsl`: uniform 색상과 조명을 사용하며 텍스처 샘플러가 없다.
- `main.cpp`: OpenGL 3.3 core, 깊이 테스트/컬링 활성화. 이미지 디코더, 텍스처 업로드, 알파 블렌딩 설정이 없다.
- `Game_Scene.json`: Player는 Red 재질의 Cube, scale [0.35, 0.6, 0.25]. 스프라이트 쿼드 크기, 기대 이미지 경로, 프레임 전환 기능이 정의되지 않았다.

따라서 이 결과는 연결용 에셋 준비 완료이며 게임에 연결된 상태는 아니다. 현재 로더가 지원하는 이미지 포맷은 없다.

## 출력 규격과 처리

9장 모두 **384×512, RGBA 8비트, straight-alpha PNG**. 원본 1086×1448 전체 캔버스를 비율 그대로 372×496으로 축소하고 좌우 6px, 상하 8px 투명 여백을 추가했다. 포즈별 자동 자르기/재중앙정렬은 하지 않았다. 공통 좌표를 유지하여 점프 포즈가 바닥으로 강제 정렬되거나 걷기 중 기준점이 달라지지 않는다.

원본은 픽셀아트가 아닌 종이 질감 그림이므로 premultiplied-alpha Lanczos 축소 후 straight alpha로 저장했다. 이는 경계의 색 번짐을 줄이며 nearest-neighbor보다 질감 보존에 적합하다. 원본 프리뷰에 표시된 기준점 (530,1389)을 동일 변환한 좌표는 manifest에 기록했다. 원본 idle의 아주 옅은 가장자리 픽셀도 임의로 제거하지 않았다.

384×512는 원본의 3:4 비율을 유지하며, 대상 OpenGL 3.3에서 POT 강제 규격을 만들 필요가 없다. 9장 RGBA 비압축 메모리는 약 6.75MiB(원본 약 54MiB), mipmap 제외. 런타임 지원이 없는 atlas를 임의로 추가하지 않고 독립 프레임을 제공한다. **새 AI 생성 0장, 기존 원본 축소 및 패딩 9장**이다.

원본 루트: `C:/TUKorea/4_2/Computer Graphics/CG_TeamProject/src/Default_Project/Assets/Characters/PaperChild/`

출력 루트: 이 브랜치의 `src/Default_Project/Assets/Characters/PaperChild/`

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

원본 `art_preview/paper_child_pose_lineup.png`(1902×356)와 `paper_child_walk_8fps.gif`(360×480)는 참고 프리뷰이며 게임 텍스처로 사용하지 않았다. 해당 원본들도 수정하지 않았다.

## 확인 및 재현

- `tools/resize_paper_child.py`: Pillow 사용, 출력 크기/모드/알파/여백 검사 및 원본 SHA-256 불변 확인. 9장 통과.
- `manifest.json`: 원본/출력 SHA-256, alpha bbox, 공통 pivot, 변환 규격.
- `art_preview/sprite_resize/contact_sheet.png`: 1152×1632 체크무늬 배경 정적 검토 이미지. 직접 이미지로 확인했으며 모자/얼굴/옷 색감과 포즈를 유지하고 새 잘림은 보이지 않았다.
- `art_preview/sprite_resize/walk_8fps.webp`: 384×512, 4프레임, 프레임당 125ms 검토용 애니메이션. 게임 파일이 아니며 실제 게임 재생 검증으로 간주하지 않는다.
- **게임 내 스프라이트 스크린샷 없음**: 코드에 텍스처 렌더링 경로가 없어 파일 배치만으로 표시할 수 없다. 프리뷰를 게임 스크린샷으로 대체하지 않았다.
- **Debug x64 빌드 성공(exit 0)**: VS 18 Community MSBuild로 `src/Default_Project/Default_Project.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo` 실행. C4819 인코딩, C4244/C4267 형 변환, LNK4098 런타임 라이브러리 경고가 있었다. 빌드를 위해 소스를 바꾸지 않았다.
- 실행 파일 `src/Default_Project/x64/Debug/Default_Project.exe`를 프로젝트 디렉터리를 작업 디렉터리로 실행했다. **현재 게임 기준 화면** `art_preview/sprite_resize/game_baseline.jpg`(1099×648 캡처)에서 빨간 큐브 플레이어와 플랫폼 렌더링을 직접 확인했다. 새 스프라이트가 표시되는 증거는 아니며 이동/점프 수동 조작 검증도 하지 않았다. 확인용으로 띄운 게임 창은 종료했다.

재현 명령(Pillow 필요):

```powershell
python -X utf8 tools/resize_paper_child.py --source 'C:/TUKorea/4_2/Computer Graphics/CG_TeamProject/src/Default_Project/Assets/Characters/PaperChild'
```

## 후속 연결 제안 (미구현)

PNG 디코더와 RGBA 텍스처 업로드, UV가 있는 3:4 쿼드, sampler2D 셰이더를 추가해야 한다. 이미지 좌표의 위/아래와 OpenGL UV 방향을 한 번만 뒤집고, `GL_CLAMP_TO_EDGE`, `GL_LINEAR`, straight-alpha용 `SRC_ALPHA/ONE_MINUS_SRC_ALPHA`를 적용한다. 투명 픽셀의 depth write 처리와 3D 뷰의 빌보드/컬링 정책도 정해야 한다. manifest의 공통 pivot을 쿼드 원점으로 사용하고 월드 크기는 플레이어 충돌체에 맞춰 별도로 정한다. 걷기 프레임은 01→02→03→04, 8fps를 초기값으로 권장하며 왼쪽은 UV 반전으로 처리할 수 있다. 이 manifest는 문서용이며 현재 게임에서 자동으로 읽히지 않는다.
