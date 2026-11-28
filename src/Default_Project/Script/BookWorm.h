#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class Transform;
class ViewTransition;
class StageFlow;

// ======================================================================
//  책벌레 하나 (레벨 파일의 worm, 기획서 5.2)
//
//  [AI 작성, 담당자 검토·설명] 2026-11-28  기록: docs/AI_활용_기록.md
//
//  바닥 위에서 처음 x ± range 안을 speed(칸/초)로 좌우 왕복한다. 처음에는 오른쪽(+x)으로 간다.
//  여기는 움직이기만 한다. 플레이어와 닿았을 때(밟기 / 피해) 판단은 PlayerHealth가 한다
//  (MovingPlatform은 움직이기만 하고 올라타기는 PlayerController가 하는 것과 같은 나눔).
//  시점 전환 0.5초, 스테이지 클리어 대기 2초·엔딩 동안은 게임이 멈추므로(기획서 4.1, 3.3) 책벌레도 멈춘다.
//  LevelLoader가 보라 상자 + 트리거 충돌 상자와 함께 붙인다. 트리거라서 플레이어를 막지 않는다.
// ======================================================================
class BookWorm : public Component {
public:
	float range;	// 왕복 범위: 처음 x ± range
	float speed;	// 칸/초. 0 이하이면 움직이지 않는다

	BookWorm(float range, float speed) : range(range), speed(speed) {}

	void Start() override;
	void Update(float deltaTime) override;

	// ---- PlayerHealth가 부른다 ----
	bool IsAlive() const;	// 처치되지 않았나
	void Kill();			// 밟혀서 처치: 사라진다
	void Revive();			// 다시 살린다: 처음 자리에서 오른쪽으로 (게임 오버 뒤 스테이지를 다시 할 때)

	// [연결 지점] 책벌레 그림(이미지 작업)이 생기면 SpriteRenderer::flipX = (GetDirection() < 0)으로 가는 쪽을 보게 한다
	float GetDirection() const { return dir; }	// +1 오른쪽, -1 왼쪽

private:
	Transform* tr = nullptr;
	ViewTransition* view = nullptr;
	StageFlow* stage = nullptr;		// 스테이지 클리어 대기·엔딩 동안은 멈춘다
	vec3 startPos = vec3(0.0f);		// 처음 위치(상자 중심). 왕복의 가운데이고 되살릴 자리
	float dir = 1.0f;
};
