#include "LevelLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

namespace {
	// 숫자 n개를 차례로 읽는다. 하나라도 못 읽으면 false.
	bool ReadFloats(istringstream& ss, float* dst, int n) {
		for (int i = 0; i < n; ++i) {
			if (!(ss >> dst[i])) return false;
		}
		return true;
	}

	// 줄의 남은 부분(앞 공백 제외)을 통째로 읽는다. 힌트 문장용.
	string RestOfLine(istringstream& ss) {
		string rest;
		getline(ss, rest);
		size_t begin = rest.find_first_not_of(" \t");
		return (begin == string::npos) ? string() : rest.substr(begin);
	}
}

bool LoadLevelFile(const string& path, LevelData& out) {
	ifstream in(path);
	if (!in) {
		cout << "[레벨] 파일을 열 수 없습니다: " << path << endl;
		return false;
	}

	out = LevelData{};
	string line;
	int lineNo = 0;
	int errorCount = 0;
	auto fail = [&](const string& why) {
		cout << "[레벨] " << path << ":" << lineNo << " " << why << endl;
		++errorCount;
	};

	while (getline(in, line)) {
		++lineNo;
		// UTF-8 BOM, 윈도우 줄바꿈, # 주석 제거
		if (lineNo == 1 && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
			line = line.substr(3);
		}
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t hash = line.find('#');
		if (hash != string::npos) line = line.substr(0, hash);

		istringstream ss(line);
		string kind;
		if (!(ss >> kind)) continue; // 빈 줄

		float v[10];
		if (kind == "name") {
			out.name = RestOfLine(ss);
		}
		else if (kind == "page") {
			if (ReadFloats(ss, v, 1)) out.page = static_cast<int>(v[0]);
			else fail("page 번호가 필요합니다");
		}
		else if (kind == "subtitle") {
			out.subtitle = RestOfLine(ss);
		}
		else if (kind == "theme") {
			ss >> out.theme;
		}
		else if (kind == "next") {
			ss >> out.next;
		}
		else if (kind == "killY") {
			if (ReadFloats(ss, v, 1)) out.killY = v[0];
			else fail("killY 높이가 필요합니다");
		}
		else if (kind == "start") {
			if (ReadFloats(ss, v, 3)) out.start = vec3(v[0], v[1], v[2]);
			else fail("start x y z 형식이어야 합니다");
		}
		else if (kind == "block" || kind == "spike") {
			if (!ReadFloats(ss, v, 6)) { fail(kind + " x y z w h d [재질] 형식이어야 합니다"); continue; }
			LevelBlock b;
			b.center = vec3(v[0], v[1], v[2]);
			b.size = vec3(v[3], v[4], v[5]);
			ss >> b.material;
			if (b.size.x <= 0 || b.size.y <= 0 || b.size.z <= 0) { fail("크기(w h d)는 0보다 커야 합니다"); continue; }
			(kind == "block" ? out.blocks : out.spikes).push_back(b);
		}
		else if (kind == "mover") {
			LevelMover m;
			if (!ReadFloats(ss, v, 6) || !(ss >> m.material) || !ReadFloats(ss, v + 6, 4)) {
				fail("mover x y z w h d 재질 ax ay az 주기 형식이어야 합니다");
				continue;
			}
			m.center = vec3(v[0], v[1], v[2]);
			m.size = vec3(v[3], v[4], v[5]);
			m.amplitude = vec3(v[6], v[7], v[8]);
			m.period = v[9];
			if (m.period <= 0.0f) { fail("mover 주기는 0보다 커야 합니다"); continue; }
			out.movers.push_back(m);
		}
		else if (kind == "worm") {
			if (!ReadFloats(ss, v, 5)) { fail("worm x y z 범위 속도 형식이어야 합니다"); continue; }
			out.worms.push_back({ vec3(v[0], v[1], v[2]), v[3], v[4] });
		}
		else if (kind == "checkpoint") {
			if (ReadFloats(ss, v, 3)) out.checkpoints.push_back(vec3(v[0], v[1], v[2]));
			else fail("checkpoint x y z 형식이어야 합니다");
		}
		else if (kind == "fragment") {
			if (ReadFloats(ss, v, 3)) out.fragments.push_back(vec3(v[0], v[1], v[2]));
			else fail("fragment x y z 형식이어야 합니다");
		}
		else if (kind == "lamp") {
			if (ReadFloats(ss, v, 4)) out.lamps.push_back({ vec3(v[0], v[1], v[2]), v[3] });
			else fail("lamp x y z 밝기 형식이어야 합니다");
		}
		else if (kind == "hint") {
			if (!ReadFloats(ss, v, 2)) { fail("hint x시작 x끝 문장 형식이어야 합니다"); continue; }
			out.hints.push_back({ v[0], v[1], RestOfLine(ss) });
		}
		else {
			fail("알 수 없는 종류: " + kind);
		}
	}

	if (out.fragments.empty()) {
		cout << "[레벨] " << path << ": fragment(페이지 조각)가 하나도 없습니다" << endl;
		++errorCount;
	}

	cout << "[레벨] " << path << " 불러옴 (블록 " << out.blocks.size()
		<< ", 가시 " << out.spikes.size() << ", 움직이는 발판 " << out.movers.size()
		<< ", 책벌레 " << out.worms.size() << ", 체크포인트 " << out.checkpoints.size()
		<< ", 오류 " << errorCount << ")" << endl;
	return true;
}
