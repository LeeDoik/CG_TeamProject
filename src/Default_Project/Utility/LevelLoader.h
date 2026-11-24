#pragma once
#include <string>
#include <vector>
#include <gl/glm/glm.hpp>
#include "Component.h"
#include "AppContext.h"
using namespace std;
using namespace glm;

// ======================================================================
//  레벨 파일 로더 (기획서 6.7 "레벨 파일 형식")
//
//  Levels/*.txt를 한 줄씩 읽어 LevelData에 모으고(Parse), 장면 오브젝트를 만든다(Build).
//  파일 형식은 기획서 6.7을 그대로 따른다. 파서 코드는 AI가 작성을 도왔다(docs/AI_활용_기록.md).
//
//  여기서 하지 않는 것: 충돌 규칙, 2D/3D 전환 조건, 3D 게이지,
//  가시·책벌레·체크포인트·조각·등불·힌트·낙사의 동작.
//  담당자가 LevelLoader.cpp의 [연결 지점] 주석 자리에 붙인다.
// ======================================================================

// block, spike: 중심과 크기
struct LevelBox {
	vec3 center;
	vec3 size;			// w h d
	string material;	// 재질 이름. 비어 있으면 기본 재질
	int line;			// 파일 줄 번호 (경고 메시지, 오브젝트 이름에 쓴다)
};

// mover: 위치 = center + amplitude × sin(2π t / period)
struct LevelMover {
	vec3 center;
	vec3 size;
	string material;
	vec3 amplitude;		// (ax, ay, az)
	float period;		// 주기(초)
	int line;
};

// worm: x ± range를 speed로 왕복
struct LevelWorm {
	vec3 position;
	float range;
	float speed;
	int line;
};

// checkpoint, fragment: 위치만 있다
struct LevelPoint {
	vec3 position;
	int line;
};

// lamp: 점광원
struct LevelLamp {
	vec3 position;
	float brightness;	// 밝기
	int line;
};

// hint: 플레이어 x가 xStart~xEnd 안에 있을 때 보여 줄 문장
struct LevelHint {
	float xStart;
	float xEnd;
	string text;		// UTF-8 그대로 둔다 (창 제목에 쓰는 glfwSetWindowTitle도 UTF-8을 받는다)
	int line;
};

// 레벨 파일 하나를 읽은 결과
struct LevelData {
	string file;			// 읽은 파일 경로

	string name;			// name: 스테이지 이름 (UTF-8)
	string subtitle;		// subtitle: 장 제목 리본
	int page = 0;			// page: 조각 번호 (0이면 파일에 없음)
	string next;			// next: 다음 레벨 파일. 비어 있으면 엔딩
	string theme;			// theme: day, afternoon, sunset, night
	bool hasKillY = false;
	float killY = 0.0f;		// killY: 낙사 높이
	bool hasStart = false;
	vec3 start{ 0.0f };		// start: 시작 위치(발 기준)

	vector<LevelBox> blocks;
	vector<LevelBox> spikes;
	vector<LevelMover> movers;
	vector<LevelWorm> worms;
	vector<LevelPoint> checkpoints;
	vector<LevelPoint> fragments;
	vector<LevelLamp> lamps;
	vector<LevelHint> hints;
};

// 장면의 "Level" 오브젝트에 붙는 컴포넌트. 동작 없이 LevelData만 들고 있다.
// 낙사 높이·힌트·다음 레벨처럼 레벨 값이 필요한 스크립트는 Hierarchy에서 이 컴포넌트를 찾아 쓴다.
class LevelInfo : public Component {
public:
	LevelData data;
};

class LevelLoader {
public:
	int skippedLines = 0;	// 마지막 Parse에서 건너뛴 잘못된 줄 수

	// Parse 다음 Build. 파일을 열지 못하면 false
	bool Load(AppContext& ctx, const string& file);

	// 1단계: 파일 → LevelData. 잘못된 줄은 "파일:줄 번호: 이유"를 출력하고 그 줄만 건너뛴다
	bool Parse(const string& file, LevelData& level);

	// 2단계: LevelData → 장면 오브젝트. start가 있으면 플레이어를 그 자리로 옮긴다
	void Build(AppContext& ctx, const LevelData& level);
};
