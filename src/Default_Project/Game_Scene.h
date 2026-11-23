#pragma once
#include "Scene.h"

// 팝업북 게임 씬 (1단계: 플레이어 이동·점프, E로 2D/3D 전환 시험용 지형)
// 씬 내용은 Game_Scene.json에 있다. 엔진 기본 씬(Default_Scene)은 그대로 둔다.
class Game_Scene : public Scene {
public:
	Game_Scene(ScreenSize screenSize);

	void LoadScene(AppContext& ctx) override;
};
