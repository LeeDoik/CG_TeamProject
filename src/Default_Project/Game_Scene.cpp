#include "Game_Scene.h"
#include "Common.h"
#include "SceneLoader.h"

Game_Scene::Game_Scene(ScreenSize screenSize) {
	name = "Game_Scene";
	this->screenSize = screenSize;
}

void Game_Scene::LoadScene(AppContext& ctx) {
	SceneLoader sceneLoader{};
	sceneLoader.LoadScene(ctx, "Game_Scene.json");
}
