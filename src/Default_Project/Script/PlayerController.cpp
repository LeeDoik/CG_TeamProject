#include "PlayerController.h"
#include "Object.h"
#include "Transform.h"
#include "BoxCollider.h"
#include "RigidBody.h"
#include "ColliderManager.h"
#include "ViewTransition.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
	vec3 Center(BoxCollider* b) {
		return (b->minPos + b->maxPos) * 0.5f;
	}

	// 발판 고르기 기준: 플레이어 깊이 z가 발판의 z 범위 안이면 0, 밖이면 발판 깊이 중앙까지의 거리
	float DepthDistance(BoxCollider* b, float z) {
		if (z >= b->minPos.z && z <= b->maxPos.z) return 0.0f;
		return std::abs(Center(b).z - z);
	}
}

void PlayerController::Start() {
	tr = gameObject->GetComponent<Transform>();
	box = gameObject->GetComponent<BoxCollider>();

	if (!box) cout << "[PlayerController] " << gameObject->name << " 오브젝트에 BoxCollider가 없습니다." << endl;
	if (gameObject->GetComponent<RigidBody>()) {
		cout << "[PlayerController] RigidBody가 있으면 엔진이 플레이어를 밀어내서 축별 이동과 부딪힙니다. Game_Scene.json에서 빼 주세요." << endl;
	}

	// 시점 전환 컴포넌트: JSON의 "view" 오브젝트, 없으면 씬에서 찾는다
	if (viewObj) view = viewObj->GetComponent<ViewTransition>();
	if (!view) {
		for (auto& obj : gameObject->ctx.Hierarchy) {
			view = obj->GetComponent<ViewTransition>();
			if (view) break;
		}
	}

	// 2D에서 z로 늘렸다가 3D에서 되돌릴 원래 크기
	if (box) {
		box->RecalculateCollision();
		baseSizeZ = box->size.z;
		baseDepth = box->maxPos.z - box->minPos.z;
	}
}

// 한 프레임의 순서. 축마다 따로 움직이는 것(4번)이 기획서 4.3의 핵심이다.
void PlayerController::Update(float deltaTime) {
	if (!tr || !box) return;

	// 0. 시점 전환 0.5초 동안은 게임이 멈춘다(기획서 4.1). 속도는 그대로 두었다가 전환이 끝나면 이어서 쓴다.
	if (view && view->IsTransitioning()) return;

	// 1. 충돌 규칙을 지금 모드에 맞춘다. 2D: 상자를 z로 늘려 깊이 무시 / 3D: 원래 크기로 x·y·z 비교
	bool is3D = view && view->GetMode() == ViewMode::View3D;
	SetDepthIgnored(!is3D);

	// 2. 움직이기 전에 이미 겹쳐 있는 상자 정리 (올라오는 발판이면 올라타고, 아니면 무시)
	HandleStartOverlaps();

	// 3. 입력 → 속도 (기획서 5장 수치)
	UpdateVelocity(deltaTime, is3D);

	// 4. 축마다 따로 움직인다: x → z(3D만) → y. 부딪히면 그 면에 붙이고 그 축 속도를 0으로 만든다.
	//    y를 마지막에 하므로 바닥에 선 채 옆으로 걸을 때 바닥 모서리에 걸리지 않는다.
	if (MoveAxis(0, velocity.x * deltaTime)) velocity.x = 0.0f;
	if (is3D && MoveAxis(2, velocity.z * deltaTime)) velocity.z = 0.0f;
	if (MoveAxis(1, velocity.y * deltaTime)) velocity.y = 0.0f;

	// 5. 발밑 확인: 서 있는 발판을 기억한다 (점프 가능 여부, 움직이는 발판 따라가기, 2D → 3D 깊이 맞추기에 쓴다)
	ground = FindGround();
}

// 2D에서는 충돌 상자를 z축으로 depth2D칸까지 늘리고, 3D에서는 원래 크기로 되돌린다 (제안서 구현 계획)
//  BoxCollider::size는 메시 크기에 곱하는 배수라서 z 배수만 바꾸고 RecalculateCollision으로 다시 계산한다.
//  상자 중심은 그대로이므로 앞뒤로 똑같이 늘어난다.
void PlayerController::SetDepthIgnored(bool ignore) {
	if (ignore == depthIgnored) return;
	depthIgnored = ignore;
	box->size.z = ignore ? baseSizeZ * (depth2D / baseDepth) : baseSizeZ;
	box->RecalculateCollision();
}

// 플레이어를 막는 상자 목록: 장면의 충돌체 중 내 것이 아니고 트리거가 아닌 BoxCollider
// (트리거는 가시·체크포인트처럼 닿았는지만 볼 물체라서 막지 않는다)
vector<BoxCollider*> PlayerController::GetSolids() {
	vector<BoxCollider*> solids;
	for (Collider* col : gameObject->ctx.CollisionObjects) {
		if (col == box || col->isTrigger || col->type != ColliderType::Box) continue;
		if (!col->gameObject->isValid) continue;
		solids.push_back(static_cast<BoxCollider*>(col));
	}
	return solids;
}

// 겹침 검사는 엔진의 OBB 검사(분리축 15개)를 그대로 쓴다.
// ColliderManager는 AppContext 참조 하나만 들고 있어서 잠깐 만들어 쓰는 비용이 거의 없다.
bool PlayerController::Overlaps(BoxCollider* other) {
	ColliderManager checker{ gameObject->ctx };
	return checker.CheckBoxBox(box, other);
}

bool PlayerController::IsIgnored(BoxCollider* other) const {
	return std::find(ignored.begin(), ignored.end(), other) != ignored.end();
}

// 움직이기 전에 이미 겹쳐 있는 상자 처리 (기획서 4.3 "이미 겹친 채 시작한 물체는 빠져나갈 때까지 무시한다")
//  축별 이동은 "움직이기 전에는 아무것과도 겹쳐 있지 않다"는 전제로 부딪힌 면을 고른다.
//  이미 겹친 상자는 어느 면으로 들어왔는지 알 수 없으므로 아래처럼 따로 처리한다.
//   - 발이 상자 윗면보다 조금(stepUpLimit 0.3칸 이하) 아래: 올라오는 발판에 발이 파묻힌 것이라 윗면 위로 올린다
//     (기획서 4.3 "올라오는 발판은 플레이어를 들어 올린다")
//   - 그 밖의 겹침: ignored에 넣고 빠져나갈 때까지 부딪히지 않는다. 겹치지 않게 되면 목록에서 빠진다.
void PlayerController::HandleStartOverlaps() {
	vector<BoxCollider*> stillIgnored;
	for (BoxCollider* other : GetSolids()) {
		if (!Overlaps(other)) continue;		// 겹치지 않음 → 무시 목록에 있었다면 여기서 빠진다
		if (IsIgnored(other) || !StepUpOnto(other)) stillIgnored.push_back(other);
	}
	ignored = stillIgnored;
}

// 발이 상자 윗면보다 조금 아래에 있으면 윗면 위(skin만큼 위)로 올린다. 올린 자리가 다른 상자와 겹치면 올리지 않는다.
bool PlayerController::StepUpOnto(BoxCollider* other) {
	float sink = other->maxPos.y - box->minPos.y;	// 발이 윗면보다 얼마나 아래에 있나
	if (sink > stepUpLimit) return false;			// 깊이 겹침(머리 위에서 내려온 발판, 옆에서 파고든 벽)은 올리지 않는다

	vec3 before = tr->position;
	tr->SetLocalPosition(before + vec3(0.0f, sink + skin, 0.0f));
	for (BoxCollider* o : GetSolids()) {
		if (!IsIgnored(o) && Overlaps(o)) {
			tr->SetLocalPosition(before);
			return false;
		}
	}
	if (velocity.y < 0.0f) velocity.y = 0.0f;
	return true;
}

// 키 입력과 중력으로 이번 프레임 속도를 정한다 (기획서 3.2, 5.1). 위치는 아직 바꾸지 않는다.
void PlayerController::UpdateVelocity(float deltaTime, bool is3D) {
	InputManager& input = gameObject->ctx.inputManager;
	bool grounded = (ground != nullptr);

	// 좌우 이동 (화면 오른쪽 = +x)
	float dx = 0.0f;
	if (input.GetKeyPressed(GLFW_KEY_A)) dx -= 1.0f;
	if (input.GetKeyPressed(GLFW_KEY_D)) dx += 1.0f;
	velocity.x = dx * moveSpeed;

	// 깊이 이동: 3D에서만. W = 화면 안쪽(-z), S = 앞쪽(+z)
	float dz = 0.0f;
	if (is3D) {
		if (input.GetKeyPressed(GLFW_KEY_W)) dz -= 1.0f;
		if (input.GetKeyPressed(GLFW_KEY_S)) dz += 1.0f;
	}
	velocity.z = dz * depthSpeed;

	// 점프: 입력 여유(미리 받기 0.12초, 발판을 벗어난 뒤 0.08초)
	if (grounded) {
		coyoteTimer = coyoteTime;
		if (velocity.y < 0.0f) velocity.y = 0.0f;
	}
	else coyoteTimer = std::max(0.0f, coyoteTimer - deltaTime);

	if (input.GetKeyDown(GLFW_KEY_SPACE)) jumpBufferTimer = jumpBufferTime;
	else jumpBufferTimer = std::max(0.0f, jumpBufferTimer - deltaTime);

	if (jumpBufferTimer > 0.0f && coyoteTimer > 0.0f) {
		velocity.y = jumpSpeed;
		jumpBufferTimer = 0.0f;
		coyoteTimer = 0.0f;
	}

	// 중력: 올라가는 중에 Space를 떼고 있으면 2배 (짧게 누르면 낮게 뜀)
	float g = gravity;
	if (velocity.y > 0.0f && !input.GetKeyPressed(GLFW_KEY_SPACE)) g *= lowJumpGravityScale;
	velocity.y -= g * deltaTime;
	velocity.y = std::max(velocity.y, -maxFallSpeed);
}

// 축 하나(0 = x, 1 = y, 2 = z)로만 amount만큼 움직인다. 부딪히면 그 면에 붙이고 true를 돌려준다.
//  - 관통 방지: 한 번에 maxStep(0.25칸)씩 나눠 움직인다. 플레이어의 가장 얇은 쪽(깊이 0.5칸)보다 작은 걸음이라
//    빠르게 떨어져도 얇은 발판을 건너뛰지 않는다.
//  - 움직이기 전에는 아무것과도 겹쳐 있지 않으므로(겹쳐 있던 것은 3번에서 정리), 한 걸음 뒤에 겹친 상자는
//    이번 축 이동으로 부딪힌 것이다. 그래서 그 상자의 "들어온 쪽 면"에 붙이면 된다.
//  - 면에 딱 붙이지 않고 skin(0.005칸)만큼 띄운다. 엔진 CheckBoxBox는 수치 오차를 막으려고 비교할 때
//    아주 작은 값(EPSILON)을 더해서, 딱 맞닿은 상자도 겹친 것으로 본다. 딱 붙이면 바닥에 선 채로
//    옆으로 걸을 때 바닥과 부딪혔다고 잘못 판단한다.
//  - skip: 같이 움직이는 발판처럼 이번 이동에서 뺄 상자
bool PlayerController::MoveAxis(int axis, float amount, BoxCollider* skip) {
	if (amount == 0.0f) return false;

	int steps = static_cast<int>(std::ceil(std::abs(amount) / maxStep));
	float step = amount / static_cast<float>(steps);
	vector<BoxCollider*> solids = GetSolids();

	for (int i = 0; i < steps; ++i) {
		vec3 pos = tr->position;
		pos[axis] += step;
		tr->SetLocalPosition(pos);	// Transform이 바뀌면 BoxCollider의 범위(minPos/maxPos)도 다시 계산된다

		bool hit = false;
		for (BoxCollider* other : solids) {
			if (other == skip || IsIgnored(other) || !Overlaps(other)) continue;

			// 부딪힌 면에 붙인다. 레벨의 block에는 회전이 없어서 OBB와 AABB가 같으므로
			// 면 위치는 BoxCollider가 계산해 둔 월드 AABB(minPos/maxPos)에서 읽는다.
			//  +방향으로 가다 부딪힘 → 상대의 작은 쪽 면(min) 앞, -방향 → 큰 쪽 면(max) 뒤
			if (step > 0.0f) pos[axis] += (other->minPos[axis] - skin) - box->maxPos[axis];
			else pos[axis] += (other->maxPos[axis] + skin) - box->minPos[axis];
			tr->SetLocalPosition(pos);
			hit = true;
		}
		if (hit) return true;	// 남은 걸음은 버린다
	}
	return false;
}

// 발밑 groundProbe(0.05칸) 안에 있는 발판을 찾는다. 없으면 nullptr(공중).
//  플레이어를 잠깐 그만큼 내려 보고 겹치는 상자를 찾은 다음 제자리로 돌린다.
//  2D에서는 상자가 z로 늘어나 있어서 깊이가 다른 발판도 찾는다(2D에서는 그 발판 위에 선 것으로 본다).
//  여러 개면 플레이어 깊이(z)가 범위 안에 있는 발판, 없으면 깊이 중앙이 가장 가까운 발판을 고른다.
BoxCollider* PlayerController::FindGround() {
	if (velocity.y > 0.0f) return nullptr;	// 올라가는 중(점프 직후)에는 서 있지 않다

	vec3 start = tr->position;
	tr->SetLocalPosition(start - vec3(0.0f, groundProbe, 0.0f));

	BoxCollider* best = nullptr;
	float bestDistance = FLT_MAX;
	for (BoxCollider* other : GetSolids()) {
		if (IsIgnored(other) || !Overlaps(other)) continue;
		float d = DepthDistance(other, start.z);
		if (d < bestDistance) {
			best = other;
			bestDistance = d;
		}
	}

	tr->SetLocalPosition(start);
	return best;
}

vec3 PlayerController::GetFootPosition() const {
	if (!box) return tr ? tr->worldPosition : vec3(0.0f);
	vec3 c = Center(box);
	return vec3(c.x, box->minPos.y, c.z);
}

// 2D 규칙(깊이 무시)으로 겹친 발판·벽이 있는지 본다 (기획서 4.1 "3D → 2D" 검사)
//  충돌 상자를 잠깐 z로 늘려서 검사하고 원래대로 돌린다. 무시 중인 상자도 센다.
bool PlayerController::OverlapsAny2D() {
	if (!box) return false;

	bool wasIgnoringDepth = depthIgnored;
	SetDepthIgnored(true);

	bool hit = false;
	for (BoxCollider* other : GetSolids()) {
		if (Overlaps(other)) {
			hit = true;
			break;
		}
	}

	SetDepthIgnored(wasIgnoringDepth);
	return hit;
}

// 서 있는 발판이 delta만큼 움직였을 때 같이 움직인다 (기획서 4.3 "움직이는 발판 위에서는 발판과 함께 움직인다")
//  MovingPlatform이 자기가 움직인 바로 그 프레임에 부른다. 플레이어 Update에서 따라가면 발판이 플레이어보다
//  늦게 Update되어 한 프레임씩 늦게 따라가고, 빠른 발판 위에서 미끄러지는 것처럼 보인다.
//  같이 움직이는 발판과는 부딪히지 않게 skip으로 빼고, 다른 벽에는 막힌다. 올라오는 발판이면 같이 올라간다.
void PlayerController::RideAlong(vec3 delta, BoxCollider* platform) {
	if (!tr || !box || platform != ground) return;
	MoveAxis(0, delta.x, platform);
	MoveAxis(2, delta.z, platform);
	MoveAxis(1, delta.y, platform);
}

// 깊이만 옮긴다. Transform 위치가 충돌 상자 중심이므로(Game_Scene.json의 center 0) z를 그대로 넣는다.
void PlayerController::SetDepth(float z) {
	vec3 pos = tr->position;
	pos.z = z;
	tr->SetLocalPosition(pos);
}

// 발이 footPosition에 오게 옮긴다(바닥에서 skin만큼 위). 속도, 서 있던 발판, 무시 목록은 지운다.
void PlayerController::Respawn(vec3 footPosition) {
	if (!box) return;
	vec3 move = footPosition - GetFootPosition();
	tr->SetLocalPosition(tr->position + move + vec3(0.0f, skin, 0.0f));

	velocity = vec3(0.0f);
	ground = nullptr;
	ignored.clear();
	jumpBufferTimer = 0.0f;
	coyoteTimer = 0.0f;
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
