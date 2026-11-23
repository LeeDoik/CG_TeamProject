#include "ViewTransition.h"
#include "Camera.h"
#include "Object.h"
#include "Transform.h"
#include "CoreRules.h"
#include <cmath>
#include <algorithm>
#include <gl/glm/gtc/quaternion.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

namespace {
	// 카메라 오브젝트의 앞(+z)이 dir을 보게 하는 회전
	// 엔진 Camera는 로컬 +z를 앞으로 쓰고, quatLookAtRH는 -z를 주어진 방향에 맞추므로 방향을 뒤집어 넘긴다.
	quat LookRotation(vec3 dir) {
		return quatLookAtRH(-normalize(dir), vec3(0.0f, 1.0f, 0.0f));
	}
}

void ViewTransition::Start() {
	camera = gameObject->GetComponent<Camera>();
	if (camera) camera->useProjectionOverride = true;
	else cout << "[ViewTransition] " << gameObject->name << " 오브젝트에 Camera가 없습니다." << endl;

	if (targetObj) target = targetObj->GetComponent<Transform>();
	if (target) focus = target->worldPosition;

	UpdateCamera(0.0f);
}

void ViewTransition::Update(float deltaTime) {
	InputManager& input = gameObject->ctx.inputManager;

	// E: 2D ↔ 3D 전환 시작 (전환 조건 검사는 2단계에서 추가)
	if (!transitioning && input.GetKeyDown(GLFW_KEY_E)) {
		BeginTransition(mode == ViewMode::View2D ? ViewMode::View3D : ViewMode::View2D);
	}

	// 전환 진행: 0.5초 동안 viewT를 0→1 또는 1→0으로 바꾼다
	if (transitioning) {
		timer += deltaTime;
		float p = std::min(1.0f, timer / transitionTime);
		viewT = (targetMode == ViewMode::View3D) ? p : 1.0f - p;

		if (p >= 1.0f) {
			mode = targetMode;
			transitioning = false;
			cout << "[시점] " << (mode == ViewMode::View3D ? "3D" : "2D") << " 전환 끝" << endl;
		}
	}

	UpdateCamera(deltaTime);
}

void ViewTransition::BeginTransition(ViewMode to) {
	targetMode = to;
	transitioning = true;
	timer = 0.0f;
	cout << "[시점] " << (to == ViewMode::View3D ? "2D → 3D" : "3D → 2D") << " 전환 시작" << endl;
}

void ViewTransition::UpdateCamera(float deltaTime) {
	if (!camera) return;
	Transform* tr = gameObject->GetComponent<Transform>();

	// 1. 바라보는 점: 플레이어를 부드럽게 따라간다
	if (target) {
		float follow = 1.0f - std::exp(-followSharpness * deltaTime);
		focus += (target->worldPosition - focus) * follow;
	}
	vec3 look = focus + vec3(0.0f, focusHeight, 0.0f);

	// 2. 카메라 자세: 바라보는 점은 고정하고 방향은 회전 보간(slerp), 거리는 선형 보간한다.
	//    위치를 따로 보간하면 전환 도중 화면 중심이 플레이어에서 벗어난다.
	float s = smoothstep(0.0f, 1.0f, viewT);
	float pitchRad = radians(pitch3D);
	vec3 toEye3D = vec3(0.0f, std::sin(pitchRad), std::cos(pitchRad));	// 바라보는 점 → 3D 카메라 방향

	quat rot2D = LookRotation(vec3(0.0f, 0.0f, -1.0f));	// 정면(-z 방향)을 본다
	quat rot3D = LookRotation(-toEye3D);					// 위·앞쪽에서 내려다본다
	quat rot = slerp(rot2D, rot3D, s);
	float dist = mix(distance2D, distance3D, s);
	vec3 eye = look - (rot * vec3(0.0f, 0.0f, 1.0f)) * dist;

	tr->SetLocalRotation(rot);
	tr->SetLocalPosition(eye);

	// 3. 투영 행렬: 직교·원근 행렬을 만들어 BlendProjection(CoreRules)으로 섞는다.
	//    원근 화각은 바라보는 점까지의 거리 dist에서 직교 화면과 같은 크기가 되도록 맞춘다(돌리 줌).
	float aspect = (camera->aspectRatio > 0.0f) ? camera->aspectRatio : 16.0f / 9.0f;	// DrawScene이 매 프레임 갱신
	float halfH = orthoHalfHeight;
	float halfW = halfH * aspect;
	float fov = 2.0f * std::atan(halfH / dist);

	mat4 ortho2D = ortho(-halfW, halfW, -halfH, halfH, camera->nearPlane, camera->farPlane);
	mat4 persp3D = perspective(fov, aspect, camera->nearPlane, camera->farPlane);
	camera->projectionOverride = BlendProjection(ortho2D, persp3D, s);
}

Component* ViewTransition::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	ViewTransition* vt = obj->AddComponent<ViewTransition>();

	if (props.contains("target")) {
		string targetName = props["target"];
		auto it = spawnedObjects.find(targetName);
		if (it != spawnedObjects.end()) vt->targetObj = it->second;
		else cout << "[ViewTransition] target 오브젝트를 찾지 못했습니다: " << targetName << endl;
	}
	if (props.contains("transitionTime")) vt->transitionTime = props["transitionTime"];
	if (props.contains("orthoHalfHeight")) vt->orthoHalfHeight = props["orthoHalfHeight"];
	if (props.contains("pitch3D")) vt->pitch3D = props["pitch3D"];
	if (props.contains("distance3D")) vt->distance3D = props["distance3D"];
	if (props.contains("focusHeight")) vt->focusHeight = props["focusHeight"];
	if (props.contains("followSharpness")) vt->followSharpness = props["followSharpness"];

	return vt;
}

REGISTER_COMPONENT(ViewTransition)
