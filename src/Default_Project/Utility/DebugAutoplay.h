#pragma once
#include <string>
#include <vector>
#include <gl/glew.h>
#include <gl/glfw3.h>
using namespace std;

struct AppContext;

// 테스트·캡처용 자동 재생. 환경 변수를 줄 때만 켜지고, 평소 실행에는 아무 영향이 없다.
//  POPUP_HEADLESS=1          창을 숨긴 채 실행한다 (다른 프로그램의 포커스·키 입력을 건드리지 않는다)
//  POPUP_FIXED_DT=0.0166667  매 프레임 같은 시간 간격으로 진행한다 (실행할 때마다 같은 결과)
//  POPUP_AUTOPLAY=<파일>      아래 형식의 스크립트대로 키를 누르고 화면을 PNG로 저장한다
//  POPUP_SHOT_DIR=<폴더>      캡처 저장 폴더 (기본: Screenshots, .gitignore에 있음)
//
//  스크립트 한 줄: <프레임 번호> <명령> [인자]   (# 뒤는 주석, 프레임은 0부터)
//    120 down D        D 키를 누른 상태로 둔다 (키: A D W S E SPACE)
//    180 up D          D 키를 뗀다
//    150 shot a.png    그 프레임 화면을 저장한다 (플레이어 그림 상태도 콘솔에 찍는다)
//    151 log           플레이어 그림 상태만 콘솔에 찍는다
//    400 quit          종료한다
//  자동 재생 중에는 플레이어 그림이 바뀔 때마다 콘솔에 한 줄씩 찍는다.
class DebugAutoplay {
public:
	bool headless = false;	// 창 숨김
	double fixedDt = 0.0;	// 0보다 크면 고정 시간 간격

	void Init();								// 환경 변수를 읽는다 (창을 만들기 전에 부른다)
	void ApplyInput(AppContext& ctx);			// 이번 프레임의 키 명령 (glfwPollEvents 뒤에 부른다)
	void Capture(GLFWwindow* window, AppContext& ctx);	// 이번 프레임 캡처 (다 그린 뒤, 버퍼 교체 전에 부른다)
	void EndFrame(GLFWwindow* window, AppContext& ctx);	// 프레임 번호를 넘기고 quit를 처리한다

private:
	struct Command {
		long frame;
		string op;
		string arg;
	};
	vector<Command> commands;
	long frame = 0;
	string shotDir = "Screenshots";
	string lastSpriteFrame;
	bool lastFacingLeft = false;

	void PrintPlayerState(AppContext& ctx, const string& label);
};
