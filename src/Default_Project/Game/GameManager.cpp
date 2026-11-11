#include "GameManager.h"
#include "CoreRules.h"
#include "OnTopRenderer.h"
#include "FadeRenderer.h"
#include "HudRenderer.h"
#include "Sound.h"
#include "CharacterArt.h"
#include "StoryDecor.h"
#include "ProceduralTextures.h"
#include "Object.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"
#include "Mesh.h"
#include "Material.h"
#include "MeshRenderer3D.h"
#include "ScreenSize.h"
#include <gl/glfw3.h>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <gl/glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
using namespace std;
using namespace glm;

extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;
extern ScreenSize screenSize;

GameManager* GameManager::instance = nullptr;

namespace {
	// 플레이어·이동 수치 (1 = 블록 한 칸)
	const vec3 kPlayerSize{ 0.7f, 1.2f, 0.5f };
	const float kMoveSpeed = 5.0f;          // 좌우 이동
	const float kDepthSpeed = 4.0f;         // 3D 깊이 이동 (W/S)
	const float kJumpSpeed = 11.0f;         // 점프 높이 약 2칸, 거리 약 3.5칸
	const float kGravity = 30.0f;
	const float kMaxFallSpeed = 20.0f;
	const float kSkin = 0.001f;             // 면에 붙일 때 띄우는 간격

	// 게임 규칙 수치 (제안서 1.3)
	const int kMaxHp = 3;
	const int kTotalFragments = 4;
	const float kGaugeDrainPerSec = 1.0f / 5.0f;   // 3D에서 5초면 바닥
	const float kGaugeFillPerSec = 1.0f / 2.5f;    // 2D에서 2.5초면 가득
	const float kMinGaugeToEnter3D = 0.15f;
	const float kTransitionTime = 0.5f;            // 시점 전환 시간
	const float kInvulnTime = 1.0f;

	// 카메라
	const float kOrthoHalfHeight = 5.6f;           // 2D 화면 세로 절반 (블록 칸 수)
	const vec3 kCamOffset3D{ 0.0f, 8.03f, 9.73f }; // 3D: 비스듬한 고정 각도. 바라보는 점까지 약 12 → 화각 약 50도

	// 스테이지 순서 (F1~F4로 바로 가기)
	const char* kStageFiles[] = { "Levels/tutorial.txt", "Levels/stage1.txt", "Levels/stage2.txt", "Levels/stage3.txt" };

	// 종이 인형 그림(128x192)에서 발끝이 충돌 상자 바닥에 오도록 맞춘 사각형 크기·위치
	const vec3 kDollQuadScale{ 0.914f, 1.371f, 1.0f };
	const vec3 kDollQuadOffset{ 0.0f, 0.028f, 0.0f };

	vec3 HalfSize() { return kPlayerSize * 0.5f; }
	float Rand(float a, float b) { return a + (b - a) * (static_cast<float>(rand()) / RAND_MAX); }

	// 배경 배치용: 같은 레벨이면 늘 같은 모양이 나오는 의사 난수
	float Hash01(int i, int seed) {
		uint32_t h = static_cast<uint32_t>(i) * 2654435761u ^ static_cast<uint32_t>(seed) * 40503u;
		h = (h ^ (h >> 15)) * 2246822519u;
		h ^= h >> 13;
		return (h & 0xFFFF) / 65535.0f;
	}

	// 카메라 오브젝트의 앞(+z)이 dir을 보게 하는 회전 (엔진 Camera는 로컬 +z를 앞으로 쓴다)
	quat LookRotation(vec3 dir) { return quatLookAtRH(-normalize(dir), vec3(0.0f, 1.0f, 0.0f)); }

	// 선분 a→b가 상자를 지나는지 (반투명 벽 판정)
	bool SegmentHitsBox(vec3 a, vec3 b, const Box& box) {
		vec3 d = b - a;
		float t0 = 0.0f, t1 = 1.0f;
		for (int i = 0; i < 3; ++i) {
			if (std::abs(d[i]) < 1e-6f) {
				if (a[i] < box.min[i] || a[i] > box.max[i]) return false;
				continue;
			}
			float tn = (box.min[i] - a[i]) / d[i], tf = (box.max[i] - a[i]) / d[i];
			if (tn > tf) std::swap(tn, tf);
			t0 = std::max(t0, tn);
			t1 = std::min(t1, tf);
			if (t0 > t1) return false;
		}
		return true;
	}
}

int GameManager::GetMaxHp() const { return kMaxHp; }
float GameManager::GetPageCover() const {
	if (state != GameState::StageClear) return 0.0f;
	return std::clamp((1.1f - stateTimer) / 1.1f, 0.0f, 1.0f);   // 클리어 2초 중 마지막 1.1초 동안 덮는다
}
int GameManager::GetTotalFragments() const { return kTotalFragments; }
bool GameManager::IsMuted() const { return Sound::IsMuted(); }

GameManager::~GameManager() {
	Sound::Shutdown();
	if (instance == this) instance = nullptr;
}

void GameManager::Awake() {
	instance = this;
}

void GameManager::Start() {
	// 테스트용 환경 변수: 첫 레벨 바꾸기, 입력 재생, 상태 로그, 자동 캡처
	string levelOverride = GetEnvVar("POPUP_LEVEL");
	if (!levelOverride.empty()) firstLevel = levelOverride;
	string replayFile = GetEnvVar("POPUP_REPLAY");
	if (!replayFile.empty()) replay.Load(replayFile);
	debugLog = !GetEnvVar("POPUP_LOG").empty();
	istringstream captureList(GetEnvVar("POPUP_CAPTURE"));
	for (string item; getline(captureList, item, ',');) {
		if (!item.empty()) captureTimes.push_back(atof(item.c_str()));
	}
	sort(captureTimes.begin(), captureTimes.end());

	bool headless = !GetEnvVar("POPUP_HEADLESS").empty();
	Sound::Init(headless || !GetEnvVar("POPUP_MUTE").empty());

	CreatePersistentObjects();
	string style = GetEnvVar("POPUP_CHARACTER");
	SetCharacterStyle(style.empty() ? 0 : atoi(style.c_str()) - 1);
	characterMsgTimer = 0.0f;

	// 테스트(레벨 지정·입력 재생)는 타이틀을 건너뛰고 바로 시작한다
	if (!levelOverride.empty() || replay.active) StartGame(firstLevel);
	else GoToTitle();
	Sound::PlayMusic();
}

// ======================================================================
//  오브젝트 만들기
// ======================================================================

// 메인 루프가 Hierarchy를 도는 중에도 안전하도록 항상 pendingHierarchy에 넣는다.
Object* GameManager::Spawn(const string& name, bool isLevelObject) {
	AppContext& ctx = gameObject->ctx;
	auto obj = make_unique<Object>(ctx, name);
	Object* ptr = obj.get();
	ctx.pendingHierarchy.push_back(move(obj));
	if (isLevelObject) levelObjects.push_back(ptr);
	return ptr;
}

Object* GameManager::SpawnCube(const string& name, vec3 center, vec3 size, const string& material, bool isLevelObject) {
	Object* obj = Spawn(name, isLevelObject);
	Transform* tr = obj->GetComponent<Transform>();
	tr->SetLocalScale(size.x, size.y, size.z);
	tr->SetLocalPosition(center);
	obj->AddComponent<MeshRenderer3D>(meshChache["FlatCube"], GetMaterial(material));
	return obj;
}

Material* GameManager::GetMaterial(const string& name) {
	auto it = materialChache.find(name);
	if (it != materialChache.end()) return it->second;
	cout << "[게임] 없는 재질 이름이라 Paper로 대신합니다: " << name << endl;
	return materialChache["Paper"];
}

// 레벨이 바뀌어도 남는 것: 카메라, 햇빛, 플레이어, 그림자, HUD
void GameManager::CreatePersistentObjects() {
	cameraObj = Spawn("MainCamera", false);
	camera = cameraObj->AddComponent<Camera>(static_cast<float>(screenSize.width), static_cast<float>(screenSize.height));
	camera->nearPlane = 0.1f;
	camera->farPlane = 300.0f;
	camera->backgroundColor = vec4(0.96f, 0.92f, 0.84f, 1.0f);
	camera->useProjectionOverride = true;

	// 방향광: 앞면·윗면은 밝고 옆면은 조금 어둡게 (Standard 셰이더는 direction 쪽에서 빛이 온다고 본다)
	Object* sun = Spawn("Sun", false);
	sunLight = sun->AddComponent<Light>();
	sunLight->lightType = LightType::Directional;
	sunLight->lightColor = vec4(1.0f, 0.97f, 0.9f, 1.0f);
	sunLight->intensity = 0.8f;
	vec3 toLight = normalize(vec3(-0.3f, 0.6f, 0.74f));
	sun->GetComponent<Transform>()->SetLocalRotation(quatLookAtRH(toLight, vec3(0.0f, 1.0f, 0.0f)));

	// 보조광: 반대쪽 옆면이 새까맣게 보이지 않도록 약하게
	Object* fill = Spawn("FillLight", false);
	fillLight = fill->AddComponent<Light>();
	fillLight->lightType = LightType::Directional;
	fillLight->lightColor = vec4(0.9f, 0.95f, 1.0f, 1.0f);
	fillLight->intensity = 0.25f;
	fill->GetComponent<Transform>()->SetLocalRotation(quatLookAtRH(normalize(vec3(0.6f, 0.35f, 0.5f)), vec3(0.0f, 1.0f, 0.0f)));

	// 종이 인형: 투명 배경 그림을 붙인 앞뒤 사각형 (알파 테스트). 2D에서는 OnTopRenderer로 맨 앞에 그린다.
	playerRoot = Spawn("Player", false);
	Object* doll = Spawn("Player_Doll", false);
	Transform* dollTr = doll->GetComponent<Transform>();
	dollTr->SetParent(playerRoot->GetComponent<Transform>());
	dollTr->SetLocalScale(kDollQuadScale.x, kDollQuadScale.y, kDollQuadScale.z);
	dollTr->SetLocalPosition(kDollQuadOffset);
	playerRenderers.push_back(doll->AddComponent<OnTopRenderer>(meshChache["SpriteQuad"], GetMaterial("DollSprite")));

	// 3D 발밑 반투명 그림자
	shadow = Spawn("Player_Shadow", false);
	shadowRenderer = shadow->AddComponent<FadeRenderer>(meshChache["Sphere"], GetMaterial("Shadow"));
	shadowRenderer->alpha = 0.45f;
	shadow->GetComponent<Transform>()->SetLocalScale(0.38f, 0.015f, 0.26f);
	shadow->isValid = false;

	// HUD: 렌더 큐는 z가 작은 것부터 그리므로 z를 아주 크게 두어 맨 마지막에 그린다.
	Object* hud = Spawn("HUD", false);
	hud->GetComponent<Transform>()->SetLocalPosition(0.0f, 0.0f, 100000.0f);
	hud->AddComponent<HudRenderer>()->game = this;

	// 팝업북 꾸미기 (하늘·해달별·구름·꽃잎)
	decor.Init([this](const string& name, bool isLevelObject) { return Spawn(name, isLevelObject); });
	decor.ApplyTheme("day");
}

void GameManager::ClearLevel() {
	// 엔진은 오브젝트를 지워도 ctx.lights에서 빼 주지 않으므로 먼저 뺀다.
	auto& lights = gameObject->ctx.lights;
	for (Light* l : levelLights) lights.erase(remove(lights.begin(), lights.end(), l), lights.end());
	levelLights.clear();

	// 자식(나중에 만든 것)부터 지워야 부모 포인터가 먼저 사라지지 않는다.
	for (auto it = levelObjects.rbegin(); it != levelObjects.rend(); ++it) (*it)->Destroy();
	levelObjects.clear();
	solids.clear();
	hazards.clear();
	worms.clear();
	checkpoints.clear();
	pickups.clear();
	decor.ClearLevel();
	level = LevelData{};
	currentFile.clear();
}

// file은 복사해서 받는다: LoadLevel(currentFile, ...)처럼 부르면 ClearLevel이 currentFile을 지우기 때문
void GameManager::LoadLevel(string file, bool resetHp) {
	LevelData data;
	if (!LoadLevelFile(file, data)) return; // 못 읽으면 지금 레벨 유지

	ClearLevel();
	level = data;
	currentFile = file;

	// 발판·벽: 카메라를 가리면 반투명해지도록 FadeRenderer를 붙인다
	auto spawnSolid = [&](const string& name, vec3 center, vec3 size, const string& material) {
		Solid s;
		s.obj = Spawn(name, true);
		Transform* tr = s.obj->GetComponent<Transform>();
		tr->SetLocalScale(size.x, size.y, size.z);
		tr->SetLocalPosition(center);
		s.renderer = s.obj->AddComponent<FadeRenderer>(meshChache["FlatCube"], GetMaterial(material));
		s.base = s.center = center;
		s.size = size;
		return s;
	};
	float minX = 1e9f, maxX = -1e9f;
	for (const auto& b : level.blocks) {
		solids.push_back(spawnSolid("Block", b.center, b.size, b.material.empty() ? "Paper" : b.material));
		minX = std::min(minX, b.center.x - b.size.x * 0.5f);
		maxX = std::max(maxX, b.center.x + b.size.x * 0.5f);
	}
	for (const auto& m : level.movers) {
		Solid s = spawnSolid("Mover", m.center, m.size, m.material);
		s.amplitude = m.amplitude;
		s.period = m.period;
		solids.push_back(s);
	}

	// 가시: 판정 상자 + 받침 + 45도 돌린 작은 상자(톱니)
	for (const auto& sp : level.spikes) {
		hazards.push_back({ Box::FromCenterSize(sp.center, sp.size) });
		vec3 baseSize(sp.size.x, sp.size.y * 0.3f, sp.size.z);
		vec3 baseCenter(sp.center.x, sp.center.y - sp.size.y * 0.35f, sp.center.z);
		SpawnCube("Spike_Base", baseCenter, baseSize, "SpikeBase", true);
		int teeth = std::max(1, static_cast<int>(std::round(sp.size.x / 0.5f)));
		float step = sp.size.x / teeth;
		float tooth = step * 0.72f;
		for (int i = 0; i < teeth; ++i) {
			float x = sp.center.x - sp.size.x * 0.5f + step * (i + 0.5f);
			Object* o = SpawnCube("Spike", vec3(x, baseCenter.y + baseSize.y * 0.5f, sp.center.z), vec3(tooth, tooth, sp.size.z * 0.9f), "Spike", true);
			o->GetComponent<Transform>()->SetLocalRotation(0.0f, 0.0f, 45.0f);
		}
	}

	// 책벌레: 몸 두 마디 + 머리 + 앞뒤 눈
	for (const auto& w : level.worms) {
		Worm worm;
		worm.pos = w.foot + vec3(0.0f, 0.3f, 0.0f);
		worm.minX = w.foot.x - w.range;
		worm.maxX = w.foot.x + w.range;
		worm.speed = w.speed;
		worm.root = Spawn("Worm", true);
		Transform* rootTr = worm.root->GetComponent<Transform>();
		rootTr->SetLocalPosition(worm.pos);
		auto seg = [&](vec3 localPos, vec3 size, const string& mat) {
			Object* o = SpawnCube("Worm_Part", vec3(0.0f), size, mat, true);
			Transform* t = o->GetComponent<Transform>();
			t->SetParent(rootTr);
			t->SetLocalPosition(localPos);
			worm.parts.push_back(o);
		};
		seg(vec3(-0.3f, -0.05f, 0.0f), vec3(0.42f, 0.42f, 0.5f), "Worm");
		seg(vec3(0.05f, -0.03f, 0.0f), vec3(0.42f, 0.46f, 0.52f), "Worm");
		seg(vec3(0.36f, 0.02f, 0.0f), vec3(0.34f, 0.54f, 0.56f), "WormHead");
		seg(vec3(0.45f, 0.12f, 0.29f), vec3(0.07f, 0.09f, 0.02f), "Ink");
		seg(vec3(0.45f, 0.12f, -0.29f), vec3(0.07f, 0.09f, 0.02f), "Ink");
		worms.push_back(worm);
	}

	for (const auto& c : level.checkpoints) {
		SpawnCube("Checkpoint_Pole", c + vec3(0.0f, 0.8f, 0.0f), vec3(0.1f, 1.6f, 0.1f), "Pole", true);
		Object* flag = SpawnCube("Checkpoint_Flag", c + vec3(0.3f, 1.38f, 0.0f), vec3(0.5f, 0.36f, 0.06f), "CheckOff", true);
		checkpoints.push_back({ flag, c, false });
	}

	// 페이지 조각: 투명 배경 그림을 붙인 앞뒤 사각형
	for (const auto& f : level.fragments) {
		Object* o = Spawn("Fragment", true);
		Transform* t = o->GetComponent<Transform>();
		t->SetLocalScale(0.72f, 0.96f, 1.0f);
		t->SetLocalPosition(f);
		o->AddComponent<MeshRenderer3D>(meshChache["SpriteQuad"], GetMaterial("PageSprite"));
		pickups.push_back({ o, f, false });
	}

	// 등불(점광원)
	for (const auto& l : level.lamps) {
		Object* o = Spawn("Lamp", true);
		Transform* t = o->GetComponent<Transform>();
		t->SetLocalScale(0.22f, 0.22f, 0.22f);
		t->SetLocalPosition(l.position);
		Light* light = o->AddComponent<Light>();   // 모양은 StoryDecor의 빛무리 그림으로 그린다
		light->lightType = LightType::Point;
		light->lightColor = vec4(1.0f, 0.82f, 0.5f, 1.0f);
		light->intensity = l.intensity;
		levelLights.push_back(light);
	}

	// 분위기(하늘·조명)와 꾸미기. 소품을 만들기 전에 테마를 정해야 색이 맞는다.
	const StoryTheme& theme = decor.ApplyTheme(level.theme);
	sunLight->lightColor = vec4(theme.sunColor, 1.0f);
	sunLight->intensity = theme.sunIntensity;
	fillLight->intensity = theme.fillIntensity;
	camera->backgroundColor = vec4(theme.skyBottom, 1.0f);
	vector<vector<FadeRenderer*>> caps;
	decor.BuildLevel(level, caps);
	for (size_t i = 0; i < caps.size() && i < solids.size(); ++i) solids[i].caps = caps[i];

	if (resetHp) hp = kMaxHp;
	fragments = std::max(0, level.page - 1);
	respawnPoint = level.start + vec3(0.0f, HalfSize().y + kSkin, 0.0f);
	ResetPlayerAt(respawnPoint);
	invuln = 0.0f;
	camFocus = pos;
	state = GameState::Playing;
	stateTimer = 0.0f;
	clock = 0.0f;
	cout << "[게임] " << file << " 시작 (실행 후 " << (startTime >= 0.0 ? glfwGetTime() - startTime : 0.0) << "초)" << endl;
}

// ======================================================================
//  화면 흐름
// ======================================================================

void GameManager::StartGame(string file) {
	playTime = 0.0f;
	LoadLevel(file, true);
}

void GameManager::GoToTitle() {
	ClearLevel();
	ResetPlayerAt(vec3(0.0f, -100.0f, 0.0f));
	state = GameState::Title;
	selectedButton = 0;
}

std::vector<MenuButton> GameManager::GetMenuButtons() const {
	vector<pair<string, MenuAction>> items;
	float startY = 0.0f;
	float H = static_cast<float>(screenSize.height), W = static_cast<float>(screenSize.width);
	switch (state) {
	case GameState::Title:    items = { { "START", MenuAction::Start }, { "QUIT", MenuAction::Quit } }; startY = H * 0.60f; break;
	case GameState::Paused:   items = { { "RESUME", MenuAction::Resume }, { "TITLE", MenuAction::ToTitle } }; startY = H * 0.48f; break;
	case GameState::GameOver: items = { { "RETRY", MenuAction::Retry }, { "TITLE", MenuAction::ToTitle } }; startY = H * 0.58f; break;
	case GameState::Ending:   items = { { "TITLE", MenuAction::ToTitle }, { "QUIT", MenuAction::Quit } }; startY = H * 0.70f; break;
	default: return {};
	}
	float ui = std::max(0.7f, H / 1080.0f);
	float bw = 300.0f * ui, bh = 64.0f * ui, gap = 20.0f * ui;
	vector<MenuButton> buttons;
	for (size_t i = 0; i < items.size(); ++i)
		buttons.push_back({ items[i].first, (W - bw) * 0.5f, startY + i * (bh + gap), bw, bh, items[i].second });
	return buttons;
}

// 메뉴: 마우스로 가리키고 클릭, 또는 W/S·방향키로 고르고 Enter·Space
void GameManager::HandleMenu() {
	vector<MenuButton> buttons = GetMenuButtons();
	if (buttons.empty()) return;
	InputManager& input = gameObject->ctx.inputManager;
	int n = static_cast<int>(buttons.size());
	selectedButton = std::clamp(selectedButton, 0, n - 1);

	float mx = (input.GetMouseX() + 1.0f) * 0.5f * screenSize.width;
	float my = (1.0f - input.GetMouseY()) * 0.5f * screenSize.height;
	int hovered = -1;
	for (int i = 0; i < n; ++i) {
		const MenuButton& b = buttons[i];
		if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) hovered = i;
	}
	static float lastMx = -1.0f, lastMy = -1.0f;
	if ((mx != lastMx || my != lastMy) && hovered >= 0 && hovered != selectedButton) { selectedButton = hovered; Sound::Play("select"); }
	lastMx = mx; lastMy = my;

	if (input.GetKeyDown(GLFW_KEY_W) || input.GetKeyDown(GLFW_KEY_UP)) { selectedButton = (selectedButton + n - 1) % n; Sound::Play("select"); }
	if (input.GetKeyDown(GLFW_KEY_S) || input.GetKeyDown(GLFW_KEY_DOWN)) { selectedButton = (selectedButton + 1) % n; Sound::Play("select"); }

	bool click = input.GetKeyDown(GLFW_MOUSE_BUTTON_LEFT) && hovered >= 0;
	if (click) selectedButton = hovered;
	if (click || input.GetKeyDown(GLFW_KEY_ENTER) || input.GetKeyDown(GLFW_KEY_SPACE)) ActivateMenu(buttons[selectedButton].action);
}

void GameManager::ActivateMenu(MenuAction action) {
	Sound::Play("select");
	switch (action) {
	case MenuAction::Start:   StartGame(kStageFiles[0]); break;
	case MenuAction::Resume:  state = pausedFrom; break;
	case MenuAction::Retry:   LoadLevel(currentFile, true); break;
	case MenuAction::ToTitle: GoToTitle(); break;
	case MenuAction::Quit: {
		GLFWwindow* window = glfwGetCurrentContext();
		if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	}
	}
}

// 캐릭터 시안 바꾸기: 종이 인형 재질의 텍스처 유닛만 바꾼다
void GameManager::SetCharacterStyle(int style) {
	characterStyle = ((style % CharacterArt::kCount) + CharacterArt::kCount) % CharacterArt::kCount;
	GetMaterial("DollSprite")->SetInt("spriteTex", ProceduralTextures::kCharacterBase + characterStyle);
	characterMsgTimer = 3.0f;
	cout << "[게임] 캐릭터 시안 " << (characterStyle + 1) << "/" << CharacterArt::kCount << endl;
}

// R: 지금 스테이지 다시, F1~F4: 튜토리얼·스테이지 1~3 (개발·테스트용)
bool GameManager::HandleDebugKeys() {
	InputManager& input = gameObject->ctx.inputManager;
	if (input.GetKeyDown(GLFW_KEY_R) && !currentFile.empty() && state != GameState::Title) { LoadLevel(currentFile, true); return true; }
	const int keys[] = { GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4 };
	for (int i = 0; i < 4; ++i) {
		if (input.GetKeyDown(keys[i])) {
			if (state == GameState::Title) StartGame(kStageFiles[i]);
			else LoadLevel(kStageFiles[i], true);
			return true;
		}
	}
	return false;
}

// ======================================================================
//  매 프레임
// ======================================================================

void GameManager::Update(float dt) {
	realClock += dt;
	double now = glfwGetTime();
	if (startTime < 0.0) startTime = now;
	double elapsed = now - startTime;
	InputManager& input = gameObject->ctx.inputManager;
	if (replay.active && replay.Apply(elapsed, input)) {
		GLFWwindow* window = glfwGetCurrentContext();
		if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
	// F12 또는 POPUP_CAPTURE 시각 → 이번 프레임을 다 그린 뒤 스크린샷 저장
	if (input.GetKeyDown(GLFW_KEY_F12)) captureRequested = true;
	while (nextCapture < captureTimes.size() && captureTimes[nextCapture] <= elapsed) {
		captureRequested = true;
		++nextCapture;
	}
	if (input.GetKeyDown(GLFW_KEY_M)) Sound::SetMuted(!Sound::IsMuted());
	if (input.GetKeyDown(GLFW_KEY_C)) SetCharacterStyle(characterStyle + 1);
	characterMsgTimer = std::max(0.0f, characterMsgTimer - dt);

	blockedTimer = std::max(0.0f, blockedTimer - dt);
	pageReveal = std::max(0.0f, pageReveal - dt);
	shakeTimer = std::max(0.0f, shakeTimer - dt);

	bool menuState = state == GameState::Title || state == GameState::Paused || state == GameState::GameOver || state == GameState::Ending;
	if (menuState) {
		if (state == GameState::Paused && input.GetKeyDown(GLFW_KEY_ESCAPE)) state = pausedFrom;
		else if (!HandleDebugKeys()) HandleMenu();
	}
	else if (!HandleDebugKeys()) {
		if ((state == GameState::Playing || state == GameState::Transition) && input.GetKeyDown(GLFW_KEY_ESCAPE)) {
			pausedFrom = state;
			state = GameState::Paused;
			selectedButton = 0;
		}
		else {
			switch (state) {
			case GameState::Playing:
				clock += dt;
				playTime += dt;
				invuln = std::max(0.0f, invuln - dt);
				UpdateMovers();
				UpdatePlayer(dt);
				UpdateWorms(dt);
				CheckHazardsAndPickups();
				if (state == GameState::Playing) UpdateGauge(dt);
				if (state == GameState::Playing && input.GetKeyDown(GLFW_KEY_E)) TrySwitchView();
				break;
			case GameState::Transition:
				playTime += dt;
				UpdateTransition(dt); // 전환하는 동안은 게임을 멈춘다
				break;
			case GameState::StageClear:
				stateTimer -= dt;
				if (stateTimer <= 0.0f) {
					if (!level.next.empty()) LoadLevel("Levels/" + level.next, true); // 다음 스테이지에서 HP 회복
					else { state = GameState::Ending; selectedButton = 0; }
					pageReveal = kPageRevealTime;   // 넘긴 책장이 마저 넘어가며 다음 장이 드러난다
				}
				break;
			default:
				break;
			}
		}
	}

	UpdateCamera(dt);
	SyncVisuals(dt);
	UpdateWindowTitle();

	if (debugLog && elapsed >= nextLogTime) {
		nextLogTime = elapsed + 0.25;
		const char* stateName[] = { "Title", "Playing", "Transition", "Paused", "StageClear", "GameOver", "Ending" };
		cout << fixed;
		cout.precision(2);
		cout << "[로그] t=" << elapsed << " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< (mode == ViewMode::View3D ? " 3D" : " 2D") << " viewT=" << viewT << " gauge=" << gauge
			<< " hp=" << hp << " ground=" << groundSolid << " " << stateName[static_cast<int>(state)];
		// 가장 가까운 책벌레 위치 (입력 스크립트 타이밍 맞추기용)
		const Worm* nearest = nullptr;
		for (const auto& w : worms) {
			if (w.alive && (!nearest || std::abs(w.pos.x - pos.x) < std::abs(nearest->pos.x - pos.x))) nearest = &w;
		}
		if (nearest && std::abs(nearest->pos.x - pos.x) < 12.0f) cout << " worm=" << nearest->pos.x << (nearest->dir > 0 ? ">" : "<");
		cout << " clock=" << clock << endl;
		cout << defaultfloat;
		cout.precision(6);
	}
}

void GameManager::OnFrameRendered() {
	if (!captureRequested) return;
	captureRequested = false;
	++captureCount;
	string dir = GetEnvVar("POPUP_CAPTURE_DIR");
	if (dir.empty()) dir = "Screenshots";
	ostringstream name;
	name << dir << "/shot_" << captureCount << ".bmp";
	if (SaveScreenshotBMP(name.str(), screenSize.width, screenSize.height)) cout << "[캡처] " << name.str() << endl;
	else cout << "[캡처] 저장 실패: " << name.str() << endl;
}

void GameManager::ResetPlayerAt(vec3 p) {
	pos = p;
	vel = vec3(0.0f);
	grounded = false;
	groundSolid = -1;
	knockTimer = 0.0f;
	coyote = 0.0f;
	jumpBuffer = 0.0f;
	mode = targetMode = ViewMode::View2D;
	viewT = 0.0f;
	gauge = 1.0f;
	if (state == GameState::Transition) state = GameState::Playing;
}

// 낙사·가시는 HP를 잃고 체크포인트로, 게이지 강제 복귀 실패는 HP 감소 없이 체크포인트로
void GameManager::RespawnAtCheckpoint(bool loseHp) {
	if (loseHp) {
		LoseHp();
		if (state == GameState::GameOver) return;
	}
	ResetPlayerAt(respawnPoint);
	invuln = loseHp ? kInvulnTime : 0.3f;
}

void GameManager::LoseHp() {
	hp = std::max(0, hp - 1);
	Sound::Play("hurt");
	if (hp == 0) {
		state = GameState::GameOver;
		selectedButton = 0;
		cout << "[게임] HP 0 → 게임 오버" << endl;
	}
}

void GameManager::UpdateMovers() {
	const float twoPi = 6.2831853f;
	for (auto& s : solids) {
		if (s.period <= 0.0f) { s.delta = vec3(0.0f); continue; }
		vec3 next = s.base + s.amplitude * std::sin(twoPi * clock / s.period);
		s.delta = next - s.center;
		s.center = next;
		s.obj->GetComponent<Transform>()->SetLocalPosition(next);
	}
}

void GameManager::UpdatePlayer(float dt) {
	InputManager& input = gameObject->ctx.inputManager;

	// 움직이는 발판 위에 서 있으면 같이 움직인다 (2D에서는 깊이 방향은 따라가지 않는다)
	if (groundSolid >= 0 && groundSolid < static_cast<int>(solids.size())) {
		vec3 d = solids[groundSolid].delta;
		if (mode == ViewMode::View2D) d.z = 0.0f;
		pos += d;
	}

	// 올라오는 발판이 착지하는 플레이어를 아래에서 파고들면 위로 올려 태운다
	// (그대로 두면 "이미 겹친 물체는 무시" 규칙 때문에 발판을 뚫고 떨어진다)
	for (size_t i = 0; i < solids.size(); ++i) {
		const Solid& s = solids[i];
		if (s.period <= 0.0f) continue;
		Box b = s.GetBox();
		if (!BoxesOverlap(PlayerBox(), b, mode)) continue;
		float feet = pos.y - HalfSize().y;
		if (feet >= b.max.y - 0.35f && vel.y <= 0.0f) {
			pos.y = b.max.y + HalfSize().y + kSkin;
			vel.y = 0.0f;
			grounded = true;
			groundSolid = static_cast<int>(i);
		}
	}

	float ix = 0.0f, iz = 0.0f;
	if (input.GetKeyPressed(GLFW_KEY_D)) ix += 1.0f;
	if (input.GetKeyPressed(GLFW_KEY_A)) ix -= 1.0f;
	if (mode == ViewMode::View3D) {
		if (input.GetKeyPressed(GLFW_KEY_S)) iz += 1.0f; // 화면 쪽(+z)
		if (input.GetKeyPressed(GLFW_KEY_W)) iz -= 1.0f; // 안쪽(-z)
	}
	if (knockTimer > 0.0f) knockTimer -= dt;
	else vel.x = ix * kMoveSpeed;
	vel.z = iz * kDepthSpeed;
	if (ix > 0.0f) facingRight = true;
	else if (ix < 0.0f) facingRight = false;

	// 점프
	jumpBuffer = input.GetKeyDown(GLFW_KEY_SPACE) ? 0.12f : jumpBuffer - dt;
	coyote = grounded ? 0.08f : coyote - dt;
	if (jumpBuffer > 0.0f && coyote > 0.0f) {
		vel.y = kJumpSpeed;
		jumpBuffer = 0.0f;
		coyote = 0.0f;
		Sound::Play("jump");
	}
	if (vel.y > 0.0f && !input.GetKeyPressed(GLFW_KEY_SPACE)) vel.y -= kGravity * dt; // 짧게 누르면 낮게 뜀
	vel.y = std::max(vel.y - kGravity * dt, -kMaxFallSpeed);

	// 축마다 따로 움직이고 부딪히면 면에 붙인다: x → (3D면 z) → y
	prevBottom = pos.y - HalfSize().y;
	MoveAxis(0, vel.x * dt);
	if (mode == ViewMode::View3D) MoveAxis(2, vel.z * dt);
	grounded = false;
	groundSolid = -1;
	MoveAxis(1, vel.y * dt);
}

void GameManager::MoveAxis(int axis, float amount) {
	if (amount == 0.0f) return;
	vec3 half = HalfSize();

	// 움직이기 전에 이미 겹쳐 있던 물체는 무시한다 (겹친 채 전환됐을 때 빠져나올 수 있게)
	vector<char> ignore(solids.size(), 0);
	Box startBox = PlayerBox();
	for (size_t i = 0; i < solids.size(); ++i) ignore[i] = BoxesOverlap(startBox, solids[i].GetBox(), mode) ? 1 : 0;

	// 한 번에 0.25칸 이하로 나눠 움직여 얇은 발판을 뚫지 않게 한다
	int steps = std::max(1, static_cast<int>(std::ceil(std::abs(amount) / 0.25f)));
	float stepAmount = amount / steps;
	for (int s = 0; s < steps; ++s) {
		pos[axis] += stepAmount;
		bool hit = false;
		for (size_t i = 0; i < solids.size(); ++i) {
			if (ignore[i]) continue;
			Box b = solids[i].GetBox();
			if (!BoxesOverlap(PlayerBox(), b, mode)) continue;
			pos[axis] = (stepAmount > 0.0f) ? b.min[axis] - half[axis] - kSkin : b.max[axis] + half[axis] + kSkin;
			hit = true;
			if (axis == 1) {
				if (stepAmount < 0.0f) { grounded = true; groundSolid = static_cast<int>(i); }
				vel.y = 0.0f;
			}
		}
		if (hit) break;
	}
}

void GameManager::UpdateWorms(float dt) {
	for (auto& w : worms) {
		if (!w.alive) continue;
		w.pos.x += w.dir * w.speed * dt;
		if (w.pos.x > w.maxX) { w.pos.x = w.maxX; w.dir = -1; }
		if (w.pos.x < w.minX) { w.pos.x = w.minX; w.dir = 1; }
	}
}

void GameManager::CheckHazardsAndPickups() {
	Box pb = PlayerBox();

	if (pos.y < level.killY) {
		cout << "[게임] 떨어짐 → HP -1, 체크포인트로" << endl;
		RespawnAtCheckpoint(true);
		return;
	}

	if (invuln <= 0.0f) {
		for (const auto& h : hazards) {
			if (BoxesOverlap(pb, h.box, mode)) {
				cout << "[게임] 가시 → HP -1, 체크포인트로" << endl;
				RespawnAtCheckpoint(true);
				return;
			}
		}
	}

	for (auto& w : worms) {
		if (!w.alive) continue;
		Box wb = w.GetBox();
		if (!BoxesOverlap(pb, wb, mode)) continue;
		if (vel.y < 0.0f && prevBottom >= wb.max.y - 0.2f) {
			// 위에서 밟음 → 처치하고 살짝 튀어 오름
			w.alive = false;
			w.root->isValid = false;
			for (Object* o : w.parts) o->isValid = false;
			vel.y = 8.0f;
			Sound::Play("stomp");
			cout << "[게임] 책벌레를 밟아 물리침" << endl;
		}
		else if (invuln <= 0.0f) {
			cout << "[게임] 책벌레에 닿음 → HP -1" << endl;
			LoseHp();
			if (state == GameState::GameOver) return;
			invuln = kInvulnTime;
			knockTimer = 0.25f;
			vel.x = (pos.x < w.pos.x) ? -6.0f : 6.0f;
			vel.y = 6.0f;
		}
	}

	for (auto& c : checkpoints) {
		if (c.active) continue;
		if (BoxesOverlap(pb, Box::FromCenterSize(c.foot + vec3(0.0f, 1.0f, 0.0f), vec3(1.0f, 2.0f, 2.0f)), mode)) {
			c.active = true;
			c.flag->GetComponent<MeshRenderer3D>()->materials[0] = GetMaterial("CheckOn");
			respawnPoint = c.foot + vec3(0.0f, HalfSize().y + kSkin, 0.0f);
			Sound::Play("checkpoint");
			cout << "[게임] 체크포인트 저장 x=" << c.foot.x << endl;
		}
	}

	for (auto& p : pickups) {
		if (p.taken) continue;
		if (BoxesOverlap(pb, Box::FromCenterSize(p.pos, vec3(0.9f, 1.0f, 1.0f)), mode)) {
			p.taken = true;
			p.obj->isValid = false;
			fragments = std::max(fragments, level.page);
			state = GameState::StageClear;
			stateTimer = 2.0f;
			Sound::Play("clear");
			cout << "[게임] 페이지 조각 " << fragments << "/" << kTotalFragments << " 획득 → 스테이지 클리어" << endl;
		}
	}
}

// 3D에서 줄고 2D에서 찬다. 0이 되면 2D로 강제 복귀 (겹치는 자리면 HP 감소 없이 체크포인트로)
void GameManager::UpdateGauge(float dt) {
	if (mode == ViewMode::View3D) {
		gauge -= kGaugeDrainPerSec * dt;
		if (gauge <= 0.0f) {
			gauge = 0.0f;
			if (CanReturnTo2D(PlayerBox(), SolidBoxes())) {
				BeginTransition(ViewMode::View2D);
			}
			else {
				cout << "[게임] 게이지 0, 2D로 돌아갈 수 없는 자리 → 체크포인트로 (HP 유지)" << endl;
				Sound::Play("blocked");
				RespawnAtCheckpoint(false);
			}
		}
	}
	else {
		gauge = std::min(1.0f, gauge + kGaugeFillPerSec * dt);
	}
}

void GameManager::TrySwitchView() {
	if (mode == ViewMode::View2D) {
		if (gauge < kMinGaugeToEnter3D) { SwitchBlocked(); return; }
		float newZ = ChooseDepthFor3D(PlayerBox(), SolidBoxes(), pos.z);
		if (newZ != pos.z) cout << "[게임] 3D 전환: 발판 깊이로 맞춤 z " << pos.z << " → " << newZ << endl;
		pos.z = newZ;
		BeginTransition(ViewMode::View3D);
	}
	else {
		if (CanReturnTo2D(PlayerBox(), SolidBoxes())) BeginTransition(ViewMode::View2D);
		else SwitchBlocked();
	}
}

void GameManager::SwitchBlocked() {
	blockedTimer = 0.6f;  // HUD 아이콘 빨간색
	shakeTimer = 0.3f;    // 화면 흔들림
	Sound::Play("blocked");
	cout << "[게임] 시점 전환 막힘 x=" << pos.x << endl;
}

void GameManager::BeginTransition(ViewMode to) {
	targetMode = to;
	transitionTimer = 0.0f;
	state = GameState::Transition;
	Sound::Play("switch");
}

void GameManager::UpdateTransition(float dt) {
	transitionTimer += dt;
	float p = std::min(1.0f, transitionTimer / kTransitionTime);
	viewT = (targetMode == ViewMode::View3D) ? p : 1.0f - p;
	if (p >= 1.0f) {
		mode = targetMode;
		state = GameState::Playing;
	}
}

// ======================================================================
//  화면
// ======================================================================

// 추적 카메라: 2D는 정면, 3D는 비스듬한 고정 각도. 전환 중에는 두 자세 사이를 잇는다.
void GameManager::UpdateCamera(float dt) {
	float follow = 1.0f - std::exp(-8.0f * dt);
	camFocus += (pos - camFocus) * follow;

	// 바라보는 점(플레이어 조금 위)은 고정하고, 방향은 회전 보간, 거리는 선형 보간한다.
	// 위치와 회전을 따로 보간하면 전환 도중 화면 중심이 플레이어에서 벗어난다.
	float s = smoothstep(0.0f, 1.0f, viewT);
	vec3 look = camFocus + vec3(0.0f, 1.0f, 0.0f);
	vec3 toEye3D = kCamOffset3D - vec3(0.0f, 1.0f, 0.0f);
	quat rot2D = LookRotation(vec3(0.0f, 0.0f, -1.0f));
	quat rot3D = LookRotation(-toEye3D);
	quat rot = slerp(rot2D, rot3D, s);
	float dist = mix(50.0f, length(toEye3D), s);   // 2D는 멀리서 직교로, 3D는 가까이서 원근으로
	vec3 eye = look - (rot * vec3(0.0f, 0.0f, 1.0f)) * dist;
	if (shakeTimer > 0.0f) eye += vec3(Rand(-1.0f, 1.0f), Rand(-1.0f, 1.0f), 0.0f) * 0.12f;

	Transform* tr = cameraObj->GetComponent<Transform>();
	tr->SetLocalRotation(rot);
	tr->SetLocalPosition(eye);

	// 투영 행렬: 두 행렬을 만들어 BlendProjection(CoreRules)에 넘긴다.
	// 원근 화각은 바라보는 점까지의 거리 dist에서 직교 화면과 같은 크기가 되도록 맞춘다 (돌리 줌).
	float aspect = (camera->aspectRatio > 0.0f) ? camera->aspectRatio : 16.0f / 9.0f; // DrawScene이 매 프레임 갱신
	float halfW = kOrthoHalfHeight * aspect;
	float fov = 2.0f * std::atan(kOrthoHalfHeight / dist);
	mat4 ortho2D = ortho(-halfW, halfW, -kOrthoHalfHeight, kOrthoHalfHeight, camera->nearPlane, camera->farPlane);
	mat4 persp3D = perspective(fov, aspect, camera->nearPlane, camera->farPlane);
	camera->projectionOverride = BlendProjection(ortho2D, persp3D, viewT);

	OnTopRenderer::drawOnTop = (viewT < 0.5f);
}

void GameManager::SyncVisuals(float dt) {
	// 플레이어: 방향을 바꾸면 종이처럼 y축으로 180도 뒤집힌다
	float targetYaw = facingRight ? 0.0f : 180.0f;
	float stepYaw = 900.0f * dt;
	if (std::abs(targetYaw - visualYaw) <= stepYaw) visualYaw = targetYaw;
	else visualYaw += (targetYaw > visualYaw) ? stepYaw : -stepYaw;
	Transform* pt = playerRoot->GetComponent<Transform>();
	pt->SetLocalRotation(0.0f, visualYaw, 0.0f);
	pt->SetLocalPosition(pos);
	bool blinkOn = invuln <= 0.0f || std::fmod(clock * 12.0f, 2.0f) < 1.0f; // 무적 중 깜빡임
	bool showPlayer = blinkOn && state != GameState::Title && !currentFile.empty();
	for (MeshRenderer3D* r : playerRenderers) r->isValid = showPlayer;

	// 3D에서만 발밑 그림자
	float groundTop = 0.0f;
	if (showPlayer && viewT >= 0.5f && FindGroundTop(groundTop)) {
		shadow->isValid = true;
		shadow->GetComponent<Transform>()->SetLocalPosition(pos.x, groundTop + 0.02f, pos.z);
	}
	else {
		shadow->isValid = false;
	}

	for (auto& w : worms) {
		if (!w.alive) continue;
		Transform* t = w.root->GetComponent<Transform>();
		t->SetLocalRotation(0.0f, w.dir > 0 ? 0.0f : 180.0f, 0.0f);
		t->SetLocalPosition(w.pos.x, w.pos.y + 0.04f * std::sin(clock * 10.0f), w.pos.z); // 꿈틀
	}

	for (auto& p : pickups) {
		if (p.taken) continue;
		Transform* t = p.obj->GetComponent<Transform>();
		t->SetLocalRotation(0.0f, clock * 90.0f, 6.0f);
		t->SetLocalPosition(p.pos + vec3(0.0f, 0.15f * std::sin(clock * 2.5f), 0.0f));
	}

	// 팝업북 꾸미기: 하늘, 해·달·별, 꽃잎, 팝업 소품, 배경 판 벌어짐
	decor.Update(dt, pos, camFocus, viewT, realClock, state != GameState::Title && !currentFile.empty());

	UpdateWallFade(dt);
}

// 3D에서 카메라와 플레이어 사이를 가리는 발판·벽은 반투명하게
void GameManager::UpdateWallFade(float dt) {
	Transform* camTr = cameraObj->GetComponent<Transform>();
	vec3 eye = camTr->worldPosition;
	bool check = viewT >= 0.5f && state != GameState::Title;
	float rate = std::min(1.0f, 8.0f * dt);
	for (auto& s : solids) {
		if (!s.renderer) continue;
		float target = 1.0f;
		if (check) {
			Box b = s.GetBox();
			if (SegmentHitsBox(eye, pos, b) || SegmentHitsBox(eye, pos + vec3(0.0f, 0.5f, 0.0f), b)) target = 0.3f;
		}
		s.renderer->alpha += (target - s.renderer->alpha) * rate;
		for (FadeRenderer* c : s.caps) c->alpha = s.renderer->alpha;   // 위에 올린 잔디도 같이
	}
}

void GameManager::UpdateWindowTitle() {
	ostringstream ss;
	if (state == GameState::Title || currentFile.empty()) {
		ss << U8(u8"팝업북 · 타이틀 | Enter/클릭: 시작 | M: 소리 ") << (Sound::IsMuted() ? U8(u8"켜기") : U8(u8"끄기"));
	}
	else {
		ss << U8(u8"팝업북 · ") << level.name
			<< " | HP " << hp << "/" << kMaxHp
			<< " | " << (mode == ViewMode::View3D ? "3D" : "2D")
			<< U8(u8" 게이지 ") << static_cast<int>(std::round(gauge * 100.0f)) << "%"
			<< U8(u8" | 조각 ") << fragments << "/" << kTotalFragments;
		string msg = CurrentMessage();
		if (!msg.empty()) ss << " | " << msg;
	}

	string title = ss.str();
	if (title != lastTitle) {
		GLFWwindow* window = glfwGetCurrentContext();
		if (window) glfwSetWindowTitle(window, title.c_str());
		lastTitle = title;
	}
}

std::string GameManager::CurrentMessage() const {
	switch (state) {
	case GameState::Paused:   return U8(u8"일시정지 (ESC: 계속)");
	case GameState::GameOver: return U8(u8"게임 오버! 다시 시작하거나 타이틀로");
	case GameState::Ending:   return U8(u8"마지막 장 완성! 플레이해 주셔서 고마워요");
	case GameState::StageClear:
		return level.next.empty() ? U8(u8"마지막 조각을 찾았어요!") : U8(u8"스테이지 클리어! 다음 페이지로 넘어가요");
	default: break;
	}
	if (characterMsgTimer > 0.0f) {
		return U8(u8"캐릭터 시안 ") + to_string(characterStyle + 1) + "/" + to_string(CharacterArt::kCount) + ": "
			+ CharacterArt::Name(characterStyle) + U8(u8" (C: 다음 시안)");
	}
	if (blockedTimer > 0.0f) return U8(u8"여기서는 시점을 바꿀 수 없어요");
	for (const auto& h : level.hints) {
		if (pos.x >= h.x0 && pos.x < h.x1) return h.text;
	}
	return "";
}

Box GameManager::PlayerBox() const {
	return Box::FromCenterSize(pos, kPlayerSize);
}

std::vector<Box> GameManager::SolidBoxes() const {
	vector<Box> boxes;
	boxes.reserve(solids.size());
	for (const auto& s : solids) boxes.push_back(s.GetBox());
	return boxes;
}

// 발밑(3D 기준)에서 가장 높은 발판 윗면을 찾는다. 그림자 위치용.
bool GameManager::FindGroundTop(float& outTop) const {
	vec3 half = HalfSize();
	float feet = pos.y - half.y;
	Box column{ vec3(pos.x - half.x * 0.6f, feet - 60.0f, pos.z - half.z * 0.6f),
				vec3(pos.x + half.x * 0.6f, feet + 0.05f, pos.z + half.z * 0.6f) };
	bool found = false;
	float best = -1e9f;
	for (const auto& s : solids) {
		Box b = s.GetBox();
		if (b.max.y > feet + 0.05f) continue;
		if (BoxesOverlap(column, b, ViewMode::View3D) && b.max.y > best) {
			best = b.max.y;
			found = true;
		}
	}
	outTop = best;
	return found;
}
