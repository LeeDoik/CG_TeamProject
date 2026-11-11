#pragma once
#include <string>
#include <vector>
#include "GameTypes.h"

// 레벨 파일(Levels/*.txt): 한 줄에 객체 하나. 형식은 docs/게임_구성.md 참고.
struct LevelBlock { vec3 center{ 0.0f }; vec3 size{ 1.0f }; std::string material; };
struct LevelMover { vec3 center{ 0.0f }; vec3 size{ 1.0f }; std::string material; vec3 amplitude{ 0.0f }; float period = 1.0f; };
struct LevelWorm { vec3 foot{ 0.0f }; float range = 1.0f; float speed = 1.0f; };
struct LevelLamp { vec3 position{ 0.0f }; float intensity = 1.0f; };
struct LevelHint { float x0 = 0.0f; float x1 = 0.0f; std::string text; };

struct LevelData {
	std::string name = "Stage";
	int page = 1;                  // 이 스테이지에서 얻는 조각 번호 (튜토리얼 1, 스테이지 1은 2 ...)
	std::string next;              // 다음 레벨 파일 이름 (비어 있으면 마지막 스테이지)
	float killY = -8.0f;           // 플레이어가 이 높이보다 아래로 떨어지면 낙사
	vec3 start{ 0.0f };            // 시작 위치 (발 위치)
	std::vector<LevelBlock> blocks;
	std::vector<LevelBlock> spikes;
	std::vector<LevelMover> movers;
	std::vector<LevelWorm> worms;
	std::vector<vec3> checkpoints; // 발 위치
	std::vector<vec3> fragments;   // 중심 위치
	std::vector<LevelLamp> lamps;
	std::vector<LevelHint> hints;
};

// 파일을 열면 true. 잘못된 줄은 "파일:줄번호 이유"를 콘솔에 출력하고 그 줄만 건너뛴다.
bool LoadLevelFile(const std::string& path, LevelData& out);
