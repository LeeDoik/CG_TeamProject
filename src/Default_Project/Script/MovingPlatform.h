#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class Transform;

// 움직이는 발판 (레벨 파일의 mover, 기획서 5.3)
//  위치 = 중심 + 진폭 × sin(2π t / 주기). t는 레벨을 불러온 뒤 지난 시간이라 처음에는 중심에 있다.
//  이동만 한다. 충돌은 같은 오브젝트의 BoxCollider로 엔진이 그대로 처리한다.
//  위에 탄 플레이어를 함께 옮기는 규칙(기획서 4.3)은 충돌 규칙을 다시 쓸 때 정한다.
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
	float elapsed = 0.0f;	// t
};
