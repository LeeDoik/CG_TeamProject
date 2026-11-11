#pragma once
#include "Scene.h"

// 팝업북 게임 씬: 게임용 메시·재질을 준비하고 GameManager 오브젝트를 만든다.
// 레벨 배치는 Levels/*.txt 파일에서 읽는다 (GameManager::LoadLevel).
class StageScene : public Scene {
public:
	StageScene(ScreenSize screenSize);

	void LoadScene(AppContext& ctx) override;
};
