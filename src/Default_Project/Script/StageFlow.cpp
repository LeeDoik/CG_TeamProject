#include "StageFlow.h"
#include "Object.h"
#include "BoxCollider.h"
#include "Light.h"
#include "LevelLoader.h"
#include "PageFragment.h"
#include "PlayerController.h"
#include "PlayerHealth.h"
#include "DimensionSwitch.h"
#include "PlayerSpriteAnimator.h"
#include "ViewTransition.h"
#include <algorithm>

namespace {
	// next는 지금 레벨 파일과 같은 폴더 기준이다(PR #4 형식 규칙). "Levels/a.txt"와 "b.txt" → "Levels/b.txt"
	string SameFolder(const string& currentFile, const string& next) {
		size_t slash = currentFile.find_last_of("/\\");
		if (slash == string::npos) return next;
		return currentFile.substr(0, slash + 1) + next;
	}
}

void StageFlow::Start() {
	// 레벨이 바뀌어도 남는 것: 시점 전환 카메라와 플레이어 컴포넌트
	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!view) view = obj->GetComponent<ViewTransition>();
		if (!player) player = obj->GetComponent<PlayerController>();
	}
	if (player) {
		Object* p = player->gameObject;
		health = p->GetComponent<PlayerHealth>();
		dimension = p->GetComponent<DimensionSwitch>();
		animator = p->GetComponent<PlayerSpriteAnimator>();
		playerBox = p->GetComponent<BoxCollider>();
	}
	if (!player || !playerBox) cout << "[StageFlow] 장면에 PlayerController와 BoxCollider가 붙은 플레이어가 없습니다." << endl;

	// 첫 레벨은 Game_Scene이 게임 루프 전에 불러 두었다
	FindLevel();
	BeginStage();
}

// 상태마다 한 프레임에 하는 일 (StageFlow.h의 상태 설명)
//  클리어 시간(playTime)은 Playing일 때만 늘어난다.
//   - 넣는 것: 시점 전환 0.5초. 전환은 플레이어가 고른 행동이라 많이 바꿀수록 오래 걸리는 것이 맞다. 게임 오버 뒤 다시 하는 시간도 넣는다
//   - 빼는 것: STAGE CLEAR 2초 대기, 레벨을 바꾸는 2프레임, 엔딩. 플레이어가 줄일 수 없는 연출이라 뺀다
//   - [연결 지점] 일시정지(ESC, 화면 흐름 담당)가 생기면 그동안도 빼야 한다
void StageFlow::Update(float deltaTime) {
	stageTime += deltaTime;

	if (state == StageState::Playing) {
		playTime += deltaTime;

		// 시점 전환 0.5초 동안은 플레이어가 멈춰 있으므로(기획서 4.1) 조각 검사도 쉰다
		if (!playerBox || (view && view->IsTransitioning())) return;

		for (BoxCollider* f : fragments) {
			if (f->gameObject->isValid && Touches(f)) {
				ClearStage(f);
				return;
			}
		}
	}
	else if (state == StageState::Cleared) {
		clearWaitLeft = std::max(0.0f, clearWaitLeft - deltaTime);
		if (clearWaitLeft <= 0.0f) LoadNextLevel();
	}
	else if (state == StageState::Loading) {
		// 새 깃발·조각이 장면에 들어온 뒤에야 찾을 수 있다
		if (IsNewLevelReady()) {
			FindLevel();
			if (health) health->ResetForNewLevel();	// HP 3, 새 깃발 찾기, 새 start를 마지막 체크포인트로 (기획서 3.3)
			BeginStage();
		}
	}
	// Ending: 할 일이 없다. 게임은 멈춰 있고 엔딩 값만 내준다
}

// 장면에서 지금 레벨의 정보("Level" 오브젝트의 LevelInfo)와 조각 상자를 찾는다
void StageFlow::FindLevel() {
	level = nullptr;
	fragments.clear();
	for (auto& obj : gameObject->ctx.Hierarchy) {
		if (!obj->isValid) continue;
		if (LevelInfo* info = obj->GetComponent<LevelInfo>()) level = info;
		if (obj->GetComponent<PageFragment>()) {
			if (BoxCollider* box = obj->GetComponent<BoxCollider>()) fragments.push_back(box);
		}
	}
}

// 스테이지 시작 (기획서 7.2 "스테이지 시작 때 PAGE n + 영문 부제를 약 2.8초 보여 준다")
//  레벨 값을 옮겨 두고 스테이지 시간을 0부터 잰다. LevelInfo는 다음 레벨로 갈 때 지워지므로 값을 복사해 둔다.
void StageFlow::BeginStage() {
	if (level) {
		levelFile = level->data.file;
		nextFile = level->data.next;
		page = level->data.page;
		stageName = level->data.name;
		subtitle = level->data.subtitle;
	}
	else cout << "[스테이지] 경고: 장면에 레벨(Level 오브젝트)이 없습니다. Game_Scene.h의 levelFile을 확인하세요" << endl;
	if (fragments.empty()) cout << "[스테이지] 경고: 레벨에 조각(fragment)이 없어 이 스테이지는 클리어할 수 없습니다" << endl;

	state = StageState::Playing;
	stageTime = 0.0f;
	++stageStartCount;
	cout << "[스테이지] PAGE " << page << " " << subtitle << " 시작: " << levelFile
		<< " (조각 " << GetCollectedPages() << "/" << totalPages << ")" << endl;
	// [연결 지점] "PAGE n" + 부제 리본(UI): GetStageStartCount()가 바뀐 프레임부터 GetStageTime()이 2.8초가 될 때까지
}

// 플레이어가 조각에 닿았나 (기획서 4.3 차원별 규칙: 2D는 x·y, 3D는 x·y·z 구간이 모두 겹쳐야 한다)
//  체크포인트 깃발(PlayerHealth::TouchCheckpoints)은 2D·3D 모두 깊이를 무시하지만, 조각은 3D에서 깊이도 맞아야 닿는다.
//  기획서 3.1 핵심 루프는 알맞은 시점을 골라 길을 찾아가는 것이고, 조각은 그 길 끝의 목표다
//  (6장 "받침대는 2D로 올라 조각 4", "게이지 한 번으로 끝까지 → 조각 3"). 3D에서 깊이를 무시하면 깊이가 다른 줄에 놓인
//  조각을 그 줄까지 가지 않고 얻고, 3D 화면에서는 떨어져 있는데 얻은 것으로 보인다. 깃발은 지나가며 닿는 표시라 깊이를 무시했다.
//  2D에서는 플레이어 충돌 상자가 이미 z로 100칸 늘어나 있어(PlayerController::SetDepthIgnored) z도 늘 겹치지만, 규칙이 보이게 모드로 나눠 적었다.
//  상자에 회전이 없어서 월드 AABB(minPos/maxPos) 구간을 그대로 비교한다(TouchCheckpoints와 같은 방식).
bool StageFlow::Touches(BoxCollider* f) const {
	bool overlapX = playerBox->maxPos.x > f->minPos.x && playerBox->minPos.x < f->maxPos.x;
	bool overlapY = playerBox->maxPos.y > f->minPos.y && playerBox->minPos.y < f->maxPos.y;
	bool overlapZ = playerBox->maxPos.z > f->minPos.z && playerBox->minPos.z < f->maxPos.z;
	bool in3D = view && view->GetMode() == ViewMode::View3D;
	return overlapX && overlapY && (overlapZ || !in3D);
}

// 조각 획득 = 스테이지 클리어 (기획서 3.3 "조각을 얻으면 STAGE CLEAR가 나오고, 2초 뒤 책장이 넘어가며 다음 스테이지로 간다")
//  - 조각을 숨긴다: isValid를 끄면 엔진이 그리지 않고 Update·충돌 검사에서도 뺀다. 오브젝트는 다음 레벨로 갈 때 지운다
//  - 모은 조각은 page 번호로 센다. 같은 page를 다시 얻어도 한 번만 센다. page가 없거나(0) 1~4 밖이면 세지 않는다
//  - Cleared가 되면 IsStopped()가 true라서 플레이어·게이지·HP 검사·움직이는 발판이 멈춘다
void StageFlow::ClearStage(BoxCollider* fragment) {
	fragment->gameObject->isValid = false;

	if (page >= 1 && page <= totalPages) collectedPages.insert(page);
	else cout << "[조각] 경고: 이 레벨의 page(" << page << ")가 1~" << totalPages << " 밖이라 모은 조각 수에 넣지 않습니다" << endl;

	++clearCount;
	clearWaitLeft = clearWait;
	state = StageState::Cleared;
	if (animator) animator->SetOverrideState(PlayerSpriteState::Clear);	// 클리어 그림 (PlayerSpriteAnimator의 [연결 지점])

	cout << "[조각] PAGE " << page << " 획득 → STAGE CLEAR (조각 " << GetCollectedPages() << "/" << totalPages << "), "
		<< clearWait << "초 뒤 " << (nextFile.empty() ? "엔딩" : nextFile) << endl;
	// [연결 지점] 반짝이·STAGE CLEAR 띠(UI), pickup·clear 효과음(소리): GetClearCount()가 바뀐 프레임에. 남은 시간은 GetClearWaitLeft()
}

// 다음 레벨로 (기획서 3.3 "2초 뒤 책장이 넘어가며 다음 스테이지로 간다")
//  1. 플레이어의 속도·발판 기억을 지운다.
//  2. next 파일을 불러온다. 새 오브젝트는 pendingHierarchy에 쌓이고 엔진이 프레임 끝에 장면에 넣는다(LevelLoader.h 설명).
//     LevelLoader가 플레이어를 새 start로 옮긴다. 파일을 열지 못하면 지금 레벨을 그대로 두고 엔딩으로 간다(이유는 콘솔에 나온다).
//  3. 지난 레벨 오브젝트를 지운다. 카메라·조명·플레이어·Stage는 레벨 파일로 만든 것이 아니라서 남는다.
//  4. 나머지를 새 레벨 처음 상태로: 2D, 게이지 100%, 클리어 그림 끄기.
//     HP 3·체크포인트 초기화는 새 깃발이 장면에 들어간 뒤에 한다(Update의 Loading).
void StageFlow::LoadNextLevel() {
	if (nextFile.empty()) {
		BeginEnding();
		return;
	}

	string path = SameFolder(levelFile, nextFile);
	vector<Object*> oldObjects;
	if (level) oldObjects = level->objects;		// 새 레벨을 만들기 전에 받아 둔다

	// 먼저 제자리에서 Respawn을 불러 플레이어의 속도와 서 있던 발판 기억을 지운다
	// (지난 레벨 발판을 가리키는 포인터가 PlayerController에 남지 않게). 새 start로는 LevelLoader가 옮긴다
	if (player) player->Respawn(player->GetFootPosition());

	LevelLoader loader{};
	if (!loader.Load(gameObject->ctx, path)) {
		cout << "[스테이지] 다음 레벨을 열지 못해 엔딩으로 갑니다: " << path << endl;
		BeginEnding();
		return;
	}

	RemoveLevelObjects(oldObjects);
	level = nullptr;
	fragments.clear();

	if (dimension) dimension->RefillGauge();
	if (view) view->ResetTo2D();
	if (animator) animator->ClearOverrideState();

	++levelChangeCount;
	state = StageState::Loading;
	cout << "[스테이지] 다음 레벨로: " << path << endl;
	// [연결 지점] 책장 넘김(UI): GetLevelChangeCount()가 바뀐 프레임에 시작한다
}

// 지난 레벨 오브젝트를 지운다 (엔진 Object::Destroy)
//  Destroy는 프레임 끝에 지우므로(main.cpp) 그 사이 이 프레임의 남은 Update·충돌 검사·그리기에서 빠지게 isValid부터 끈다.
//  엔진이 지울 때 정리하지 않는 포인터 두 가지는 여기서 정리한다.
//   - ctx.lights: 등불에 Light가 붙으면, 지운 Light를 가리키는 포인터가 남아 그릴 때 망가진다
//   - 충돌 상자의 collidingColliders(엔진이 "닿아 있는 상자"를 적어 두는 목록): 지운 상자 주소가 플레이어 쪽에 남으면,
//     새 레벨 상자가 우연히 같은 주소에 생겼을 때 엔진이 이미 닿아 있다고 착각한다. 레벨 상자는 모두 새것이라 다 비워도 된다
void StageFlow::RemoveLevelObjects(const vector<Object*>& objects) {
	for (Object* obj : objects) {
		obj->isValid = false;
		obj->Destroy();
	}

	erase_if(gameObject->ctx.lights, [&objects](Light* light) {
		return find(objects.begin(), objects.end(), light->gameObject) != objects.end();
	});

	for (Collider* col : gameObject->ctx.CollisionObjects) col->collidingColliders.clear();
}

// 새 레벨이 장면에 다 들어왔나 (Loading)
//  엔진 순서(main.cpp): 새 오브젝트는 프레임 끝에 pendingHierarchy → Hierarchy로 옮겨진다. 다음 프레임 Start에서
//  충돌 상자가 pendingCollisionObjects에 등록을 신청하고, 그 프레임 끝에 CollisionObjects로 옮겨진다(BoxCollider::Start).
//  두 대기 목록이 모두 비어야 새 깃발·조각을 찾을 수 있고 플레이어가 새 발판을 밟는다. 불러온 뒤 2프레임째에 참이 된다.
bool StageFlow::IsNewLevelReady() const {
	AppContext& ctx = gameObject->ctx;
	return ctx.pendingHierarchy.empty() && ctx.pendingCollisionObjects.empty();
}

// 엔딩 (기획서 3.3 "엔딩에서는 모은 조각(4/4)과 클리어 시간을 보여 준다")
void StageFlow::BeginEnding() {
	state = StageState::Ending;
	int seconds = static_cast<int>(playTime);
	cout << "[엔딩] 조각 " << GetCollectedPages() << "/" << totalPages << ", 클리어 시간 "
		<< seconds / 60 << "분 " << seconds % 60 << "초 (" << playTime << "초)" << endl;
	// [연결 지점] 엔딩 화면(화면 흐름·UI): IsEnding()이면 GetCollectedPages()/GetTotalPages()와 GetPlayTime()을 보여 준다
}

Component* StageFlow::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	StageFlow* sf = obj->AddComponent<StageFlow>();
	if (props.contains("clearWait")) sf->clearWait = props["clearWait"];
	if (props.contains("totalPages")) sf->totalPages = props["totalPages"];
	return sf;
}

REGISTER_COMPONENT(StageFlow)
