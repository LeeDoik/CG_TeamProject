#include "PlayerHealth.h"
#include "Object.h"
#include "BoxCollider.h"
#include "PlayerController.h"
#include "DimensionSwitch.h"
#include "ViewTransition.h"
#include "Checkpoint.h"
#include "LevelLoader.h"
#include "StageFlow.h"
#include <algorithm>

void PlayerHealth::Start() {
	player = gameObject->GetComponent<PlayerController>();
	dimension = gameObject->GetComponent<DimensionSwitch>();
	box = gameObject->GetComponent<BoxCollider>();
	if (!player || !box) cout << "[PlayerHealth] " << gameObject->name << " 오브젝트에 PlayerController나 BoxCollider가 없습니다." << endl;

	// 장면에서 시점 전환 카메라, 체크포인트 깃발, 레벨 정보(killY)를 찾는다
	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
		if (!stage) stage = obj->GetComponent<StageFlow>();
		if (Checkpoint* flag = obj->GetComponent<Checkpoint>()) flags.push_back(flag);
		if (LevelInfo* info = obj->GetComponent<LevelInfo>()) {
			hasKillY = info->data.hasKillY;
			killY = info->data.killY;
		}
	}
	if (!hasKillY) cout << "[PlayerHealth] 레벨에 killY가 없어 낙사를 검사하지 않습니다." << endl;

	// 처음 발 위치 = 레벨의 start (LevelLoader가 이미 플레이어를 옮겨 두었다). 첫 "마지막 체크포인트"가 된다
	hp = maxHP;
	if (player) startFoot = player->GetFootPosition();
	checkpoint = startFoot;

	// 깃발이 killY 아래에 있으면 돌아가자마자 다시 떨어진다 → 레벨 파일 실수라서 알려만 준다
	for (Checkpoint* flag : flags) {
		if (hasKillY && flag->foot.y < killY) cout << "[PlayerHealth] 경고: x " << flag->foot.x << " 체크포인트가 killY보다 아래에 있습니다." << endl;
	}
}

// 한 프레임의 순서: 무적 시간 → 체크포인트 깃발 → 낙사. PlayerController·DimensionSwitch가 이번 프레임에 다 움직인 뒤에 본다.
void PlayerHealth::Update(float deltaTime) {
	if (!player || !box) return;

	// 시점 전환 0.5초 동안은 게임이 멈춘다(기획서 4.1). 플레이어가 움직이지 않으니 검사도 쉬고 무적 시간도 멈춘다.
	if (view && view->IsTransitioning()) return;

	// 스테이지 클리어 대기 2초·엔딩 동안도 게임이 멈춘다(기획서 3.3). 플레이어가 멈춰 있어 피해도 받지 않는다.
	if (stage && stage->IsStopped()) return;

	// 1. 무적 시간 줄이기 (적·가시에 맞은 뒤 1초)
	invincibleTimer = std::max(0.0f, invincibleTimer - deltaTime);

	// 2. 체크포인트 깃발에 닿았나
	TouchCheckpoints();

	// 3. 낙사: 발이 killY보다 아래로 내려가면 HP -1, 마지막 체크포인트로 (기획서 4.4)
	if (hasKillY && player->GetFootPosition().y < killY) FallIntoPit();
}

// 체크포인트 깃발에 닿았는지 본다 (기획서 4.4 "체크포인트 깃발에 닿음 → 그 자리를 저장하고 효과음")
//  2D·3D 모두 x·y 구간만 비교한다(깊이 무시, 팀 결정). 깃발은 가는 기둥이라 3D에서 깊이까지 맞춰 닿기 어렵기 때문에,
//  3D에서도 깃발 옆을 지나가면 저장되게 했다.
//  상자에 회전이 없어서 BoxCollider가 계산해 둔 월드 AABB(minPos/maxPos)를 그대로 비교한다(PlayerController::FindPlatformBelow와 같은 방식).
//  깃발의 BoxCollider는 트리거라서 플레이어를 막지 않고, 이 비교에 쓸 범위만 준다.
//  지금 저장된 곳보다 앞(x가 큰) 깃발만 저장한다. 뒤로 돌아가 앞서 지난 깃발에 닿아도 저장된 곳이 뒤로 가지 않게 하려는 것이다.
void PlayerHealth::TouchCheckpoints() {
	for (Checkpoint* flag : flags) {
		if (flag->foot.x <= checkpoint.x) continue;		// 저장된 곳과 같거나 뒤에 있는 깃발

		BoxCollider* f = flag->gameObject->GetComponent<BoxCollider>();
		if (!f) continue;
		bool overlapX = box->maxPos.x > f->minPos.x && box->minPos.x < f->maxPos.x;
		bool overlapY = box->maxPos.y > f->minPos.y && box->minPos.y < f->maxPos.y;
		if (!overlapX || !overlapY) continue;

		checkpoint = flag->foot;
		flag->SetActive(true);
		++saveCount;
		cout << "[체크포인트] x " << checkpoint.x << " 저장 (HP " << hp << ")" << endl;
		// [연결 지점] checkpoint 효과음(소리 담당): GetCheckpointSaveCount()가 바뀐 프레임에 낸다
	}
}

// 낙사 (기획서 4.4 "구덩이에 떨어짐"): HP -1, 마지막 체크포인트로.
//  무적과 상관없다(팀 결정). 무적은 적·가시에 닿은 피해(TakeDamage)에만 있으므로 여기서는 보지 않는다.
void PlayerHealth::FallIntoPit() {
	cout << "[낙사] 발이 killY " << killY << " 아래로 떨어졌습니다" << endl;
	if (LoseHP()) ReturnToCheckpoint();
}

// 적·가시에 닿은 피해 (기획서 4.4): HP -1, 그 뒤 invincibleTime(1초) 동안 무적.
//  toCheckpoint면 마지막 체크포인트로 보낸다(가시). 책벌레는 false로 부르고 넉백한다.
//  무적 중이면 아무 일도 하지 않는다. 무적 검사는 이 함수에만 있어서 낙사(FallIntoPit)는 무적이어도 HP가 준다.
void PlayerHealth::TakeDamage(bool toCheckpoint) {
	if (IsInvincible()) return;
	invincibleTimer = invincibleTime;

	// [연결 지점] 책벌레 작업(기획서 4.4): 넉백(반대쪽 위로 튕겨 나감, 0.25초 조작 불가)은
	//  toCheckpoint가 false일 때 여기서 PlayerController에 속도를 넣는다(PlayerController에 함수를 더한다).
	if (LoseHP() && toCheckpoint) ReturnToCheckpoint();
}

// HP를 1 깎는다. 아직 살아 있으면 true(부르는 쪽이 체크포인트로 보낸다), HP가 0이 되면 게임 오버로 가고 false.
//  같은 프레임에 두 번 불리면(예: 책벌레에 맞은 프레임에 구덩이 아래로도 내려감) 두 번째는 HP를 깎지 않는다.
//  ctx.time은 프레임 시작에 한 번 정해지므로, 마지막으로 깎은 시각과 같으면 같은 프레임이다.
//  부르는 순서(어느 스크립트가 먼저 Update되는지)와 상관없이 막을 수 있어서 이 방법을 썼다.
bool PlayerHealth::LoseHP() {
	double now = gameObject->ctx.time;
	if (now == lastHurtTime) return true;	// 이번 프레임에 이미 깎았다

	lastHurtTime = now;
	hp = std::max(0, hp - 1);
	++hurtCount;
	cout << "[HP] -1 → " << hp << endl;
	// [연결 지점] hurt 효과음(소리 담당), 하트 줄어드는 연출(UI 담당): GetHurtCount()가 바뀐 프레임에

	if (hp > 0) return true;
	GameOver();		// 게임 오버가 start로 보내므로 부르는 쪽은 할 일이 없다
	return false;
}

// 마지막 체크포인트로 돌아간다 (기획서 4.4). 낙사, 가시, 게이지 0 + 2D 복귀 불가, 스테이지 다시 시작이 모두 이 함수를 쓴다.
//  - 발을 체크포인트 자리에 놓고 속도·발판 기억을 지운다(PlayerController::Respawn). 떨어지던 속도가 남지 않는다.
//  - 3D 게이지를 가득 채운다(팀 결정). 체크포인트에서 다시 시작할 때 3D를 바로 쓸 수 있게 하려는 것이다.
//  - 2D로 되돌린다. 2D는 깊이를 무시하므로 깃발 아래 발판에 깊이와 상관없이 바로 서서 다시 떨어지지 않는다.
//    3D였으면 0.5초 전환이 시작되고 그동안 게임이 멈춘다(기획서 4.1). 2D였으면 그대로다.
void PlayerHealth::ReturnToCheckpoint() {
	player->Respawn(checkpoint);
	if (dimension) dimension->RefillGauge();
	if (view && view->GetMode() == ViewMode::View3D) view->BeginTransition(ViewMode::View2D);
	cout << "[체크포인트] (" << checkpoint.x << ", " << checkpoint.y << ", " << checkpoint.z << ")로 돌아감 (HP " << hp << ")" << endl;
}

// HP 0 (기획서 4.4, 3.3)
void PlayerHealth::GameOver() {
	++gameOverCount;
	cout << "[HP] 0 → 게임 오버" << endl;

	// [연결 지점] 게임 오버 화면(화면 흐름·상태 머신, 임성진 담당, 기획서 3.3·7.1):
	//  화면이 생기면 아래 RestartStage()를 지우고, GetGameOverCount()가 바뀐 것을 보고 게임을 멈춘 뒤 게임 오버 화면을 띄운다.
	//  [다시 시작] 버튼이 RestartStage()를 부르면 된다. 지금은 화면이 없어서 바로 스테이지를 처음부터 다시 한다(임시).
	RestartStage();
}

// 지금 스테이지를 처음부터 (기획서 3.3 게임 오버의 [다시 시작]): HP 3, 켜진 깃발을 끄고 start로
//  임시라서 HP·체크포인트·무적·위치·게이지만 처음으로 돌린다. 움직이는 발판(나중에 책벌레·조각)은 그대로다.
//  [연결 지점] 화면 흐름이 레벨을 다시 불러오는 방식으로 다시 시작하면 그쪽이 이 일을 대신한다.
void PlayerHealth::RestartStage() {
	hp = maxHP;
	invincibleTimer = 0.0f;
	checkpoint = startFoot;
	for (Checkpoint* flag : flags) flag->SetActive(false);
	cout << "[HP] 스테이지를 처음부터 다시 합니다 (HP " << hp << ")" << endl;
	ReturnToCheckpoint();
}

// 다음 레벨을 불러온 뒤 StageFlow가 부른다 (기획서 3.3 "다음 스테이지로 간다. HP는 3으로 회복한다")
//  새 레벨에서 할 일은 Start와 같다: 장면에서 깃발과 레벨 정보(killY)를 다시 찾고, HP 3, 새 start를 마지막 체크포인트로.
//  지난 레벨 깃발은 이미 지워졌으므로 목록부터 비운다(지운 오브젝트를 가리키는 포인터가 남지 않게).
//  LevelLoader가 플레이어를 새 start에 옮겨 둔 뒤(새 레벨이 장면에 다 들어왔을 때) 불리므로 Start가 지금 발 위치를 start로 적는다.
void PlayerHealth::ResetForNewLevel() {
	flags.clear();
	invincibleTimer = 0.0f;
	Start();
	cout << "[HP] 다음 스테이지: HP " << hp << ", 체크포인트 = start (" << checkpoint.x << ", " << checkpoint.y << ", " << checkpoint.z << ")" << endl;
}

Component* PlayerHealth::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	PlayerHealth* ph = obj->AddComponent<PlayerHealth>();
	if (props.contains("maxHP")) ph->maxHP = props["maxHP"];
	if (props.contains("invincibleTime")) ph->invincibleTime = props["invincibleTime"];
	return ph;
}

REGISTER_COMPONENT(PlayerHealth)
