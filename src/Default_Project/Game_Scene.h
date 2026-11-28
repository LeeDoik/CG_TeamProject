#pragma once
#include "Scene.h"

// 팝업북 게임 씬
// 카메라·조명·플레이어는 Game_Scene.json, 지형·적·장치는 레벨 파일(Levels/*.txt, 기획서 6.7)에서 만든다.
// 엔진 기본 씬(Default_Scene)은 그대로 둔다.
class Game_Scene : public Scene {
public:
	// 처음에 불러올 레벨. 다른 레벨을 보려면 이 줄만 바꾼다. 비워 두면 JSON만 불러온다
	//  rules_test.txt: 충돌·게이지·전환 조건·HP·체크포인트·낙사 시험 레벨 / test.txt: 레벨 로더 시험 레벨(틀린 줄 포함)
	string levelFile = "Levels/rules_test.txt";

	Game_Scene(ScreenSize screenSize);

	void LoadScene(AppContext& ctx) override;
};
