#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class Camera;
class Transform;

enum class ViewMode {
	View2D,		// 직교투영, 정면에서 따라감
	View3D		// 원근투영, 위·앞쪽에서 내려다보며 따라감
};

// 메인 카메라에 붙이는 컴포넌트
//  - 플레이어(target)를 따라가는 카메라
//  - E 키로 2D ↔ 3D 시점 전환 (0.5초 동안 투영 행렬 보간, 그동안 게임 멈춤)
//  - Camera의 투영 행렬을 덮어써서 전환 중간 행렬을 넘긴다
class ViewTransition : public Component {
public:
	// 기획서 4.1 수치
	float transitionTime = 0.5f;	// 전환 시간(초)
	float orthoHalfHeight = 5.6f;	// 2D 화면 세로 절반(칸). 세로 약 11.2칸
	float pitch3D = 40.0f;			// 3D 카메라가 내려다보는 각도(도)
	float distance3D = 12.6f;		// 3D 카메라에서 바라보는 점까지 거리
	float distance2D = 50.0f;		// 2D 카메라 거리. 직교라 화면 크기에는 영향 없음
	float focusHeight = 1.0f;		// 바라보는 점 = 플레이어 위치 + 이 높이
	float followSharpness = 8.0f;	// 따라가는 빠르기(클수록 바로 붙음)

	void Start() override;
	void Update(float deltaTime) override;

	ViewMode GetMode() const { return mode; }
	bool IsTransitioning() const { return transitioning; }
	float GetViewT() const { return viewT; }	// 0 = 2D, 1 = 3D

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	Object* targetObj = nullptr;
	Transform* target = nullptr;
	Camera* camera = nullptr;

	ViewMode mode = ViewMode::View2D;
	ViewMode targetMode = ViewMode::View2D;
	bool transitioning = false;
	float timer = 0.0f;
	float viewT = 0.0f;
	vec3 focus = vec3(0.0f);

	void BeginTransition(ViewMode to);
	void UpdateCamera(float deltaTime);
};
