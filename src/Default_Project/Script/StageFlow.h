#pragma once
#include "Component.h"
#include <set>
#include <string>
#include <vector>
using namespace std;

class Object;
class BoxCollider;
class LevelInfo;
class PlayerController;
class PlayerHealth;
class DimensionSwitch;
class PlayerSpriteAnimator;
class ViewTransition;

// 스테이지 상태 (기획서 3.3 게임 흐름)
enum class StageState {
	Playing,	// 플레이 중
	Cleared,	// 조각을 얻었다. STAGE CLEAR를 보여 주며 2초 기다린 뒤 다음 레벨로
	Loading,	// 다음 레벨을 만들었다. 새 오브젝트가 장면에 다 들어갈 때까지(2프레임) 기다린다
	Ending		// 마지막 레벨(next 없음)의 조각까지 얻었다. 엔딩
};

// ======================================================================
//  페이지 조각 획득 · 스테이지 클리어 · 다음 레벨 · 엔딩 데이터 (기획서 3.3)
//
//  [AI 작성, 담당자 검토·설명] 2026-11-28
//  게임 규칙·레벨은 이도익 담당이다. AI가 작성하고 담당자가 읽고 발표·면담에서 설명하기로 했다.
//  기록: docs/AI_활용_기록.md
//
//  Game_Scene.json의 "Stage" 오브젝트에 붙는다. 카메라·플레이어처럼 레벨이 바뀌어도 남는다.
//  JSON에서 Player 뒤에 두어서, 플레이어가 이번 프레임에 다 움직인 뒤에 조각을 검사한다.
//   Playing : 클리어 시간을 재고, 플레이어가 조각에 닿으면 조각을 숨기고 Cleared로
//   Cleared : 2초 기다린 뒤 다음 레벨(next)을 불러온다. next가 없으면 Ending
//   Loading : 새 레벨이 장면에 다 들어가면 HP 3·체크포인트 초기화 후 Playing (다음 스테이지 시작)
//   Ending  : 아무것도 하지 않는다. 모은 조각 수와 클리어 시간을 내준다
//
//  Playing이 아닌 동안(IsStopped) 플레이어 조작, 3D 게이지, HP 검사, 움직이는 발판이 멈춘다.
//  시점 전환 0.5초에 멈추는 것과 같은 방식으로, 그 스크립트들이 Update 첫머리에서 IsStopped()를 본다.
//
//  STAGE CLEAR 띠, PAGE n 리본, 엔딩 화면, 책장 넘김은 여기서 그리지 않는다(화면 흐름·UI, 임성진 담당).
//  아래 "UI·소리·화면 흐름이 읽는 값"으로 상태와 횟수만 내준다(PlayerHealth와 같은 방식, 기획서 10.3).
// ======================================================================
class StageFlow : public Component {
public:
	float clearWait = 2.0f;		// 조각을 얻은 뒤 다음 레벨까지 기다리는 시간(초, 기획서 3.3)
	int totalPages = 4;			// 모아야 할 조각 수 (기획서 1.2 "페이지 조각 4개")

	void Start() override;
	void Update(float deltaTime) override;

	// ---- 다른 스크립트가 보는 값 ----
	bool IsStopped() const { return state != StageState::Playing; }	// 클리어 대기·레벨 바꾸는 중·엔딩이면 true → 게임을 멈춘다

	// ---- UI·소리·화면 흐름이 읽는 값 ----
	//  ...Count는 PlayerHealth처럼 매 프레임 읽고, 값이 바뀐 프레임에 연출·효과음을 시작한다.
	StageState GetState() const { return state; }
	bool IsEnding() const { return state == StageState::Ending; }			// 엔딩 화면을 띄울 때

	int GetStageStartCount() const { return stageStartCount; }		// 스테이지가 시작될 때마다 +1(첫 레벨 포함) → "PAGE n" + 부제 리본
	float GetStageTime() const { return stageTime; }				// 지금 스테이지가 시작된 뒤 지난 시간(초) → 리본을 2.8초 동안 보여 줄 때
	int GetPage() const { return page; }							// 지금 레벨의 page → 리본 "PAGE n"
	string GetStageName() const { return stageName; }				// 지금 레벨의 name (UTF-8. 한글이면 HUD 점 글꼴로는 못 그린다)
	string GetSubtitle() const { return subtitle; }					// 지금 레벨의 subtitle → 리본 영문 부제

	int GetClearCount() const { return clearCount; }				// 조각을 얻을 때마다 +1 → 반짝이, STAGE CLEAR 띠, pickup·clear 효과음
	float GetClearWaitLeft() const { return clearWaitLeft; }		// Cleared일 때 다음 레벨까지 남은 시간(초)
	int GetLevelChangeCount() const { return levelChangeCount; }	// 다음 레벨로 넘어갈 때마다 +1 → 책장 넘김

	int GetCollectedPages() const { return static_cast<int>(collectedPages.size()); }	// 모은 조각 수(page 번호 기준, 같은 page는 한 번) → HUD "n/4", 엔딩
	int GetTotalPages() const { return totalPages; }
	float GetPlayTime() const { return playTime; }					// 클리어 시간(초). 엔딩이 되면 더 늘지 않는다. 무엇을 넣고 빼는지는 Update 설명

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	ViewTransition* view = nullptr;
	PlayerController* player = nullptr;
	PlayerHealth* health = nullptr;			// 다음 스테이지에서 HP 3·체크포인트 초기화
	DimensionSwitch* dimension = nullptr;	// 다음 스테이지에서 게이지 100%
	PlayerSpriteAnimator* animator = nullptr;	// 클리어 그림
	BoxCollider* playerBox = nullptr;

	LevelInfo* level = nullptr;			// 지금 레벨의 정보("Level" 오브젝트). 레벨을 바꾸는 동안은 nullptr
	vector<BoxCollider*> fragments;		// 지금 레벨의 조각 상자들

	StageState state = StageState::Playing;

	// 지금 레벨 값. LevelInfo는 레벨을 바꿀 때 지워지므로 스테이지가 시작될 때 옮겨 둔다
	string levelFile;
	string nextFile;
	int page = 0;
	string stageName;
	string subtitle;

	set<int> collectedPages;			// 얻은 조각의 page 번호들
	float playTime = 0.0f;
	float stageTime = 0.0f;
	float clearWaitLeft = 0.0f;
	int stageStartCount = 0;
	int clearCount = 0;
	int levelChangeCount = 0;

	void FindLevel();
	void BeginStage();
	bool Touches(BoxCollider* fragment) const;
	void ClearStage(BoxCollider* fragment);
	void LoadNextLevel();
	void RemoveLevelObjects(const vector<Object*>& objects);
	bool IsNewLevelReady() const;
	void BeginEnding();
};
