#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class Transform;
class BoxCollider;
class ViewTransition;
class PlayerController;
class StageFlow;

// 움직이는 발판 (레벨 파일의 mover, 기획서 5.3)
//  위치 = 중심 + 진폭 × sin(2π t / 주기). t는 레벨을 불러온 뒤 지난 시간이라 처음에는 중심에 있다.
//  t는 게임이 흐를 때만 늘어난다. 시점 전환 0.5초, 스테이지 클리어 대기 2초 동안은 게임이 멈추므로(기획서 4.1, 3.3) 발판도 멈춘다.
//  플레이어가 이 발판 위에 서 있으면 발판이 움직인 만큼 같이 옮긴다(PlayerController::RideAlong, 기획서 4.3).
//  위에 서 있지 않을 때 올라오는 발판에 발이 파묻히면 PlayerController가 발판 위로 올린다.
class MovingPlatform : public Component {
public:
	vec3 center;		// 중심
	vec3 amplitude;		// 진폭 (ax, ay, az)
	float period;		// 주기(초), 0보다 커야 한다

	MovingPlatform(vec3 center, vec3 amplitude, float period);

	void Start() override;
	void Update(float deltaTime) override;

private:
	Transform* tr = nullptr;
	BoxCollider* box = nullptr;
	ViewTransition* view = nullptr;
	PlayerController* player = nullptr;
	StageFlow* stage = nullptr;
	float elapsed = 0.0f;	// t
};
