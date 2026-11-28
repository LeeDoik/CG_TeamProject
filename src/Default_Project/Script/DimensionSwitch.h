#pragma once
#include "Component.h"
#include "ViewTransition.h"
#include <gl/glm/glm.hpp>
using namespace glm;

class PlayerController;

// 전환이 막힌 이유 (UI·소리 담당이 이유마다 다르게 보여 주고 싶을 때 쓴다)
enum class SwitchBlock {
	None,
	LowGauge,	// 게이지가 15% 미만이라 3D로 못 간다
	Overlap		// 2D 규칙으로 보면 발판·벽과 겹쳐서 2D로 못 돌아간다
};

// ======================================================================
//  시점 전환 조건과 3D 게이지 (기획서 4.1, 4.2)
//
//  [AI 작성, 담당자 검토·설명] 2026-11-24
//  제안서 4.2에서 "직접 작성"하기로 한 전환 조건이다. 이번에는 AI가 작성하고
//  담당자(이도익)가 읽고 발표·면담에서 설명하기로 했다. 기록: docs/AI_활용_기록.md
//
//  플레이어 오브젝트에 PlayerController 다음으로 붙인다. 플레이어가 이번 프레임에 다 움직인 뒤
//  E 입력을 보고 전환해도 되는지 검사하고, 통과하면 ViewTransition(카메라)에 전환을 시작시킨다.
//  HUD(하트·게이지 막대·2D/3D 아이콘)와 소리는 여기서 하지 않는다.
//  담당자가 붙일 수 있게 아래 "UI·소리가 읽는 값"으로 상태만 내준다(기획서 10.3).
// ======================================================================
class DimensionSwitch : public Component {
public:
	// 기획서 4.2 수치
	float drainTime = 5.0f;		// 3D에 있는 동안 100% → 0%에 걸리는 시간(초)
	float fillTime = 2.5f;		// 2D에 있는 동안 0% → 100%에 걸리는 시간(초)
	float enterMin = 0.15f;		// 게이지가 이보다 적으면 3D로 못 간다
	float warningLevel = 0.2f;	// 3D에서 게이지가 이 이하이면 경고(깜빡임)

	// 기획서 4.1 막힘 연출
	float shakeTime = 0.3f;		// 화면 흔들림 시간(초)

	void Start() override;
	void Update(float deltaTime) override;

	// ---- UI·소리가 읽는 값 ----
	float GetGauge() const { return gauge; }							// 3D 게이지 0~1
	bool IsGaugeWarning() const;	// 3D에 있고 게이지 20% 이하 → 게이지 막대 깜빡임 (2D에서 충전 중일 때는 끈다)
	ViewMode GetMode() const;		// 지금 모드(2D/3D 아이콘). 전환 0.5초 동안은 전환 전 모드
	bool IsTransitioning() const;	// 전환 중(0.5초)인지
	int GetBlockedCount() const { return blockedCount; }				// 전환이 막힐 때마다 1씩 늘어난다 → 값이 바뀌면 blocked 효과음
	float GetTimeSinceBlocked() const { return timeSinceBlocked; }		// 마지막으로 막힌 뒤 지난 시간 → 0.6초 미만이면 아이콘 빨강
	SwitchBlock GetLastBlockReason() const { return lastBlock; }		// 마지막으로 막힌 이유

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	PlayerController* player = nullptr;
	ViewTransition* view = nullptr;

	float gauge = 1.0f;					// 처음에는 가득 차 있다
	int blockedCount = 0;
	float timeSinceBlocked = 999.0f;	// 아직 막힌 적 없음
	SwitchBlock lastBlock = SwitchBlock::None;
	vec3 startFoot = vec3(0.0f);		// 레벨에 start가 없을 때 돌아갈 곳(처음 발 위치)

	void TryEnter3D();
	void TryReturnTo2D();
	void ForceReturnTo2D();
	void Block(SwitchBlock reason);
	vec3 GetRespawnPoint();
};
