#include "PlayerController.h"
#include "Object.h"
#include "Transform.h"
#include "RigidBody.h"
#include "BoxCollider.h"
#include "ViewTransition.h"
#include <algorithm>

void PlayerController::Start() {
	tr = gameObject->GetComponent<Transform>();
	rb = gameObject->GetComponent<RigidBody>();
	box = gameObject->GetComponent<BoxCollider>();

	if (!rb) cout << "[PlayerController] " << gameObject->name << " 오브젝트에 RigidBody가 없습니다." << endl;
	if (!box) cout << "[PlayerController] " << gameObject->name << " 오브젝트에 BoxCollider가 없습니다." << endl;
	if (rb) rb->useGravity = false;	// 중력은 기획서 수치(30)로 여기서 직접 준다

	// 시점 전환 컴포넌트: JSON의 "view" 오브젝트, 없으면 씬에서 찾는다
	if (viewObj) view = viewObj->GetComponent<ViewTransition>();
	if (!view) {
		for (auto& obj : gameObject->ctx.Hierarchy) {
			view = obj->GetComponent<ViewTransition>();
			if (view) break;
		}
	}
}

void PlayerController::Update(float deltaTime) {
	if (!rb) return;

	// 1. 시점 전환 중에는 멈춘다. 전환 직전 속도는 저장했다가 끝나면 되살린다.
	bool transitioning = view && view->IsTransitioning();
	if (transitioning) {
		if (!wasTransitioning) savedVelocity = rb->velocity;
		wasTransitioning = true;
		rb->velocity = vec3(0.0f);
		return;
	}
	if (wasTransitioning) {
		rb->velocity = savedVelocity;
		wasTransitioning = false;
	}

	// 2. 지난 프레임 충돌 처리에서 발밑 접촉이 있었으면 땅 위
	grounded = groundContact;
	groundContact = false;

	InputManager& input = gameObject->ctx.inputManager;
	bool is3D = view && view->GetMode() == ViewMode::View3D;

	// rb->velocity에는 지난 프레임 충돌 처리(밀어내기)가 반영되어 있다
	vec3 v = rb->velocity;

	// 3. 좌우 이동 (화면 오른쪽 = +x)
	float dx = 0.0f;
	if (input.GetKeyPressed(GLFW_KEY_A)) dx -= 1.0f;
	if (input.GetKeyPressed(GLFW_KEY_D)) dx += 1.0f;
	v.x = dx * moveSpeed;

	// 4. 깊이 이동: 3D에서만. W = 화면 안쪽(-z), S = 앞쪽(+z)
	if (is3D) {
		float dz = 0.0f;
		if (input.GetKeyPressed(GLFW_KEY_W)) dz -= 1.0f;
		if (input.GetKeyPressed(GLFW_KEY_S)) dz += 1.0f;
		v.z = dz * depthSpeed;
	}
	else v.z = 0.0f;

	// 5. 점프: 입력 여유(미리 받기 0.12초, 발판을 벗어난 뒤 0.08초)
	if (grounded) {
		coyoteTimer = coyoteTime;
		if (v.y < 0.0f) v.y = 0.0f;
	}
	else coyoteTimer = std::max(0.0f, coyoteTimer - deltaTime);

	if (input.GetKeyDown(GLFW_KEY_SPACE)) jumpBufferTimer = jumpBufferTime;
	else jumpBufferTimer = std::max(0.0f, jumpBufferTimer - deltaTime);

	if (jumpBufferTimer > 0.0f && coyoteTimer > 0.0f) {
		v.y = jumpSpeed;
		jumpBufferTimer = 0.0f;
		coyoteTimer = 0.0f;
		grounded = false;
	}

	// 6. 중력: 올라가는 중에 Space를 떼고 있으면 2배 (짧게 누르면 낮게 뜀)
	float g = gravity;
	if (v.y > 0.0f && !input.GetKeyPressed(GLFW_KEY_SPACE)) g *= lowJumpGravityScale;
	v.y -= g * deltaTime;
	v.y = std::max(v.y, -maxFallSpeed);

	// RigidBody::Update가 이 속도로 위치를 옮긴다 (PlayerController를 RigidBody보다 앞에 두면 같은 프레임에 반영)
	rb->velocity = v;
}

void PlayerController::OnTriggerEnter(Object* other) {
	CheckGround(other);
}

void PlayerController::OnTriggerStay(Object* other) {
	CheckGround(other);
}

// 발바닥이 상대 상자 윗면 근처에 있고 가로·깊이로 겹치면 서 있는 것으로 본다
void PlayerController::CheckGround(Object* other) {
	if (!box || !other) return;
	BoxCollider* otherBox = other->GetComponent<BoxCollider>();
	if (!otherBox || otherBox->isTrigger) return;

	// 올라가는 중에 발판 옆면·모서리를 스친 것은 땅이 아니다 (공중 점프 방지)
	if (rb && rb->velocity.y > 0.5f) return;

	const float footTolerance = 0.15f;	// 밀어내기 전 살짝 파묻힌 정도까지 허용
	const float edgeInset = 0.01f;		// 옆면에 붙어 있을 때를 땅으로 잘못 보지 않게

	bool feetOnTop = box->minPos.y >= otherBox->maxPos.y - footTolerance;
	bool overlapX = box->maxPos.x > otherBox->minPos.x + edgeInset && box->minPos.x < otherBox->maxPos.x - edgeInset;
	bool overlapZ = box->maxPos.z > otherBox->minPos.z + edgeInset && box->minPos.z < otherBox->maxPos.z - edgeInset;

	if (feetOnTop && overlapX && overlapZ) groundContact = true;
}

Component* PlayerController::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	PlayerController* pc = obj->AddComponent<PlayerController>();

	if (props.contains("view")) {
		string viewName = props["view"];
		auto it = spawnedObjects.find(viewName);
		if (it != spawnedObjects.end()) pc->viewObj = it->second;
		else cout << "[PlayerController] view 오브젝트를 찾지 못했습니다: " << viewName << endl;
	}
	if (props.contains("moveSpeed")) pc->moveSpeed = props["moveSpeed"];
	if (props.contains("depthSpeed")) pc->depthSpeed = props["depthSpeed"];
	if (props.contains("jumpSpeed")) pc->jumpSpeed = props["jumpSpeed"];
	if (props.contains("gravity")) pc->gravity = props["gravity"];
	if (props.contains("maxFallSpeed")) pc->maxFallSpeed = props["maxFallSpeed"];

	return pc;
}

REGISTER_COMPONENT(PlayerController)
