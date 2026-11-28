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
}
