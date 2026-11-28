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
//  - 2D ↔ 3D 시점 전환 (0.5초 동안 투영 행렬 보간, 그동안 게임 멈춤)
//    E 키 입력과 전환 조건 검사는 DimensionSwitch가 하고, 통과하면 BeginTransition을 부른다
//  - Camera의 투영 행렬을 덮어써서 전환 중간 행렬을 넘긴다
//  - 전환이 막히면 화면을 흔든다(Shake)
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
	float shakeAmplitude = 0.15f;	// 흔들림 세기(칸). 흔들리는 동안 점점 줄어든다

	void Start() override;
	void Update(float deltaTime) override;

	ViewMode GetMode() const { return mode; }
	bool IsTransitioning() const { return transitioning; }
	float GetViewT() const { return viewT; }	// 0 = 2D, 1 = 3D

	void BeginTransition(ViewMode to);	// to 모드로 0.5초 전환을 시작한다 (조건 검사는 부르는 쪽에서)
	void Shake(float duration);			// duration초 동안 화면을 흔든다 (전환 막힘)
	void ResetTo2D();					// 전환 없이 바로 2D, 카메라도 플레이어에 바로 붙인다 (다음 레벨, StageFlow가 부른다)

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
	float shakeTimer = 0.0f;		// 남은 흔들림 시간
	float shakeDuration = 0.0f;		// 이번 흔들림 전체 시간

	void UpdateCamera(float deltaTime);
};
