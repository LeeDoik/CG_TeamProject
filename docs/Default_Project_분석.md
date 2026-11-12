# Default_Project 분석

팀원이 올린 `src/Default_Project`(main `bd62e91` 기준)를 읽고 정리한 문서다. 코드를 읽어서 알아낸 내용이고, 실행해서 확인한 것은 아니다. 파일 경로는 `src/Default_Project` 기준이다.

## 주요 내용 요약

- **구조:** Unity와 비슷하다. `Object`에 `Component`를 붙이고, 씬은 JSON 파일(`Default_Scene.json`)로 만든다. 컴포넌트는 `REGISTER_COMPONENT` 매크로로 등록하면 JSON에서 이름으로 붙일 수 있다.
- **한 프레임 순서:** 입력 → 새 오브젝트 `Start` → 전체 `Update` → 충돌 검사·해소 → 생성·삭제 대기 목록 반영 → 카메라별 그리기.
- **렌더링:** 퐁 조명 셰이더(`Standard`) 하나가 중심이다. 방향광·점광·스포트라이트를 최대 10개까지 받는다. 카메라를 여러 대 둘 수 있고(뷰포트·우선순위), 카메라마다 원근·직교를 고를 수 있다.
- **충돌·물리:** 박스(OBB, 분리축 15개), 구, 박스-구 충돌을 검사한다. `RigidBody`는 중력, 힘·충격량, 반발을 처리한다.
- **없는 기능:** 텍스처, 알파 블렌딩(반투명), 글자·UI, 소리가 없다. OBJ의 UV(`vt`)도 읽지 않는다.
- **조심할 점:** 씬 로드 때 붙인 컴포넌트의 `Start`는 한 번만 불리지만, 오브젝트가 시작된 뒤 `AddComponent`로 붙인 컴포넌트는 `Start`가 매 프레임 다시 불린다. 조명이나 부모 오브젝트를 지우면 남은 포인터 때문에 멈출 수 있다. 마찰은 사실상 적용되지 않는다. 자세한 내용은 [10장](#10-조심할-점과-버그)에 있다.
- **우리 게임과 연결:** 2D/3D 전환은 `Camera`의 `isOrthographic`을 켜고 끄는 것만 있다. 부드러운 전환, 2D 충돌 규칙(z 무시), 반투명 벽, 종이 텍스처는 새로 만들어야 한다. [11장](#11-우리-게임에-쓸-때)에 정리했다.

## 1. 빌드·실행 환경

| 항목 | 내용 |
|---|---|
| 솔루션 | `Default_Project.slnx` (새 XML 솔루션 형식) |
| 툴셋 | v145 (VS 2026), C++20, 콘솔 앱 |
| 설정이 된 구성 | **Debug \| x64만** include 경로(`Component;Data_Structure;Script;Utility`)와 링크 옵션(`opengl32.lib glew32.lib glfw3.lib`)이 있다. Release와 Win32 구성에는 둘 다 없어서 그대로는 빌드되지 않는다. |
| 라이브러리 | GLEW·GLFW·GLM은 저장소에 없다. 수업 실습처럼 VS 기본 include 폴더에 `gl/glew.h`, `gl/glfw3.h`, `gl/glm/`이 설치돼 있다고 가정한다. DLL도 없다. |
| 실행 위치 | `Shader/`, `Assets/`, `Default_Scene.json`을 상대 경로로 읽으므로 프로젝트 폴더에서 실행해야 한다. |
| 창 | 1920×1080, OpenGL 3.3 Core |
| 파일 인코딩 | CP949와 UTF-8이 섞여 있다. `main.cpp`, `Mesh.cpp`, `BoxCollider.cpp`, `SceneLoader.cpp`, `Default_Scene.cpp`가 CP949다. |

## 2. 폴더 구조

| 폴더·파일 | 내용 |
|---|---|
| `main.cpp` | GLFW·GLEW 초기화, 셰이더 컴파일, 메시·머티리얼 캐시, 메인 루프, 그리기 |
| `Common.h`, `AllComponent.h`, `AllDataStructure.h` | 공통 include 묶음 |
| `Data_Structure/` | `AppContext`, `Object`, `Component`, `Scene`, `Mesh`, `Material`, `Collider`(충돌체 부모), `Vertex`, `json.hpp`(nlohmann) |
| `Component/` | 기본 컴포넌트: `Transform`, `Camera`, `Light`, `MeshRenderer3D`, `BoxCollider`, `SphereCollider`, `RigidBody`, `Spline`, `MeshSpline` |
| `Component/Lagacy/` | 예전 2D용 코드(`BoxCollider2D`, `MeshRenderer2DLagacy`). 프로젝트에 포함돼 있지 않다. 저장소에 없는 헤더(`Position2D.h`, `Color.h`)를 쓰고 `Render()` 형식도 지금 `Component`와 달라서, 다시 넣으면 빌드가 안 된다. |
| `Script/` | 게임 스크립트 예시: `SceneCamera`(자유 시점 카메라), `TestScript`(테스트용) |
| `Utility/` | `ColliderManager`(충돌), `InputManager`(입력), `SceneLoader`(JSON 씬), `ShaderManager`(셰이더 컴파일), `ObjImporter`(OBJ 읽기) |
| `Shader/` | `Standard`, `Debug`, `Spline`, `LocalPosColor` 셰이더 4쌍 |
| `Assets/` | 기본 메시 OBJ 5개: Cube, Pyramid, Sphere, Torus, SpotLight |
| `Default_Scene.*` | 기본 씬 클래스와 씬 JSON |

## 3. 핵심 구조

### 3.1 AppContext

게임 전체 상태를 담는 구조체다(`Data_Structure/AppContext.h`). 창의 user pointer로 연결돼 콜백에서도 꺼내 쓴다.

| 멤버 | 역할 |
|---|---|
| `time`, `deltaTime` | 현재 시간, 프레임 간격(최대 0.05초로 자름) |
| `inputManager` | 키·마우스 상태 |
| `Hierarchy` | 씬의 모든 `Object` (`unique_ptr`로 소유) |
| `pendingHierarchy`, `pendingDestroyObjects` | 프레임 중에 생성·삭제를 요청한 오브젝트. 프레임 끝에 반영한다. |
| `CollisionObjects`, `pendingCollisionObjects` | 충돌 검사 대상 |
| `cameras`, `pendingCameras` | 카메라 목록 |
| `lights` | 조명 목록 |

### 3.2 Object

`Data_Structure/Object.h`. 이름, 고유 `id`, `isValid`(활성 여부), 컴포넌트 목록을 가진다.

- 만들 때 `Transform`이 자동으로 붙는다.
- `AddComponent<T>(인자...)`는 컴포넌트를 만들어 붙이고 바로 `Awake()`를 부른다.
- `GetComponent<T>()`는 `dynamic_cast`로 첫 번째로 맞는 컴포넌트를 찾는다. 그래서 `GetComponent<Collider>()`로 박스·구 충돌체를 구분 없이 찾을 수 있다.
- `Instantiate(name)`은 자식 오브젝트를 만들어 `pendingHierarchy`에 넣는다. 다음 프레임부터 돈다.
- `Destroy()`는 `pendingDestroyObjects`에 넣기만 하고, 실제 삭제는 프레임 끝에 한다.

### 3.3 Component 수명 주기

`Data_Structure/Component.h`에 정의된 가상 함수다.

| 함수 | 불리는 때 |
|---|---|
| `Awake()` | `AddComponent` 직후 |
| `Start()` | 오브젝트의 첫 프레임 `Update` 전 |
| `Update(dt)` | 매 프레임 |
| `Render(camera)` | 카메라마다 한 번씩 |
| `OnTriggerEnter/Stay/Exit(other)` | 충돌이 시작·유지·끝날 때. 이름과 달리 트리거가 아닌 충돌에도 불린다. |

`Expose(name, &변수)`로 변수를 이름으로 노출하는 기능도 있지만 지금은 `Camera`만 쓴다.

### 3.4 컴포넌트 등록과 JSON 씬

- 컴포넌트마다 `static Component* CreateFromJson(Object*, const json&, const map<string, Object*>&)`를 만든다. `.cpp` 끝에 `REGISTER_COMPONENT(이름)`을 쓰면 전역 등록표(`ComponentRegistry`)에 들어간다.
- `SceneLoader::LoadScene`(`Utility/SceneLoader.cpp`)은 JSON을 다섯 단계로 읽는다.
  1. 오브젝트를 모두 만들고 위치·회전(오일러 각, 도)·크기를 넣는다.
  2. `parent` 이름으로 부모-자식을 연결한다.
  3. 최상위 오브젝트부터 월드 행렬을 계산한다.
  4. `components` 배열의 `type` 이름으로 등록표를 찾아 컴포넌트를 붙인다. 속성은 `properties` 안에 쓴다.
  5. `isValid`를 넣는다.
- 오브젝트마다 `transform.position/rotation/scale`과 `isValid`가 꼭 있어야 한다. 빠지면 JSON 예외로 멈춘다.

### 3.5 메인 루프 한 프레임

`main.cpp:155-246`

1. 시간 계산(`deltaTime` 최대 0.05초)
2. `inputManager.Update()`(이전 키 상태 저장) → ESC 검사 → `glfwPollEvents()`(콜백으로 현재 키 상태 갱신)
3. 아직 시작 안 한 오브젝트의 `Start()`
4. 모든 활성 오브젝트의 `Update(dt)`
5. `ColliderManager::Update()`: 충돌 검사, 밀어내기, 충돌 이벤트
6. 대기 목록 반영: 새 오브젝트, 새 충돌체, 새 카메라 추가 → 삭제 요청 처리
7. `DrawScene()`: 카메라별로 그리고 버퍼 교체

### 3.6 생성과 삭제

- 씬을 불러올 때는 `Scene::Instantiate`나 `SceneLoader`가 `Hierarchy`에 바로 넣는다.
- 게임 도중에는 `Object::Instantiate`를 써야 한다. 반복문이 `Hierarchy`를 도는 중이라 바로 넣으면 안 된다.
- 삭제할 때는 `CollisionObjects`, `cameras`, 부모의 `children`에서 빼고 `Hierarchy`에서 지운다. **`lights`와 자식 오브젝트는 정리하지 않는다**([10장](#10-조심할-점과-버그)).

## 4. 컴포넌트

| 컴포넌트 | 하는 일 | JSON 속성 |
|---|---|---|
| `Transform` | 위치·회전(쿼터니언)·크기. 값을 바꾸면 월드 행렬을 다시 계산하고, 자식과 충돌체에도 전파한다. `Translate`, `Rotate`, `SetParent` 제공 | (오브젝트의 `transform`) |
| `Camera` | 뷰·투영 행렬. 로컬 **+Z 방향**을 본다. 원근(`fov`) 또는 직교(`isOrthographic`). 직교는 화면 100픽셀을 1단위로 잡는다(1920×1080이면 19.2×10.8). 뷰포트 비율과 우선순위로 여러 대를 그린다. `Start`에서 카메라 목록에 등록 | `fov`, `nearPlane`, `farPlane`, `isOrthographic`, `priority`, `backgroundColor`, `viewportRect`, `width`, `height` |
| `Light` | 방향광·점광·스포트라이트. 로컬 +Z 방향으로 비춘다(C++에서 -Z를 넘기고 셰이더에서 뒤집는다). `Awake`에서 조명 목록에 등록 | `lightType`, `lightColor`, `intensity`, `constant`, `linear`, `quadratic`, `cutOff`, `outerCutOff` |
| `MeshRenderer3D` | 메시를 머티리얼로 그린다. 행렬·카메라 위치·조명을 셰이더에 넘긴다. 와이어프레임 옵션 | `mesh`(캐시 이름), `materials`(캐시 이름 배열, 첫 번째만 사용), `isWireframe` |
| `BoxCollider` | 박스 충돌체. `size`는 실제 크기가 아니라 **메시 크기에 곱하는 배율**이다. `isDebug`면 초록 선으로 그린다 | `center`, `size`, `isTrigger`, `isDebug` |
| `SphereCollider` | 구 충돌체. 반지름에는 가장 큰 축의 크기를 곱한다 | `center`, `radius`, `isTrigger`, `isDebug` |
| `RigidBody` | 중력(-9.81), `AddForce`(힘·충격량), 속도 적분 | `mass`, `useGravity`, `isKinematic` |
| `Spline` | 점들을 선으로 잇는다(좌표축 표시에 사용) | `controlPoints`(x,y,z 나열), `color`, `lineWidth` |
| `MeshSpline` | OBJ 파일의 정점을 닫힌 선으로 잇는다 | `pointObj`, `color`, `lineWidth` |
| `SceneCamera` (스크립트) | 자유 시점: 오른쪽 버튼 드래그로 회전, WASD 이동, Q/E 아래·위, Shift 빠르게 | 없음 |
| `TestScript` (스크립트) | R로 위쪽 힘, ↑로 위로 이동, 충돌 중이면 콘솔에 출력 | 없음 |

## 5. 렌더링

### 5.1 그리는 순서

`DrawScene`(`main.cpp:299-380`)

1. 활성 카메라를 모아 `priority`가 낮은 것부터 정렬한다.
2. 모든 오브젝트를 **월드 z좌표가 작은 것부터** 정렬한다(카메라와의 거리가 아니다).
3. 카메라마다 뷰포트·시저를 잡고 색·깊이 버퍼를 지운 뒤 모든 오브젝트의 `Render`를 부른다.
4. 기본 씬에서는 메인 카메라를 그린 다음, 오른쪽 위 미니맵 카메라를 위에 덧그린다.

깊이 검사와 뒷면 제거는 켜져 있고, **블렌딩은 켜는 코드가 없다.** 셰이더가 알파를 내보내도 반투명이 되지 않는다.

### 5.2 셰이더

| 프로그램 | 쓰는 곳 | 내용 |
|---|---|---|
| `Standard` | `MeshRenderer3D` 기본 | 퐁 조명. 조명마다 환경광 0.2 + 난반사 + 정반사(광택 128)를 더하고 `tColor`를 곱한다. 점광·스포트라이트는 거리 감쇠, 스포트라이트는 원뿔 경계를 부드럽게 처리 |
| `Debug` | 충돌체 선 | 초록색 단색 |
| `Spline` | 선 컴포넌트 | `color` 단색 |
| `LocalPosColor` | 머티리얼 `LocalPosColor` | 정점 색(위치+0.5)을 그대로 출력 |

- 정점 형식은 `위치·색·법선`이다(`Vertex.h`). UV가 없어 텍스처를 쓸 수 없다.
- 조명 개수만큼 환경광이 더해져서 조명이 많을수록 그림자 쪽도 밝아진다.
- 법선 행렬(`transpose(inverse(model))`)을 정점 셰이더에서 정점마다 계산한다.

### 5.3 메시와 머티리얼

- `main.cpp`에서 메시 5개와 머티리얼 11개를 미리 만들어 전역 캐시(`meshChache`, `materialChache`)에 넣는다. JSON은 이 이름으로 참조한다.
  - 머티리얼: `LocalPosColor`, `DarkGreen`, `DarkGray`, `LightGray`, `DarkYellow`, `Black`, `Red`, `Green`, `Blue`, `Orange`, `Purple`
- 머티리얼은 uniform 값을 이름별 map에 저장했다가 `Use()` 때 한꺼번에 보낸다. **같은 이름의 머티리얼은 모든 오브젝트가 공유**하므로, 한 오브젝트만 색을 바꾸려면 새 머티리얼을 만들어야 한다.
- OBJ 읽기(`ObjImporter.cpp`)는 `v`, `vn`, `f`만 읽고 `vt`는 무시한다. 면이 정점을 공유하므로 정점 법선은 연결된 면 법선의 평균이 된다. 큐브(정점 8개)도 모서리가 둥글게 음영 처리되어 면마다 평평하게 보이지 않는다.

## 6. 충돌과 물리

`Utility/ColliderManager.cpp`

- **등록:** 충돌체는 `Start`에서 대기 목록에 들어가고, 그다음 프레임부터 검사한다.
- **검사:** 등록된 충돌체를 모두 짝지어 검사한다(O(n²)).
  - 박스-박스: 회전한 박스끼리 분리축 15개(SAT)
  - 구-구: 중심 거리와 반지름 합 비교
  - 박스-구: 구 중심을 박스 로컬 좌표로 옮겨 가장 가까운 점까지 거리 비교
- **밀어내기:** 둘 다 트리거가 아니고 적어도 하나가 움직이는 `RigidBody`(`isKinematic=false`)일 때만 한다.
  - 침투 깊이의 40%만큼 질량 비율로 밀어낸다(허용 오차 0.002).
  - 충격량으로 속도를 바꾼다. 반발 계수는 둘 중 작은 값이고, 느린 충돌(0.2 미만)은 튀지 않는다.
  - 박스-박스의 밀어내는 방향은 면 축 6개 중 가장 얕게 겹친 축으로 정한다.
- **이벤트:** 충돌하면 양쪽 오브젝트의 모든 컴포넌트에 `OnTriggerEnter/Stay/Exit`를 보낸다.
- 충돌은 항상 3D로 판정한다. 2D 화면에서 z를 무시하는 규칙은 없다.

## 7. 입력

`Utility/InputManager.h`

- `GetKeyDown`(이번 프레임에 눌림), `GetKeyPressed`(누르고 있음), `GetKeyUp`(이번 프레임에 뗌). GLFW 키 코드를 쓴다.
- 마우스 버튼도 같은 표에 들어간다(`GLFW_MOUSE_BUTTON_LEFT` 등).
- 마우스 위치는 -1~1 범위(OpenGL 화면 좌표)로 바꿔 저장한다.
- ESC를 누르면 바로 창이 닫힌다(`main.cpp:291`).

## 8. 기본 씬

`Default_Scene.json`과 `Default_Scene.cpp`

| 오브젝트 | 위치 | 컴포넌트 |
|---|---|---|
| MainCamera | (0, 10, 10), 회전 (45, 180, 0) | `Camera`(fov 60, far 1000), `SceneCamera` |
| MinimapCamera | (0, 20, 0), 아래를 봄 | `Camera`(오른쪽 위 23% 뷰포트, 검은 배경, priority 1) |
| Directional Light | (0, 10, 10), 회전 (150, 0, 0) | 스포트라이트 모양 메시(초록), 방향광(흰색) |
| Test | 원점 | 큐브(빨강), `BoxCollider` |
| Test2 | (0, 5, 0) | 구(파랑), `SphereCollider`, `RigidBody`, `TestScript` |
| X/Y/Z_Axis | 원점 | `Spline`으로 그린 빨강·초록·파랑 좌표축(코드에서 추가) |

구가 중력으로 떨어져 큐브에 부딪혀 멈추고, R을 누르고 있으면 위로 떠오르도록 짜여 있다. 큐브에는 `RigidBody`가 없어서 움직이지 않고 구만 밀려난다.

## 9. 새 기능 추가하는 법

### 9.1 스크립트 컴포넌트

```cpp
// Script/Player.h
#pragma once
#include "Component.h"

class Player : public Component {
public:
	float speed = 5.0f;
	void Start() override;
	void Update(float deltaTime) override;
	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);
};
```

```cpp
// Script/Player.cpp
#include "Player.h"
#include "Object.h"
#include "Transform.h"

void Player::Start() {}

void Player::Update(float deltaTime) {
	auto& input = gameObject->ctx.inputManager;
	if (input.GetKeyPressed(GLFW_KEY_D))
		gameObject->GetComponent<Transform>()->Translate(speed * deltaTime, 0.0f, 0.0f, true);
}

Component* Player::CreateFromJson(Object* obj, const json& props, const map<string, Object*>&) {
	Player* p = obj->AddComponent<Player>();
	if (props.contains("speed")) p->speed = props["speed"];
	return p;
}

REGISTER_COMPONENT(Player);
```

1. 위처럼 클래스와 `CreateFromJson`을 만들고 `REGISTER_COMPONENT`를 쓴다.
2. 두 파일을 VS 프로젝트에 추가한다. 빌드에 포함돼야 등록 코드가 실행된다.
3. JSON에 `{ "type": "Player", "properties": { "speed": 7.0 } }`를 넣는다.

기존 스크립트는 `InputManager inputManager = ...`로 입력 상태를 매 프레임 복사하는데, 위처럼 참조(`auto&`)로 받는 편이 낫다.

### 9.2 새 씬

1. `Scene`을 상속하고 `LoadScene(ctx)`에서 `SceneLoader`로 JSON을 읽거나 `Instantiate`·`AddComponent`로 직접 만든다.
2. `main.cpp`의 씬 로드 부분(`Default_Scene defaultScene{screenSize}; defaultScene.LoadScene(ctx);`)을 새 씬으로 바꾼다. 실행 중 씬 전환 기능은 없다.

## 10. 조심할 점과 버그

코드를 읽고 찾은 것이다. 실행해서 재현하지는 않았다.

| 위치 | 내용 | 영향 |
|---|---|---|
| `Object.cpp:43-45` | 씬을 불러올 때 붙인 컴포넌트는 `Object::Start`가 `hasStarted`를 켜고 `Start`를 한 번만 부른다. 그런데 오브젝트가 시작된 **뒤에** `AddComponent`로 붙인 컴포넌트는 `Update`에서 `Start()`를 부르면서 `hasStarted`를 켜지 않는다. | 나중에 붙인 컴포넌트만 `Start`가 매 프레임 불린다. 실제 `Object.cpp`로 확인했다(2프레임 뒤에 붙인 컴포넌트가 이후 3프레임 동안 3번 호출). 충돌체라면 매 프레임 중복 등록된다. 44행 `comp->Start();` 앞에 `comp->hasStarted = true;`를 넣으면 고쳐진다. |
| `Object.cpp:41-49` | 컴포넌트의 `Update` 안에서 같은 오브젝트에 `AddComponent`를 부르면, 지금 순회 중인 `components` 벡터에 원소가 추가된다. | 벡터가 재할당되면 순회가 깨져 멈출 수 있다. 다른 오브젝트에 붙이거나, 붙일 목록을 모아 두었다가 나중에 붙이는 편이 안전하다. |
| `main.cpp:210-242` | 오브젝트를 지울 때 `ctx.lights`에서 빼지 않는다. | 조명이 있는 오브젝트를 지우면 다음 그리기에서 지워진 메모리를 읽어 멈출 수 있다. |
| `main.cpp:210-242` | 부모를 지워도 자식은 남고, 자식의 `parent` 포인터는 지워진 `Transform`을 가리킨다. | 자식이 움직이면 멈출 수 있다. 자식부터 지워야 안전하다. |
| `BoxCollider.h:9`, `SphereCollider.h:9` | `Transform* tr`에 초깃값이 없다. | `AddComponent` 직후 `Start` 전에 위치를 바꾸면 쓰레기 포인터를 읽는다. JSON 씬은 위치를 먼저 정해서 괜찮지만, 코드로 만들 때는 위험하다. |
| `ColliderManager.cpp:505` | 마찰을 충격량 적용 **뒤에** 계산하는데, 마찰 함수는 두 물체가 가까워지는 중일 때만 동작한다(433행). | 충격량 적용 후에는 이미 멀어지는 상태라 마찰이 거의 적용되지 않는다. |
| `MeshRenderer3D.cpp:91` | 외곽선 그리기에서 인덱스 형식을 `GL_UNSIGNED_BYTE`로 쓴다(실제는 `GLuint`). | 지금은 `isOutline`을 켜는 곳이 없어 문제없지만, 켜면 모양이 깨진다. |
| `MeshRenderer3D.cpp:119-151` | JSON 머티리얼에 uniform을 덮어쓰는 코드가 있지만, 이름(문자열)을 주면 건너뛰고 객체를 주면 캐시 조회에서 예외가 난다. | 사실상 동작하지 않는다. 동작해도 공유 머티리얼을 바꿔 다른 오브젝트까지 바뀐다. |
| `Transform.cpp:114-123` | `SetParent`가 예전 부모의 `children`에서 자신을 빼지 않는다. | 계산 결과는 맞지만, 부모를 바꾼 오브젝트를 지운 뒤 예전 부모가 움직이면 지워진 메모리를 읽는다. |
| `main.cpp:333-342` | 그리기 순서를 월드 z로 정렬하고 블렌딩을 켜지 않는다. | 반투명이 안 되고, 켜더라도 카메라 방향에 따라 순서가 틀릴 수 있다. |
| `Camera.cpp:38-40` | 직교 투영 크기를 만들 때의 화면 크기로 고정한다. | 창 크기를 바꾸면 직교 화면이 늘어난다. |
| `Component.h:63-71` | `SetPropertyValue`는 이름이 없으면 메시지만 찍고 계속 진행한다. | 없는 이름을 넣으면 널 포인터를 써서 멈춘다. |
| `AppContext.h:25, 28` | `pendingDestroyCollisionObjects`는 쓰는 곳이 없다. `pendingCameras` 주석은 "제거할"이지만 실제로는 추가 대기 목록이다. | 읽을 때 헷갈린다. |
| 성능 | 그릴 때마다 uniform 위치를 이름으로 찾고, 충돌은 모든 쌍을 검사한다. | 오브젝트가 수백 개를 넘으면 느려질 수 있다. 팀 프로젝트 규모에서는 큰 문제 아니다. |

## 11. 우리 게임에 쓸 때

제안서의 기능을 기준으로, 엔진에 이미 있는 것과 새로 만들어야 하는 것을 나눴다.

| 게임에 필요한 것 | 엔진에 있는 것 | 새로 만들어야 하는 것 |
|---|---|---|
| 2D/3D 시점 전환 | `Camera.isOrthographic`으로 원근·직교를 바로 바꿀 수 있다. | 두 투영 사이를 부드럽게 바꾸려면 `GetProjectionMatrix`가 보간 행렬을 돌려주도록 고쳐야 한다. |
| 2D 충돌(겹쳐 보이면 닿음) | 3D 충돌 검사만 있다. | 2D 모드에서 z를 무시하는 규칙, 3D 복귀 때 깊이를 정하는 규칙 |
| 플레이어 이동·점프 | `RigidBody`(중력·충격량), `InputManager` | 플랫포머용 이동은 물리 엔진보다 직접 계산하는 편이 다루기 쉽다. |
| 반투명 벽·그림자 | 없음 | 블렌딩 켜기, 카메라 거리 기준으로 반투명 물체 정렬, 머티리얼 복사 |
| 종이 질감 | 없음 | 정점 UV, 텍스처 읽기·바인딩, 셰이더 `sampler2D` |
| 조명 | 다중 광원 퐁 셰이더 | 그대로 쓰고 필요한 효과만 더한다. |
| HUD·메뉴·글자 | 없음 | 화면 좌표로 그리는 렌더러, 글꼴 |
| 효과음·배경음 | 없음 | 소리 재생 |
| 스테이지 데이터 | JSON 씬 로더 | 그대로 쓰거나 스테이지 전용 형식을 만든다. |
| 일시정지 | ESC가 창을 닫는다. | ESC 처리 변경 |

### 추가 예정 기능에서 챙길 점

팀원이 UV 읽기, 텍스처, 2D UI, 알파 블렌딩을 엔진에 넣을 예정이다(11/12). 지금 코드 기준으로 함께 바꿔야 하는 부분이다.

- **UV:** OBJ 읽기가 `v` 하나당 정점 하나를 만든다. 큐브는 `v`가 8개인데 `vt`는 14개라서, `vt`만 더 읽으면 한 모서리에 UV가 여러 개 걸려 충돌한다. `v/vt/vn` 조합마다 정점을 따로 만들도록 바꿔야 한다. 그러면 큐브 면이 평평하게 음영 처리되지 않던 문제도 같이 풀린다. `Vertex`에 UV를 넣고 `Mesh`에 속성 3번을 추가해야 한다.
- **알파 블렌딩:** `glEnable(GL_BLEND)`만으로는 부족하다. 불투명한 물체를 먼저 그리고, 반투명한 물체는 카메라에서 먼 것부터 깊이 쓰기를 끄고 그려야 한다. 지금은 모든 물체를 월드 z로 한 번에 정렬한다. 머티리얼을 공유하므로 물체마다 투명도를 다르게 하려면 머티리얼을 따로 만들어야 한다.
- **2D UI:** 지금은 카메라마다 모든 오브젝트를 그린다. UI를 오브젝트로 만들면 미니맵 카메라에도 그려진다. 카메라별로 그릴 대상을 고르는 기능(레이어)이 있거나, UI를 모든 카메라 다음에 화면 좌표로 따로 그려야 한다.

참고로 `LeeDoik` 브랜치에서는 위의 빈칸을 임시로 채워 두었다. `Camera`에 투영 행렬 덮어쓰기(`useProjectionOverride`)를 추가했고, 게임 규칙(`GameManager`, `CoreRules`), 반투명(`FadeRenderer`), 코드로 만든 텍스처(`ProceduralTextures`), HUD(`HudRenderer`), 소리(`Sound`)를 따로 만들었다. 제안서상 직접 작성해야 하는 부분(투영 보간, 충돌 규칙, 셰이더)은 팀원이 다시 써야 한다.
