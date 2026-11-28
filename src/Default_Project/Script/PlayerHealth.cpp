#include "PlayerHealth.h"
#include "Object.h"
#include "BoxCollider.h"
#include "PlayerController.h"
#include "DimensionSwitch.h"
#include "ViewTransition.h"
#include "Checkpoint.h"
#include "Spike.h"
#include "BookWorm.h"
#include "LevelLoader.h"
#include <algorithm>

void PlayerHealth::Start() {
	player = gameObject->GetComponent<PlayerController>();
	dimension = gameObject->GetComponent<DimensionSwitch>();
	box = gameObject->GetComponent<BoxCollider>();
	if (!player || !box) cout << "[PlayerHealth] " << gameObject->name << " 오브젝트에 PlayerController나 BoxCollider가 없습니다." << endl;

	// 장면에서 시점 전환 카메라, 체크포인트 깃발, 가시, 책벌레, 레벨 정보(killY)를 찾는다
	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
		if (Checkpoint* flag = obj->GetComponent<Checkpoint>()) flags.push_back(flag);
		if (obj->GetComponent<Spike>()) {
			if (BoxCollider* spikeBox = obj->GetComponent<BoxCollider>()) spikes.push_back(spikeBox);
		}
		if (BookWorm* worm = obj->GetComponent<BookWorm>()) worms.push_back(worm);
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

// 한 프레임의 순서: 무적 시간 → 체크포인트 깃발 → 가시 → 책벌레 → 낙사. PlayerController·DimensionSwitch가 이번 프레임에 다 움직인 뒤에 본다.
void PlayerHealth::Update(float deltaTime) {
	if (!player || !box) return;

	// 시점 전환 0.5초 동안은 게임이 멈춘다(기획서 4.1). 플레이어가 움직이지 않으니 검사도 쉬고 무적 시간도 멈춘다.
	if (view && view->IsTransitioning()) return;

	// 1. 무적 시간 줄이기 (적·가시에 맞은 뒤 1초)
	invincibleTimer = std::max(0.0f, invincibleTimer - deltaTime);

	// 2. 체크포인트 깃발에 닿았나
	TouchCheckpoints();

	// 3. 가시에 닿았나 (기획서 4.4)
	TouchSpikes();

	// 4. 책벌레에 닿았나: 밟기 또는 피해 (기획서 4.4, 5.2)
	TouchWorms();

	// 5. 낙사: 발이 killY보다 아래로 내려가면 HP -1, 마지막 체크포인트로 (기획서 4.4)
	if (hasKillY && player->GetFootPosition().y < killY) FallIntoPit();
}

// 플레이어와 상자가 닿았나: x·y·z 구간이 모두 겹치면 닿은 것이다 (기획서 4.3 차원별 충돌 규칙)
//  가시와 책벌레는 이 함수 하나로 2D와 3D를 다르게 판단한다.
//   - 2D: PlayerController가 플레이어 상자를 z로 100칸까지 늘려 둔다 → z 구간은 언제나 겹친다 → x·y만 본 것과 같다(깊이 무시)
//   - 3D: 원래 크기(깊이 0.5칸) → x·y·z가 모두 겹쳐야 닿는다 → 깊이를 돌아 피할 수 있다
//  벽·발판에 막히는 규칙과 같은 상자를 쓰므로, 2D에서 그 깊이의 벽에 막히는 자리면 그 깊이의 가시에도 닿는다.
//  상자에 회전이 없어서 BoxCollider가 계산해 둔 월드 AABB(minPos/maxPos)를 그대로 비교한다(TouchCheckpoints와 같은 방식).
bool PlayerHealth::Touches(BoxCollider* other) const {
	bool overlapX = box->maxPos.x > other->minPos.x && box->minPos.x < other->maxPos.x;
	bool overlapY = box->maxPos.y > other->minPos.y && box->minPos.y < other->maxPos.y;
	bool overlapZ = box->maxPos.z > other->minPos.z && box->minPos.z < other->maxPos.z;
	return overlapX && overlapY && overlapZ;
}

// 가시 (기획서 4.4 "가시에 닿음 → HP -1, 마지막 체크포인트로")
//  위·옆 구분 없이 닿으면 피해다(위에 내려앉아도 피해).
//  가시는 트리거라서 플레이어를 막지 않는다(PlayerController::GetSolids가 뺀다). 이유:
//   - 닿는 순간 체크포인트로 보내므로 막아 세울 필요가 없다.
//   - 막는 상자(고체)로 만들면 플레이어가 면에 skin(0.005칸) 띄워 붙어서 겹치지 않으므로, "닿음"을 따로 틈까지 재야 한다.
//  다만 3D → 2D 전환 검사(PlayerController::OverlapsAny2D)에서는 가시를 벽처럼 센다(팀 결정).
//   그래서 2D로 바꾸자마자 가시에 닿는 일은 없다. 3D에서 가시 옆에 있으면 2D로 못 돌아간다.
//  무적 중이면 TakeDamage가 아무 일도 하지 않는다(팀 결정: 무적 중 가시는 체크포인트로도 보내지 않는다).
//  가시 여러 개에 한꺼번에 닿아도 한 번만 맞으면 되므로 첫 가시에서 멈춘다.
void PlayerHealth::TouchSpikes() {
	for (BoxCollider* spike : spikes) {
		if (!Touches(spike)) continue;
		cout << "[가시] " << spike->gameObject->name << "에 닿았습니다" << endl;
		TakeDamage(true);
		return;
	}
}

// 책벌레 (기획서 4.4, 5.2)
//  닿음 판단은 가시와 같다(Touches). 닿았으면 "위에서 밟았나"를 본다.
//   밟음 = 이번 프레임에 발이 내려왔고(내려오는 중), 움직이기 전 발이 책벌레 윗면보다 위(또는 같은 높이)였다.
//    → 위에서 떨어지다가 이번 프레임에 처음 윗면 아래로 들어왔다는 뜻이다.
//    속도 대신 "발이 내려왔나"를 보는 이유: 프레임이 느려(최대 0.05초) 한 프레임에 책벌레(높이 0.4칸)를 지나 바닥까지 내려가면
//    바닥에 부딪혀 속도 y가 0이 되지만, 발은 내려왔으므로 밟은 것으로 볼 수 있다.
//    → 책벌레 처치(사라짐), 플레이어는 위로 튀어 오른다(초속 8). 무적 중이어도 밟을 수 있다(피해가 아니므로).
//   그 밖(옆에서 걸어 들어감, 아래에서 뛰어오르다 닿음, 책벌레가 걸어와 닿음)
//    → HP -1, 책벌레 반대쪽 위로 넉백, 0.25초 조작 불가, 1초 무적 (TakeDamage가 무적 중이면 무시한다)
//  "반대쪽"은 플레이어 중심이 책벌레 중심보다 오른쪽이면 오른쪽(+1), 아니면 왼쪽(-1)이다.
void PlayerHealth::TouchWorms() {
	float footBefore = player->GetFootYBeforeMove();
	float footNow = player->GetFootPosition().y;

	for (BookWorm* worm : worms) {
		if (!worm->IsAlive()) continue;
		BoxCollider* wormBox = worm->gameObject->GetComponent<BoxCollider>();
		if (!wormBox || !Touches(wormBox)) continue;

		bool movedDown = footNow < footBefore;
		bool wasAbove = footBefore >= wormBox->maxPos.y;
		if (movedDown && wasAbove) {
			worm->Kill();
			player->StompBounce();
			++stompCount;
			cout << "[책벌레] " << worm->gameObject->name << " 밟아서 처치" << endl;
			// [연결 지점] stomp 효과음(소리 담당): GetStompCount()가 바뀐 프레임에 낸다
			continue;
		}

		float playerX = (box->minPos.x + box->maxPos.x) * 0.5f;
		float wormX = (wormBox->minPos.x + wormBox->maxPos.x) * 0.5f;
		float away = (playerX >= wormX) ? 1.0f : -1.0f;
		if (!IsInvincible()) cout << "[책벌레] " << worm->gameObject->name << "에 옆·아래에서 닿았습니다" << endl;
		TakeDamage(false, away);
	}
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
//  toCheckpoint면 마지막 체크포인트로 보낸다(가시).
//  아니면 knockbackDir 쪽 위로 넉백한다(책벌레, 기획서 4.4 "반대쪽 위로 튕겨 나가고(0.25초 동안 조작 불가)").
//  무적 중이면 아무 일도 하지 않는다. 무적 검사는 이 함수에만 있어서 낙사(FallIntoPit)는 무적이어도 HP가 준다.
//  HP가 0이 되면 LoseHP가 게임 오버(start로)까지 처리하므로 체크포인트·넉백은 하지 않는다.
void PlayerHealth::TakeDamage(bool toCheckpoint, float knockbackDir) {
	if (IsInvincible()) return;
	invincibleTimer = invincibleTime;

	if (!LoseHP()) return;
	if (toCheckpoint) ReturnToCheckpoint();
	else if (knockbackDir != 0.0f) player->Knockback(knockbackDir);
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

// 지금 스테이지를 처음부터 (기획서 3.3 게임 오버의 [다시 시작]): HP 3, 켜진 깃발을 끄고 처치한 책벌레를 되살려 start로
//  임시라서 HP·체크포인트·무적·위치·게이지·책벌레만 처음으로 돌린다. 움직이는 발판(나중에 조각)은 그대로다.
//  책벌레는 체크포인트로 돌아갈 때는 되살리지 않는다(기획서에 없어서 정함). 한 번 처치한 책벌레가 다시 길을 막지 않게 하려는 것이다.
//  [연결 지점] 화면 흐름이 레벨을 다시 불러오는 방식으로 다시 시작하면 그쪽이 이 일을 대신한다.
void PlayerHealth::RestartStage() {
	hp = maxHP;
	invincibleTimer = 0.0f;
	checkpoint = startFoot;
	for (Checkpoint* flag : flags) flag->SetActive(false);
	for (BookWorm* worm : worms) worm->Revive();
	cout << "[HP] 스테이지를 처음부터 다시 합니다 (HP " << hp << ")" << endl;
	ReturnToCheckpoint();
}

Component* PlayerHealth::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	PlayerHealth* ph = obj->AddComponent<PlayerHealth>();
	if (props.contains("maxHP")) ph->maxHP = props["maxHP"];
	if (props.contains("invincibleTime")) ph->invincibleTime = props["invincibleTime"];
	return ph;
}

REGISTER_COMPONENT(PlayerHealth)
