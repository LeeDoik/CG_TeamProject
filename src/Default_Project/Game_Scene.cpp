#include "Game_Scene.h"
#include "Common.h"
#include "SceneLoader.h"
#include "LevelLoader.h"

Game_Scene::Game_Scene(ScreenSize screenSize) {
	name = "Game_Scene";
	this->screenSize = screenSize;
}

void Game_Scene::LoadScene(AppContext& ctx) {
	// 1. 카메라·조명·플레이어
	SceneLoader sceneLoader{};
	sceneLoader.LoadScene(ctx, "Game_Scene.json");

	// 2. 지형·적·장치. 레벨에 start가 있으면 플레이어를 그 자리로 옮긴다
	if (!levelFile.empty()) {
		LevelLoader levelLoader{};
		levelLoader.Load(ctx, levelFile);
	}

	// 3. LevelLoader는 새 오브젝트를 pendingHierarchy에 넣는다(게임 도중 다음 레벨을 불러올 때를 위해, LevelLoader.h 설명).
	//    지금은 게임 루프 전이라 바로 Hierarchy로 옮긴다. 그래야 첫 프레임 Start에서 PlayerHealth가 깃발과 레벨 정보를 찾는다.
	for (auto& obj : ctx.pendingHierarchy) ctx.Hierarchy.push_back(move(obj));
	ctx.pendingHierarchy.clear();
}
