#include "MovingPlatform.h"
#include "Object.h"
#include "Transform.h"
#include <cmath>
#include <numbers>

MovingPlatform::MovingPlatform(vec3 center, vec3 amplitude, float period)
	: center(center), amplitude(amplitude), period(period) {}

void MovingPlatform::Start() {
	tr = gameObject->GetComponent<Transform>();
}

void MovingPlatform::Update(float deltaTime) {
	if (!tr || period <= 0.0f) return;

	elapsed += deltaTime;
	float wave = std::sin(2.0f * std::numbers::pi_v<float> * elapsed / period);

	// SetLocalPosition이 BoxCollider 범위도 다시 계산하므로 충돌 상자도 같이 움직인다
	tr->SetLocalPosition(center + amplitude * wave);
}
