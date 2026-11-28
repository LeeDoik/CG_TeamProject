#pragma once
#include "Component.h"
#include <vector>
#include <gl/glm/glm.hpp>
using namespace glm;

class PlayerController;
class ViewTransition;
class BoxCollider;
class Checkpoint;

// ======================================================================
//  HP·체크포인트·낙사 (기획서 4.4)
//
//  [AI 작성, 담당자 검토·설명] 2026-11-28
//  제안서상 이도익 담당(적·함정·HP)이다. 이번에도 AI가 작성하고
//  담당자(이도익)가 읽고 발표·면담에서 설명하기로 했다. 기록: docs/AI_활용_기록.md
//
//  플레이어 오브젝트에 DimensionSwitch 다음으로 붙인다(Game_Scene.json).
//  플레이어가 이번 프레임에 다 움직인 뒤 아래를 검사한다.
//   1. 체크포인트 깃발에 닿으면 그 깃발 자리를 저장한다 (지금 저장된 곳보다 앞에 있는 깃발만)
//   2. 발이 레벨의 killY보다 아래로 떨어지면 HP -1, 마지막 체크포인트로
//   3. HP가 0이 되면 게임 오버. 화면 흐름이 아직 없어서 지금은 스테이지를 처음부터 다시 한다(임시)
//  체크포인트가 하나도 없으면 레벨의 start(처음 발 위치)가 마지막 체크포인트다.
//
//  HUD(하트)·소리·게임 오버 화면은 여기서 하지 않는다.
//  담당자가 붙일 수 있게 아래 "UI·소리·화면 흐름이 읽는 값"으로 상태만 내준다(기획서 10.3).
// ======================================================================
class PlayerHealth : public Component {
public:
	int maxHP = 3;		// HP 3으로 시작 (기획서 7.2 하트 3개)

	void Start() override;
	void Update(float deltaTime) override;

	// ---- 다른 스크립트가 부르는 함수 ----
	void TakeDamage(bool toCheckpoint);	// HP -1. toCheckpoint면 마지막 체크포인트로 (구덩이, 나중에 가시). 같은 프레임에 두 번째부터는 HP를 깎지 않는다
	void ReturnToCheckpoint();			// HP는 그대로 두고 마지막 체크포인트로 (게이지 0 + 2D 복귀 불가, DimensionSwitch가 부른다)
	void RestartStage();				// HP 3, 켜진 깃발을 끄고 start로 (게임 오버의 [다시 시작]이 부를 함수)

	// ---- UI·소리·화면 흐름이 읽는 값 ----
	//  DimensionSwitch::GetBlockedCount처럼 매 프레임 읽고, 횟수가 바뀐 프레임에 효과음·연출을 낸다.
	int GetHP() const { return hp; }							// 지금 HP → 하트 개수
	int GetMaxHP() const { return maxHP; }						// 하트 칸 수
	int GetHurtCount() const { return hurtCount; }				// HP가 줄 때마다 1씩 늘어난다 → hurt 효과음, 하트 줄어드는 연출
	int GetGameOverCount() const { return gameOverCount; }		// HP가 0이 될 때마다 1씩 늘어난다 → 게임 오버 화면
	int GetCheckpointSaveCount() const { return saveCount; }	// 체크포인트를 저장할 때마다 1씩 늘어난다 → checkpoint 효과음
	vec3 GetCheckpoint() const { return checkpoint; }			// 마지막 체크포인트(발 위치)

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	PlayerController* player = nullptr;
	BoxCollider* box = nullptr;
	ViewTransition* view = nullptr;
	vector<Checkpoint*> flags;			// 레벨의 체크포인트 깃발들

	int hp = 3;
	vec3 startFoot = vec3(0.0f);		// 처음 발 위치(레벨의 start). 체크포인트가 없을 때 돌아갈 곳
	vec3 checkpoint = vec3(0.0f);		// 마지막 체크포인트(발 위치)
	bool hasKillY = false;				// 레벨 파일에 killY가 없으면 낙사를 검사하지 않는다
	float killY = 0.0f;

	double lastHurtTime = -1.0;			// 마지막으로 HP가 준 프레임의 시각(ctx.time). 같은 프레임 중복 피해를 막는다
	int hurtCount = 0;
	int gameOverCount = 0;
	int saveCount = 0;

	void TouchCheckpoints();
	void GameOver();
};
