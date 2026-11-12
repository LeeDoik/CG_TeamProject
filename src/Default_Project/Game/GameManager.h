#pragma once
#include <vector>
#include <string>
#include "Component.h"
#include "GameTypes.h"
#include "LevelLoader.h"
#include "DebugTools.h"
#include "StoryDecor.h"

class Object;
class Camera;
class Light;
class Material;
class MeshRenderer3D;
class FadeRenderer;

enum class GameState { Title, Playing, Transition, Paused, StageClear, GameOver, Ending };

enum class MenuAction { Start, Quit, Resume, ToTitle, Retry };
struct MenuButton {
	std::string label;
	float x, y, w, h;   // 픽셀, 왼쪽 위 원점
	MenuAction action;
};

// 게임 한 판을 진행하는 컴포넌트.
// 화면 흐름(타이틀·일시정지·게임 오버·엔딩), 레벨 불러오기, 플레이어, 적·함정, 3D 게이지,
// 시점 전환, 추적 카메라, 반투명 벽, 배경 층을 한곳에서 처리한다.
// 충돌·전환 "규칙"은 CoreRules.h의 함수로만 판단한다.
class GameManager : public Component {
public:
	std::string firstLevel = "Levels/tutorial.txt";

	static GameManager* Instance() { return instance; }

	~GameManager() override;
	void Awake() override;
	void Start() override;
	void Update(float deltaTime) override;

	// HUD가 읽는 값
	Camera* GetCamera() const { return camera; }
	int GetHp() const { return hp; }
	int GetMaxHp() const;
	float GetGauge() const { return gauge; }
	ViewMode GetMode() const { return mode; }
	bool IsSwitchBlocked() const { return blockedTimer > 0.0f; }
	int GetFragments() const { return fragments; }
	int GetTotalFragments() const;
	int GetPage() const { return level.page; }
	const std::string& GetSubtitle() const { return level.subtitle; }
	float GetLevelTime() const { return clock; }
	// 페이지 넘김 연출: cover(0~1) = 책장이 화면을 덮는 정도, reveal(0~1) = 다음 장을 드러내는 중 남은 정도
	float GetPageCover() const;
	float GetPageReveal() const { return pageReveal / kPageRevealTime; }
	GameState GetState() const { return state; }
	float GetTime() const { return clock; }
	float GetRealTime() const { return realClock; }
	float GetPlayTime() const { return playTime; }
	bool IsMuted() const;
	std::vector<MenuButton> GetMenuButtons() const;
	int GetSelectedButton() const { return selectedButton; }

	// 한 프레임을 다 그린 직후(HUD가 마지막에) 부른다. 스크린샷 저장용.
	void OnFrameRendered();

private:
	static GameManager* instance;

	struct Solid {
		Object* obj = nullptr;
		FadeRenderer* renderer = nullptr;
		vec3 base{ 0.0f }, center{ 0.0f }, size{ 1.0f };
		vec3 amplitude{ 0.0f };   // 움직이는 발판: 왕복 폭
		float period = 0.0f;      // 움직이는 발판: 주기(초). 0이면 고정
		vec3 delta{ 0.0f };       // 이번 프레임에 움직인 양 (올라탄 플레이어를 같이 옮김)
		std::vector<FadeRenderer*> caps;   // 위에 올린 잔디 (반투명 같이 맞춤)
		Box GetBox() const { return Box::FromCenterSize(center, size); }
	};
	struct Hazard { Box box; };
	struct Worm {
		Object* root = nullptr;
		std::vector<Object*> parts;
		vec3 pos{ 0.0f };
		float minX = 0.0f, maxX = 0.0f, speed = 1.0f;
		int dir = 1;
		bool alive = true;
		Box GetBox() const { return Box::FromCenterSize(pos, vec3(1.0f, 0.6f, 0.6f)); }
	};
	struct Checkpoint { Object* flag = nullptr; vec3 foot{ 0.0f }; bool active = false; };
	struct Pickup { Object* obj = nullptr; vec3 pos{ 0.0f }; bool taken = false; };

	// 카메라
	Object* cameraObj = nullptr;
	Camera* camera = nullptr;
	Light* sunLight = nullptr;
	Light* fillLight = nullptr;
	vec3 camFocus{ 0.0f };

	// 플레이어
	Object* playerRoot = nullptr;
	std::vector<MeshRenderer3D*> playerRenderers;
	Object* shadow = nullptr;
	FadeRenderer* shadowRenderer = nullptr;
	vec3 pos{ 0.0f };            // 충돌 상자 중심
	vec3 vel{ 0.0f };
	bool grounded = false;
	int groundSolid = -1;        // 밟고 있는 solids 번호
	bool facingRight = true;
	float visualYaw = 0.0f;      // 종이처럼 뒤집히는 연출 각도
	float invuln = 0.0f;         // 무적 시간
	float knockTimer = 0.0f;     // 넉백 중에는 좌우 입력을 받지 않음
	float coyote = 0.0f;         // 발판 끝에서 살짝 늦게 눌러도 점프
	float jumpBuffer = 0.0f;     // 착지 직전에 눌러도 점프
	float prevBottom = 0.0f;     // 밟기 판정용 직전 프레임 발 높이

	// 게임 진행
	int hp = 3;
	int fragments = 0;
	float gauge = 1.0f;          // 3D 게이지 (0 ~ 1)
	ViewMode mode = ViewMode::View2D;
	ViewMode targetMode = ViewMode::View2D;
	float viewT = 0.0f;          // 0 = 2D, 1 = 3D (전환 중에는 그 사이 값)
	float transitionTimer = 0.0f;
	float blockedTimer = 0.0f;   // 전환이 막혔을 때 아이콘을 빨갛게
	float shakeTimer = 0.0f;
	GameState state = GameState::Title;
	GameState pausedFrom = GameState::Playing;
	float stateTimer = 0.0f;
	float clock = 0.0f;          // 플레이 중에만 흐르는 시간 (움직이는 발판·적 기준)
	float realClock = 0.0f;      // 항상 흐르는 시간 (메뉴 깜빡임 등)
	float playTime = 0.0f;       // 엔딩에 보여 줄 클리어 시간
	int selectedButton = 0;
	float pageReveal = 0.0f;     // 다음 장이 드러나는 남은 시간
	static constexpr float kPageRevealTime = 0.8f;
	int characterStyle = 0;      // 캐릭터 시안 번호 (C 키로 바꿈)
	float characterMsgTimer = 0.0f;

	// 레벨
	LevelData level;
	std::string currentFile;
	vec3 respawnPoint{ 0.0f };
	std::vector<Solid> solids;
	std::vector<Hazard> hazards;
	std::vector<Worm> worms;
	std::vector<Checkpoint> checkpoints;
	std::vector<Pickup> pickups;
	StoryDecor decor;                    // 팝업북 꾸미기
	std::vector<Object*> levelObjects;   // 레벨을 바꿀 때 지울 오브젝트 (만든 순서)
	std::vector<Light*> levelLights;
	std::string lastTitle;

	// 테스트 도구 (환경 변수 POPUP_LEVEL, POPUP_REPLAY, POPUP_LOG, POPUP_CAPTURE)
	InputReplay replay;
	bool fixedClock = false;
	bool debugLog = false;
	double startTime = -1.0;
	double nextLogTime = 0.0;
	std::vector<double> captureTimes;
	size_t nextCapture = 0;
	bool captureRequested = false;       // F12 또는 자동 캡처
	int captureCount = 0;

	// 만들기
	Object* Spawn(const std::string& name, bool isLevelObject);
	Object* SpawnCube(const std::string& name, vec3 center, vec3 size, const std::string& material, bool isLevelObject);
	Material* GetMaterial(const std::string& name);
	void CreatePersistentObjects();
	void LoadLevel(std::string file, bool resetHp);
	void ClearLevel();

	// 화면 흐름
	void StartGame(std::string file);
	void GoToTitle();
	void HandleMenu();
	void ActivateMenu(MenuAction action);
	bool HandleDebugKeys();
	void SetCharacterStyle(int style);

	// 진행
	void ResetPlayerAt(vec3 p);
	void RespawnAtCheckpoint(bool loseHp);
	void LoseHp();
	void UpdateMovers();
	void UpdatePlayer(float dt);
	void MoveAxis(int axis, float amount);
	void UpdateWorms(float dt);
	void CheckHazardsAndPickups();
	void UpdateGauge(float dt);
	void TrySwitchView();
	void SwitchBlocked();
	void BeginTransition(ViewMode to);
	void UpdateTransition(float dt);

	// 화면
	void UpdateCamera(float dt);
	void SyncVisuals(float dt);
	void UpdateWallFade(float dt);
	void UpdateWindowTitle();
	std::string CurrentMessage() const;

	Box PlayerBox() const;
	std::vector<Box> SolidBoxes() const;
	bool FindGroundTop(float& outTop) const;
};
