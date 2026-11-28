#include "MovingPlatform.h"
#include "Object.h"
#include "Transform.h"
#include "BoxCollider.h"
#include "ViewTransition.h"
#include "PlayerController.h"
#include "StageFlow.h"
#include <cmath>
#include <numbers>

MovingPlatform::MovingPlatform(vec3 center, vec3 amplitude, float period)
	: center(center), amplitude(amplitude), period(period) {}

void MovingPlatform::Start() {
	tr = gameObject->GetComponent<Transform>();
	box = gameObject->GetComponent<BoxCollider>();

	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
		if (!player) player = obj->GetComponent<PlayerController>();
		if (!stage) stage = obj->GetComponent<StageFlow>();
	}
}

void MovingPlatform::Update(float deltaTime) {
	if (!tr || period <= 0.0f) return;

	// 시점 전환 중에는 t를 늘리지 않는다 → 발판이 그 자리에 멈췄다가 전환이 끝나면 이어서 움직인다
	if (view && view->IsTransitioning()) return;
	if (stage && stage->IsStopped()) return;	// 스테이지 클리어 대기·엔딩도 같다

	elapsed += deltaTime;
	float wave = std::sin(2.0f * std::numbers::pi_v<float> * elapsed / period);

	// SetLocalPosition이 BoxCollider 범위도 다시 계산하므로 충돌 상자도 같이 움직인다
	vec3 before = tr->position;
	tr->SetLocalPosition(center + amplitude * wave);

	// 위에 서 있는 플레이어를 발판이 움직인 만큼 같이 옮긴다 (기획서 4.3)
	if (player && box && player->GetGround() == box) player->RideAlong(tr->position - before, box);
}
