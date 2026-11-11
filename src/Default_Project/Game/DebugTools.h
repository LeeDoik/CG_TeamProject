#pragma once
#include <string>
#include <vector>

class InputManager;

// 환경 변수 값 (없으면 빈 문자열)
std::string GetEnvVar(const char* name);

// 지금 그려진 화면(기본 프레임버퍼)을 24비트 BMP 파일로 저장한다. 폴더가 없으면 만든다.
// 보고서용 캡처, 제안서의 "전환 중간 프레임 캡처" 검증에 쓴다.
bool SaveScreenshotBMP(const std::string& path, int width, int height);

// 테스트용 입력 재생.
// 환경 변수 POPUP_REPLAY에 스크립트 경로를 주면, 실행 후 경과 시간에 맞춰 키를 누르고 뗀다.
// 실제 키보드를 쓰지 않으므로 창을 숨긴 채(POPUP_HEADLESS) 자동으로 플레이 테스트를 돌릴 수 있다.
//   스크립트 한 줄:  시간(초)  키  down|up      예) 1.5 D down
//                    시간(초)  quit             예) 20 quit
//   키 이름: A D W S E SPACE R M ESC ENTER UP DOWN F1~F4
class InputReplay {
public:
	bool active = false;

	bool Load(const std::string& path);
	// now 시각까지의 이벤트를 InputManager에 넣는다. quit 이벤트를 만나면 true.
	bool Apply(double now, InputManager& input);

private:
	struct Event { double time; int key; bool down; bool quit; };
	std::vector<Event> events;
	size_t next = 0;
};
