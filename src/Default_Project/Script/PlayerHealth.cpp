#include "PlayerHealth.h"
#include "Object.h"
#include "BoxCollider.h"
#include "ColliderManager.h"
#include "PlayerController.h"
#include "ViewTransition.h"
#include "Checkpoint.h"
#include "LevelLoader.h"
#include <algorithm>

void PlayerHealth::Start() {
	player = gameObject->GetComponent<PlayerController>();
	box = gameObject->GetComponent<BoxCollider>();
	if (!player || !box) cout << "[PlayerHealth] " << gameObject->name << " 오브젝트에 PlayerController나 BoxCollider가 없습니다." << endl;

	// 장면에서 시점 전환 카메라, 체크포인트 깃발, 레벨 정보(killY)를 찾는다
	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
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

// 한 프레임의 순서: 체크포인트 깃발 → 낙사. PlayerController·DimensionSwitch가 이번 프레임에 다 움직인 뒤에 본다.
void PlayerHealth::Update(float deltaTime) {
	if (!player || !box) return;

	// 시점 전환 0.5초 동안은 게임이 멈춘다(기획서 4.1). 플레이어가 움직이지 않으니 검사도 쉰다.
	if (view && view->IsTransitioning()) return;

	// 1. 체크포인트 깃발에 닿았나
	TouchCheckpoints();

	// 2. 낙사: 발이 killY보다 아래로 내려가면 HP -1, 마지막 체크포인트로 (기획서 4.4)
	if (hasKillY && player->GetFootPosition().y < killY) {
		cout << "[낙사] 발이 killY " << killY << " 아래로 떨어졌습니다" << endl;
		TakeDamage(true);
	}
}

// 체크포인트 깃발에 닿았는지 본다 (기획서 4.4 "체크포인트 깃발에 닿음 → 그 자리를 저장하고 효과음")
//  닿음 검사는 PlayerController와 같은 엔진 OBB 검사(CheckBoxBox)를 쓴다. 플레이어 충돌 상자는 2D에서 z로 늘어나 있으므로
//  2D에서는 깊이가 달라도 닿고, 3D에서는 깊이까지 겹쳐야 닿는다(기획서 4.3의 차원별 충돌 규칙과 같다).
//  지금 저장된 곳보다 앞(x가 큰) 깃발만 저장한다. 뒤로 돌아가 앞서 지난 깃발에 닿아도 저장된 곳이 뒤로 가지 않게 하려는 것이다.
void PlayerHealth::TouchCheckpoints() {
	ColliderManager checker{ gameObject->ctx };
	for (Checkpoint* flag : flags) {
		if (flag->foot.x <= checkpoint.x) continue;		// 저장된 곳과 같거나 뒤에 있는 깃발

		BoxCollider* flagBox = flag->gameObject->GetComponent<BoxCollider>();
		if (!flagBox || !checker.CheckBoxBox(box, flagBox)) continue;

		checkpoint = flag->foot;
		flag->SetActive(true);
		++saveCount;
		cout << "[체크포인트] x " << checkpoint.x << " 저장 (HP " << hp << ")" << endl;
		// [연결 지점] checkpoint 효과음(소리 담당): GetCheckpointSaveCount()가 바뀐 프레임에 낸다
	}
}

// 피해 (기획서 4.4): HP -1. toCheckpoint면 마지막 체크포인트로 보낸다(구덩이, 가시).
//  같은 프레임에 피해가 두 번 들어오면(예: 가시와 구덩이를 한 프레임에 함께 만남) 두 번째는 HP를 깎지 않는다.
//  ctx.time은 프레임 시작에 한 번 정해지므로, 마지막으로 깎은 시각과 같으면 같은 프레임이다.
//  부르는 순서(어느 스크립트가 먼저 Update되는지)와 상관없이 막을 수 있어서 이 방법을 썼다.
void PlayerHealth::TakeDamage(bool toCheckpoint) {
	double now = gameObject->ctx.time;

	// [연결 지점] 책벌레 작업(기획서 4.4): 피해 직후 무적 1초도 여기서 막는다(예: 무적 시간이 남았으면 HP를 깎지 않음).
	//  넉백(반대쪽 위로 튕겨 나감, 0.25초 조작 불가)은 toCheckpoint가 false인 책벌레 피해에서 PlayerController에 속도를 넣는다.
	if (now != lastHurtTime) {
		lastHurtTime = now;
		hp = std::max(0, hp - 1);
		++hurtCount;
		cout << "[HP] -1 → " << hp << endl;
		// [연결 지점] hurt 효과음(소리 담당), 하트 줄어드는 연출(UI 담당): GetHurtCount()가 바뀐 프레임에
		if (hp == 0) {
			GameOver();
			return;
		}
	}

	// HP를 깎지 않았어도 체크포인트로는 보낸다. 구덩이에 빠진 채로 두면 끝없이 떨어지기 때문이다.
	if (toCheckpoint) ReturnToCheckpoint();
}

// 마지막 체크포인트로 돌아간다 (기획서 4.4)
//  - 발을 체크포인트 자리에 놓고 속도·발판 기억을 지운다(PlayerController::Respawn). 떨어지던 속도가 남지 않는다.
//  - 2D로 되돌린다. 2D는 깊이를 무시하므로 깃발 아래 발판에 깊이와 상관없이 바로 서서 다시 떨어지지 않는다.
//    3D였으면 0.5초 전환이 시작되고 그동안 게임이 멈춘다(기획서 4.1). 2D였으면 그대로다.
//  - 3D 게이지는 건드리지 않는다. 기획서에 정해진 것이 없고, 2D에 있으면 2.5초 안에 다시 찬다.
void PlayerHealth::ReturnToCheckpoint() {
	player->Respawn(checkpoint);
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
//  임시라서 HP·체크포인트·위치만 처음으로 돌린다. 3D 게이지와 움직이는 발판(나중에 책벌레·조각)은 그대로다.
//  [연결 지점] 화면 흐름이 레벨을 다시 불러오는 방식으로 다시 시작하면 그쪽이 이 일을 대신한다.
void PlayerHealth::RestartStage() {
	hp = maxHP;
	checkpoint = startFoot;
	for (Checkpoint* flag : flags) flag->SetActive(false);
	cout << "[HP] 스테이지를 처음부터 다시 합니다 (HP " << hp << ")" << endl;
	ReturnToCheckpoint();
}

Component* PlayerHealth::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	PlayerHealth* ph = obj->AddComponent<PlayerHealth>();
	if (props.contains("maxHP")) ph->maxHP = props["maxHP"];
	return ph;
}

REGISTER_COMPONENT(PlayerHealth)
