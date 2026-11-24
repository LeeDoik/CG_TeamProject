#pragma once
#include "Component.h"
#include <vector>
#include <gl/glm/glm.hpp>
using namespace glm;

class Transform;
class BoxCollider;
class ViewTransition;

// ======================================================================
//  플레이어 이동·점프와 차원별 충돌 (기획서 4.3, 5.1)
//
//  [AI 작성, 담당자 검토·설명] 2026-11-24
//  제안서 4.2에서 "직접 작성"하기로 한 충돌 처리다. 이번에는 AI가 작성하고
//  담당자(이도익)가 읽고 발표·면담에서 설명하기로 했다. 기록: docs/AI_활용_기록.md
//
//  충돌 규칙 요약
//   1. 겹침 검사는 엔진의 OBB 검사(ColliderManager::CheckBoxBox, 분리축 15개)를 그대로 쓴다.
//   2. 2D에서는 플레이어 충돌 상자를 z축으로 크게 늘린다(제안서 계획).
//      z 구간이 언제나 겹치므로 결과는 x·y만 비교한 것과 같다(깊이 무시).
//   3. 이동은 x → z(3D만) → y 순서로 축마다 따로 하고, 부딪히면 그 면에 붙인다.
//   4. 엔진의 밀어내기(RigidBody + ColliderManager::ResolveCollision)는 쓰지 않는다.
//      플레이어에 RigidBody가 없으면 엔진은 겹침을 알려 주기만(OnTrigger 콜백) 하고 밀지 않는다.
// ======================================================================
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

	// 충돌 수치 (기획서 4.3)
	float skin = 0.005f;			// 면에 붙일 때 띄워 두는 틈. 이유는 MoveAxis 설명
	float maxStep = 0.25f;			// 한 번에 움직이는 최대 거리. 얇은 발판을 건너뛰는 관통을 막는다
	float groundProbe = 0.05f;		// 발밑을 확인하는 거리
	float stepUpLimit = 0.3f;		// 올라오는 발판에 발이 이만큼까지 파묻히면 발판 위로 올린다
	float depth2D = 100.0f;			// 2D에서 늘린 충돌 상자의 깊이(칸). 레벨 깊이(기획서 스테이지는 z -8 ~ 4 정도)보다 충분히 크면 된다.
									// 수천 칸처럼 너무 크게 하면 CheckBoxBox의 EPSILON 오차가 skin보다 커져 바닥에 걸린다

	void Start() override;
	void Update(float deltaTime) override;

	// ---- 다른 스크립트(DimensionSwitch, PlayerSpriteAnimator)가 쓰는 함수 ----
	bool IsGrounded() const { return ground != nullptr; }
	vec3 GetVelocity() const { return velocity; }
	BoxCollider* GetGround() const { return ground; }	// 지금 서 있는 발판. 2D에서는 2D 규칙으로 찾은 발판
	vec3 GetFootPosition() const;			// 발바닥 가운데 (충돌 상자 바닥 가운데)
	bool OverlapsAny2D();					// 2D 규칙(깊이 무시)으로 겹친 발판·벽이 있나 (3D → 2D 검사)
	void SetDepth(float z);					// 깊이(z)만 옮긴다 (2D → 3D 깊이 맞추기)
	void RideAlong(vec3 delta, BoxCollider* platform);	// 서 있는 발판이 delta만큼 움직였을 때 같이 움직인다 (MovingPlatform이 부른다)
	void Respawn(vec3 footPosition);		// 발이 footPosition에 오게 옮기고 속도·발판 기억을 지운다

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	Transform* tr = nullptr;
	BoxCollider* box = nullptr;
	Object* viewObj = nullptr;
	ViewTransition* view = nullptr;

	vec3 velocity = vec3(0.0f);

	BoxCollider* ground = nullptr;		// 지난 Update 끝에 발밑에 있던 발판 (없으면 공중)
	vector<BoxCollider*> ignored;		// 이미 겹친 채 시작해서 빠져나갈 때까지 무시하는 상자

	float jumpBufferTimer = 0.0f;	// 0보다 크면 점프 입력이 남아 있음
	float coyoteTimer = 0.0f;		// 0보다 크면 공중이어도 점프 가능

	bool depthIgnored = false;		// 지금 충돌 상자가 z로 늘어나 있는지 (2D)
	float baseSizeZ = 1.0f;			// 원래 BoxCollider::size.z
	float baseDepth = 1.0f;			// 원래 충돌 상자 깊이(칸)

	void SetDepthIgnored(bool ignore);
	vector<BoxCollider*> GetSolids();
	bool Overlaps(BoxCollider* other);
	bool IsIgnored(BoxCollider* other) const;

	void HandleStartOverlaps();
	bool StepUpOnto(BoxCollider* other);
	void UpdateVelocity(float deltaTime, bool is3D);
	bool MoveAxis(int axis, float amount, BoxCollider* skip = nullptr);
	BoxCollider* FindGround();
};
