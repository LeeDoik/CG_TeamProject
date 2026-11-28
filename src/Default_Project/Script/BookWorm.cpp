#include "BookWorm.h"
#include "Object.h"
#include "Transform.h"
#include "ViewTransition.h"
#include "StageFlow.h"

void BookWorm::Start() {
	tr = gameObject->GetComponent<Transform>();
	if (tr) startPos = tr->position;	// LevelLoader가 worm x y z 자리에 놓아 두었다

	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
		if (!stage) stage = obj->GetComponent<StageFlow>();
	}
}

// 좌우 왕복: 가는 쪽(dir)으로 speed만큼 움직이고, 범위 끝에 닿으면 끝에 맞춘 뒤 방향을 바꾼다.
//  MovingPlatform처럼 sin을 쓰지 않은 이유: 책벌레는 끝에서 느려지지 않고 같은 빠르기로 기어가야 해서다(기획서 5.2 "초속 1.4~2.8").
void BookWorm::Update(float deltaTime) {
	if (!tr || speed <= 0.0f || range <= 0.0f) return;

	// 시점 전환 중에는 게임이 멈춘다(기획서 4.1) → 그 자리에 멈췄다가 전환이 끝나면 이어서 움직인다
	if (view && view->IsTransitioning()) return;
	if (stage && stage->IsStopped()) return;	// 스테이지 클리어 대기·엔딩도 같다(기획서 3.3)

	vec3 pos = tr->position;
	pos.x += dir * speed * deltaTime;

	float left = startPos.x - range;
	float right = startPos.x + range;
	if (pos.x >= right) {
		pos.x = right;
		dir = -1.0f;
	}
	else if (pos.x <= left) {
		pos.x = left;
		dir = 1.0f;
	}

	// SetLocalPosition이 BoxCollider 범위(minPos/maxPos)도 다시 계산하므로 닿음 판정 상자도 같이 움직인다
	tr->SetLocalPosition(pos);
}

// 처치되면 오브젝트를 끈다(isValid = false). 엔진은 꺼진 오브젝트를 Update하지도 그리지도 않는다(main.cpp).
// Destroy로 지우지 않는 이유: 게임 오버 뒤 다시 할 때 Revive로 되살려야 해서다.
bool BookWorm::IsAlive() const {
	return gameObject->isValid;
}

void BookWorm::Kill() {
	gameObject->isValid = false;
}

void BookWorm::Revive() {
	gameObject->isValid = true;
	dir = 1.0f;
	if (tr) tr->SetLocalPosition(startPos);
}
