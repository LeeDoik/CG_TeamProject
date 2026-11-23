#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class Transform;
class RigidBody;
class BoxCollider;
class ViewTransition;

// 플레이어 이동·점프
//  - A/D 좌우 이동, Space 점프(짧게 누르면 낮게), 3D에서만 W/S 깊이 이동
//  - 같은 오브젝트의 RigidBody 속도를 정하고, 땅·벽과의 충돌은 엔진 ColliderManager가 밀어낸다
//  - 시점 전환 중에는 멈춘다
//  (2단계에서 기획서 4.3의 차원별 충돌 규칙으로 바꾼다)
class PlayerController : public Component {
public:
	// 기획서 5장 플레이어 수치
	float moveSpeed = 5.0f;			// 좌우 이동(칸/초)
	float depthSpeed = 4.0f;		// 깊이 이동(칸/초), 3D에서만
	float jumpSpeed = 11.0f;		// 점프 순간 위쪽 속도
	float gravity = 30.0f;			// 중력 가속도(칸/초²)
	float maxFallSpeed = 20.0f;		// 최대 낙하 속도(칸/초)
	float lowJumpGravityScale = 2.0f;	// 올라가는 중에 Space를 떼면 중력 배수 (낮은 점프)
	float jumpBufferTime = 0.12f;	// 착지 직전 점프 입력을 미리 받아 두는 시간
	float coyoteTime = 0.08f;		// 발판을 벗어난 뒤에도 점프를 받아 주는 시간

	void Start() override;
	void Update(float deltaTime) override;

	void OnTriggerEnter(Object* other) override;
	void OnTriggerStay(Object* other) override;

	bool IsGrounded() const { return grounded; }

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	Transform* tr = nullptr;
	RigidBody* rb = nullptr;
	BoxCollider* box = nullptr;
	Object* viewObj = nullptr;
	ViewTransition* view = nullptr;

	bool grounded = false;			// 이번 프레임에 땅 위에 있는지
	bool groundContact = false;		// 충돌 처리 중에 발밑 접촉이 있었는지 (다음 Update에서 grounded로 옮김)
	float jumpBufferTimer = 0.0f;	// 0보다 크면 점프 입력이 남아 있음
	float coyoteTimer = 0.0f;		// 0보다 크면 공중이어도 점프 가능

	bool wasTransitioning = false;
	vec3 savedVelocity = vec3(0.0f);	// 전환 직전 속도 (전환이 끝나면 되살림)

	void CheckGround(Object* other);
};
