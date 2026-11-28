#include "DimensionSwitch.h"
#include "Object.h"
#include "BoxCollider.h"
#include "PlayerController.h"
#include "PlayerHealth.h"
#include <algorithm>
#include <cmath>

namespace {
	int Percent(float gauge) {
		return static_cast<int>(std::round(gauge * 100.0f));
	}
}

void DimensionSwitch::Start() {
	player = gameObject->GetComponent<PlayerController>();
	if (!player) cout << "[DimensionSwitch] " << gameObject->name << " 오브젝트에 PlayerController가 없습니다." << endl;

	for (auto& obj : gameObject->ctx.Hierarchy) {
		view = obj->GetComponent<ViewTransition>();
		if (view) break;
	}
	if (!view) cout << "[DimensionSwitch] 장면에 ViewTransition이 없습니다." << endl;

	health = gameObject->GetComponent<PlayerHealth>();
	if (!health) cout << "[DimensionSwitch] " << gameObject->name << " 오브젝트에 PlayerHealth가 없어 게이지 0 + 2D 복귀 불가일 때 체크포인트로 보내지 못합니다." << endl;
}

// 한 프레임의 순서: 게이지 → 바닥나면 강제 복귀 → E 입력이면 전환 조건 검사
void DimensionSwitch::Update(float deltaTime) {
	if (!player || !view) return;
	timeSinceBlocked += deltaTime;

	// 전환 0.5초 동안은 게임이 멈춘다(기획서 4.1). 게이지도 멈추고 E도 받지 않는다.
	if (view->IsTransitioning()) return;

	bool in3D = view->GetMode() == ViewMode::View3D;

	// 1. 게이지 (기획서 4.2): 3D에서는 5초에 1 → 0, 2D에서는 2.5초에 0 → 1
	if (in3D) gauge -= deltaTime / drainTime;
	else gauge += deltaTime / fillTime;
	gauge = std::clamp(gauge, 0.0f, 1.0f);

	// 2. 3D에서 게이지가 바닥나면 2D로 강제 복귀
	if (in3D && gauge <= 0.0f) {
		ForceReturnTo2D();
		return;
	}

	// 3. E: 지금 모드의 반대쪽으로 전환을 시도한다
	//    책벌레에 맞아 넉백 중(0.25초 조작 불가, 기획서 4.4)이면 E도 받지 않는다
	if (!player->IsControlLocked() && gameObject->ctx.inputManager.GetKeyDown(GLFW_KEY_E)) {
		if (in3D) TryReturnTo2D();
		else TryEnter3D();
	}
}

// 2D → 3D (기획서 4.1)
void DimensionSwitch::TryEnter3D() {
	// 조건: 게이지 15% 이상
	if (gauge < enterMin) {
		Block(SwitchBlock::LowGauge);
		return;
	}

	// 깊이 맞추기: 2D 기준으로 서 있는 발판을 찾아, 플레이어 깊이(z)가 그 발판의 z 범위 안이면 그대로 두고
	// 밖이면 발판 깊이 중앙으로 옮긴다. 2D에서는 깊이가 다른 발판 위에도 서 있을 수 있어서,
	// 그대로 3D로 바꾸면 발밑이 비어 떨어지기 때문이다.
	// 2D의 발판 찾기는 z로 늘린 상자로 하므로(PlayerController::FindGround) 깊이가 다른 발판도 나온다.
	// 공중이면 아래쪽에서 가장 가까운 발판(2D 기준, FindPlatformBelow)에 같은 방법으로 맞춘다.
	// 아래에도 발판이 없으면 깊이를 그대로 둔다.
	bool standing = player->GetGround() != nullptr;
	BoxCollider* platform = standing ? player->GetGround() : player->FindPlatformBelow();
	if (platform) {
		float z = player->GetFootPosition().z;
		if (z < platform->minPos.z || z > platform->maxPos.z) {
			float center = (platform->minPos.z + platform->maxPos.z) * 0.5f;
			player->SetDepth(center);
			cout << "[전환] " << (standing ? "서 있는" : "아래쪽") << " 발판의 깊이에 맞춤: z " << z << " → " << center << endl;
		}
	}

	view->BeginTransition(ViewMode::View3D);
}

// 3D → 2D (기획서 4.1)
void DimensionSwitch::TryReturnTo2D() {
	// 조건: 2D 규칙(깊이 무시)으로 겹치는 발판·벽이 없어야 한다.
	// 겹친 채로 2D가 되면 플레이어가 벽 속에 들어간 모습이 되기 때문이다.
	if (player->OverlapsAny2D()) {
		Block(SwitchBlock::Overlap);
		return;
	}

	view->BeginTransition(ViewMode::View2D);
}

// 게이지가 0이 되었을 때 (기획서 4.2)
void DimensionSwitch::ForceReturnTo2D() {
	// 2D로 돌아갈 수 없는 자리(겹침)라면 HP를 깎지 않고 마지막 체크포인트로 보낸다(기획서 4.4).
	// 위치 옮기기, 2D 전환, 게이지 채우기는 PlayerHealth::ReturnToCheckpoint가 한다(구덩이에 떨어졌을 때와 같은 함수).
	if (health && player->OverlapsAny2D()) {
		cout << "[게이지] 0%인데 2D로 돌아갈 수 없는 자리라서 마지막 체크포인트로 보냅니다 (HP 그대로)" << endl;
		health->ReturnToCheckpoint();
		return;
	}

	cout << "[게이지] 0% → 2D로 강제 복귀" << endl;
	view->BeginTransition(ViewMode::View2D);
}

// 전환 막힘: 화면을 0.3초 흔들고, UI·소리 담당이 쓸 값을 바꾼다
void DimensionSwitch::Block(SwitchBlock reason) {
	++blockedCount;
	timeSinceBlocked = 0.0f;
	lastBlock = reason;
	view->Shake(shakeTime);

	// [연결 지점] 2D/3D 아이콘 0.6초 빨강(UI 담당), blocked 효과음(소리 담당).
	//  GetBlockedCount()가 바뀐 프레임에 효과음, GetTimeSinceBlocked() < 0.6이면 아이콘을 빨갛게 그린다.
	if (reason == SwitchBlock::LowGauge) {
		cout << "[전환 막힘] 게이지 " << Percent(gauge) << "% (" << Percent(enterMin) << "% 미만이라 3D로 못 갑니다)" << endl;
	}
	else cout << "[전환 막힘] 2D로 보면 발판·벽과 겹쳐서 2D로 못 돌아갑니다" << endl;
}

// 경고는 3D에 있을 때만 켠다. 2D에서는 게이지가 차는 중이라 깜빡일 필요가 없다.
bool DimensionSwitch::IsGaugeWarning() const {
	return GetMode() == ViewMode::View3D && gauge <= warningLevel;
}

ViewMode DimensionSwitch::GetMode() const {
	return view ? view->GetMode() : ViewMode::View2D;
}

bool DimensionSwitch::IsTransitioning() const {
	return view && view->IsTransitioning();
}

Component* DimensionSwitch::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	DimensionSwitch* ds = obj->AddComponent<DimensionSwitch>();

	if (props.contains("drainTime")) ds->drainTime = props["drainTime"];
	if (props.contains("fillTime")) ds->fillTime = props["fillTime"];
	if (props.contains("enterMin")) ds->enterMin = props["enterMin"];
	if (props.contains("warningLevel")) ds->warningLevel = props["warningLevel"];
	if (props.contains("shakeTime")) ds->shakeTime = props["shakeTime"];

	return ds;
}

REGISTER_COMPONENT(DimensionSwitch)
