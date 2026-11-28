// windows.h는 UTF-8 글자를 콘솔 글자로 바꾸는 데(MultiByteToWideChar)만 쓴다.
// 다른 헤더의 using namespace std, min/max와 부딪히지 않도록 맨 먼저, 필요한 부분만 넣는다.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "LevelLoader.h"
#include "Object.h"
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include "MeshRenderer3D.h"
#include "BoxCollider.h"
#include "PlayerController.h"
#include "MovingPlatform.h"
#include <fstream>
#include <sstream>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>

// main.cpp에서 만든 메시·재질 캐시 (MeshRenderer3D.cpp와 같은 방식)
extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;

namespace {
	const char* kDefaultMaterial = "Paper";		// 재질을 안 적었거나 엔진에 없는 이름일 때. 기획서 5.3의 기본 고정 발판이 종이 발판이다
	const size_t kAnyCount = SIZE_MAX;			// name, hint처럼 문장이라 단어 수가 정해지지 않은 경우

	// ---------------- 글자 ----------------

	// 레벨 파일은 UTF-8로 읽는다. 메모장에서 ANSI(CP949)로 저장한 한글 줄은 여기서 걸러진다.
	bool IsUtf8(const string& s) {
		if (s.empty()) return true;
		return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0) > 0;
	}

	// 파일의 UTF-8 글자를 콘솔에 찍을 글자로 바꾼다.
	// 엔진 소스의 한글 문자열은 컴파일할 때 시스템 코드 페이지(한국어 Windows는 CP949)로 바뀌므로,
	// 레벨 파일에서 온 글자도 같은 코드 페이지로 맞춰야 한 줄 안에서 깨지지 않는다.
	string ToConsole(const string& utf8) {
		int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
		if (wideLen <= 0) return utf8;
		wstring wide(wideLen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), wide.data(), wideLen);

		int len = WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
		string out(len, '\0');
		WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen, out.data(), len, nullptr, nullptr);
		return out;
	}

	// 앞뒤 공백·탭과 줄 끝의 \r(Windows 줄바꿈)을 지운다
	string Trim(const string& s) {
		const char* spaces = " \t\r\n";
		size_t begin = s.find_first_not_of(spaces);
		if (begin == string::npos) return "";
		size_t end = s.find_last_not_of(spaces);
		return s.substr(begin, end - begin + 1);
	}

	// 줄에서 앞 단어 count개를 건너뛴 나머지. 문장 안의 띄어쓰기는 그대로 둔다
	string TextAfterWords(const string& line, int count) {
		size_t pos = 0;
		for (int i = 0; i < count && pos != string::npos; ++i) {
			pos = line.find_first_not_of(" \t", pos);	// 단어 시작
			pos = line.find_first_of(" \t", pos);		// 단어 끝
		}
		if (pos == string::npos) return "";
		return Trim(line.substr(pos));
	}

	// ---------------- 값 검사 ----------------

	// "1.5", "-7", "2e3"만 숫자로 본다. "1.5m", "0,5", inf, nan은 숫자가 아니다
	bool ToFloat(const string& s, float& out) {
		auto [end, ec] = from_chars(s.data(), s.data() + s.size(), out);
		return ec == errc{} && end == s.data() + s.size() && std::isfinite(out);
	}

	bool ToInt(const string& s, int& out) {
		auto [end, ec] = from_chars(s.data(), s.data() + s.size(), out);
		return ec == errc{} && end == s.data() + s.size();
	}

	// 값 개수 검사. 틀리면 올바른 형식(usage)을 error에 적는다
	bool CheckCount(const vector<string>& args, size_t minCount, size_t maxCount, const string& usage, string& error) {
		if (args.size() >= minCount && args.size() <= maxCount) return true;
		error = "값 개수가 틀렸습니다(" + to_string(args.size()) + "개). 형식: " + usage;
		return false;
	}

	// args[first]부터 count개를 숫자로 읽는다. 틀리면 몇 번째 값인지 error에 적는다
	bool ReadNumbers(const vector<string>& args, size_t first, size_t count, float* out, string& error) {
		for (size_t i = 0; i < count; ++i) {
			if (!ToFloat(args[first + i], out[i])) {
				error = to_string(first + i + 1) + "번째 값이 숫자가 아닙니다: " + ToConsole(args[first + i]);
				return false;
			}
		}
		return true;
	}

	// 크기가 0 이하이면 scale이 0이나 음수가 되어 그리기·충돌이 망가진다
	bool CheckSize(vec3 size, string& error) {
		if (size.x > 0.0f && size.y > 0.0f && size.z > 0.0f) return true;
		error = "크기 w h d는 0보다 커야 합니다";
		return false;
	}

	// ---------------- 한 줄 읽기 ----------------

	// 키워드마다 값 개수와 숫자를 검사하고 LevelData에 넣는다.
	// 틀리면 이유를 error에 적고 false를 돌려준다. 이때 LevelData는 바뀌지 않는다.
	bool ParseLine(const string& line, int lineNo, LevelData& level, string& error) {
		if (!IsUtf8(line)) {
			error = "UTF-8이 아닌 글자가 있습니다. 파일을 UTF-8로 저장하세요";
			return false;
		}

		// 첫 단어가 키워드, 나머지는 공백으로 나눈 값
		istringstream words{ line };
		string keyword;
		words >> keyword;
		vector<string> args;
		for (string word; words >> word; ) args.push_back(word);

		float v[10];	// 숫자를 잠시 담는 곳 (mover가 숫자 10개로 가장 많다)

		if (keyword == "name" || keyword == "subtitle") {
			// 키워드 뒤 전체가 문장
			if (!CheckCount(args, 1, kAnyCount, keyword + " 문장", error)) return false;
			if (keyword == "name") level.name = TextAfterWords(line, 1);
			else level.subtitle = TextAfterWords(line, 1);
		}
		else if (keyword == "page") {
			int page;
			if (!CheckCount(args, 1, 1, "page 번호", error)) return false;
			if (!ToInt(args[0], page)) {
				error = "page 값은 정수여야 합니다: " + ToConsole(args[0]);
				return false;
			}
			level.page = page;
		}
		else if (keyword == "next") {
			if (!CheckCount(args, 1, 1, "next 파일이름", error)) return false;
			level.next = args[0];
		}
		else if (keyword == "theme") {
			if (!CheckCount(args, 1, 1, "theme day|afternoon|sunset|night", error)) return false;
			const string& t = args[0];
			if (t != "day" && t != "afternoon" && t != "sunset" && t != "night") {
				error = "theme 값은 day, afternoon, sunset, night 중 하나여야 합니다: " + ToConsole(t);
				return false;
			}
			level.theme = t;
		}
		else if (keyword == "killY") {
			if (!CheckCount(args, 1, 1, "killY 높이", error) || !ReadNumbers(args, 0, 1, v, error)) return false;
			level.killY = v[0];
			level.hasKillY = true;
		}
		else if (keyword == "start") {
			if (!CheckCount(args, 3, 3, "start x y z", error) || !ReadNumbers(args, 0, 3, v, error)) return false;
			level.start = vec3(v[0], v[1], v[2]);
			level.hasStart = true;
		}
		else if (keyword == "block" || keyword == "spike") {
			// 재질은 적어도 되고 안 적어도 된다 → 값 6개 또는 7개
			if (!CheckCount(args, 6, 7, keyword + " x y z w h d [재질]", error) || !ReadNumbers(args, 0, 6, v, error)) return false;
			LevelBox box{ vec3(v[0], v[1], v[2]), vec3(v[3], v[4], v[5]), args.size() == 7 ? args[6] : "", lineNo };
			if (!CheckSize(box.size, error)) return false;
			if (keyword == "block") level.blocks.push_back(box);
			else level.spikes.push_back(box);
		}
		else if (keyword == "mover") {
			// 7번째 값이 재질(반드시 적음), 그 뒤에 진폭 3개와 주기
			if (!CheckCount(args, 11, 11, "mover x y z w h d 재질 ax ay az 주기", error)
				|| !ReadNumbers(args, 0, 6, v, error) || !ReadNumbers(args, 7, 4, v + 6, error)) return false;
			LevelMover mover{ vec3(v[0], v[1], v[2]), vec3(v[3], v[4], v[5]), args[6], vec3(v[6], v[7], v[8]), v[9], lineNo };
			if (!CheckSize(mover.size, error)) return false;
			if (mover.period <= 0.0f) {	// sin(2π t / 주기)에서 0으로 나누지 않게
				error = "주기는 0보다 커야 합니다";
				return false;
			}
			level.movers.push_back(mover);
		}
		else if (keyword == "worm") {
			if (!CheckCount(args, 5, 5, "worm x y z 범위 속도", error) || !ReadNumbers(args, 0, 5, v, error)) return false;
			level.worms.push_back({ vec3(v[0], v[1], v[2]), v[3], v[4], lineNo });
		}
		else if (keyword == "checkpoint" || keyword == "fragment") {
			if (!CheckCount(args, 3, 3, keyword + " x y z", error) || !ReadNumbers(args, 0, 3, v, error)) return false;
			LevelPoint point{ vec3(v[0], v[1], v[2]), lineNo };
			if (keyword == "checkpoint") level.checkpoints.push_back(point);
			else level.fragments.push_back(point);
		}
		else if (keyword == "lamp") {
			if (!CheckCount(args, 4, 4, "lamp x y z 밝기", error) || !ReadNumbers(args, 0, 4, v, error)) return false;
			level.lamps.push_back({ vec3(v[0], v[1], v[2]), v[3], lineNo });
		}
		else if (keyword == "hint") {
			// 숫자 2개 뒤는 모두 문장
			if (!CheckCount(args, 3, kAnyCount, "hint x시작 x끝 문장", error) || !ReadNumbers(args, 0, 2, v, error)) return false;
			level.hints.push_back({ v[0], v[1], TextAfterWords(line, 3), lineNo });
		}
		else {
			error = "알 수 없는 키워드입니다: " + ToConsole(keyword);
			return false;
		}
		return true;
	}

	// ---------------- 장면 만들기 ----------------

	// 장면에 빈 오브젝트를 하나 만든다 (SceneLoader::Instantiate와 같다)
	Object* NewObject(AppContext& ctx, const string& name) {
		ctx.Hierarchy.push_back(make_unique<Object>(ctx, name));
		return ctx.Hierarchy.back().get();
	}

	// 중심 center, 크기 size인 큐브. Default_Cube.obj는 -1~1(한 변 2칸)이라 scale = size / 메시 크기.
	// SceneLoader처럼 Transform을 먼저 정하고 컴포넌트를 붙인다.
	Object* CreateBox(AppContext& ctx, const string& name, vec3 center, vec3 size, Material* material) {
		Object* obj = NewObject(ctx, name);
		Mesh* cube = meshChache["Cube"];

		Transform* tr = obj->GetComponent<Transform>();
		vec3 scale = size / (cube->maxPos - cube->minPos);
		tr->SetLocalPosition(center);
		tr->SetLocalScale(scale.x, scale.y, scale.z);

		obj->AddComponent<MeshRenderer3D>(cube, material);
		return obj;
	}

	// worm·checkpoint·fragment·lamp처럼 위치만 적는 줄은 y가 바닥(발) 기준이다(start와 같은 규칙).
	// 상자 높이의 절반만큼 올려서 그 바닥 위에 서게 한다.
	Object* CreateOnFloor(AppContext& ctx, const string& name, vec3 foot, vec3 size, Material* material) {
		return CreateBox(ctx, name, foot + vec3(0.0f, 0.5f * size.y, 0.0f), size, material);
	}

	// 재질 이름으로 엔진 Material(main.cpp의 materialChache)을 찾는다.
	// 안 적었으면 기본 재질, 엔진에 없는 이름이면 경고를 찍고 기본 재질을 쓴다(그 줄은 살린다).
	Material* FindMaterial(const string& name, const LevelData& level, int line) {
		if (!name.empty()) {
			auto it = materialChache.find(name);
			if (it != materialChache.end()) return it->second;
			cout << level.file << ":" << line << ": 경고: 재질이 엔진에 없어 기본 재질(" << kDefaultMaterial
				<< ")을 씁니다: " << ToConsole(name) << endl;
		}
		return materialChache[kDefaultMaterial];
	}

	// start(발 위치)에 플레이어를 놓는다.
	// 플레이어 Transform 위치는 충돌 상자의 중심이므로, 발에서 중심까지(상자 높이의 절반)만큼 올린다.
	// 플레이어에는 메시가 없어 상자 높이 = 1 × size.y × scale.y이다 (BoxCollider::RecalculateCollision 참고).
	void PlacePlayer(AppContext& ctx, const LevelData& level) {
		for (auto& obj : ctx.Hierarchy) {
			if (!obj->GetComponent<PlayerController>()) continue;

			Transform* tr = obj->GetComponent<Transform>();
			BoxCollider* box = obj->GetComponent<BoxCollider>();
			float footToCenter = 0.0f;
			if (box) footToCenter = (0.5f * box->size.y - box->center.y) * tr->scale.y;

			tr->SetLocalPosition(level.start + vec3(0.0f, footToCenter, 0.0f));
			return;
		}
		cout << "[레벨] 경고: PlayerController가 붙은 오브젝트가 없어 start를 쓰지 못했습니다" << endl;
	}

	// 불러온 결과를 콘솔에 한 번 보여 준다. 한글(name, hint)이 깨지지 않았는지 여기서 볼 수 있다.
	void PrintSummary(const LevelData& level, int skippedLines) {
		cout << "[레벨] " << level.file << ": " << ToConsole(level.name) << " / " << ToConsole(level.subtitle)
			<< " / page " << level.page << " / theme " << level.theme
			<< " / next " << (level.next.empty() ? "(엔딩)" : ToConsole(level.next)) << endl;
		cout << "[레벨] 발판 " << level.blocks.size() << ", 움직이는 발판 " << level.movers.size()
			<< ", 가시 " << level.spikes.size() << ", 책벌레 " << level.worms.size()
			<< ", 체크포인트 " << level.checkpoints.size() << ", 조각 " << level.fragments.size()
			<< ", 등불 " << level.lamps.size() << ", 힌트 " << level.hints.size()
			<< ", 건너뛴 줄 " << skippedLines << endl;
		for (const LevelHint& hint : level.hints) {
			cout << "[레벨] 힌트 x " << hint.xStart << "~" << hint.xEnd << ": " << ToConsole(hint.text) << endl;
		}
	}
}

bool LevelLoader::Load(AppContext& ctx, const string& file) {
	LevelData level;
	if (!Parse(file, level)) return false;

	Build(ctx, level);
	PrintSummary(level, skippedLines);
	return true;
}

bool LevelLoader::Parse(const string& file, LevelData& level) {
	// ifstream은 글자를 바꾸지 않고 바이트 그대로 읽는다 → 한글은 UTF-8 그대로 LevelData에 들어간다
	ifstream in{ file };
	if (!in) {
		cout << "[레벨] 파일을 열지 못했습니다: " << file << endl;
		return false;
	}

	level = LevelData{};
	level.file = file;
	skippedLines = 0;

	string raw;
	for (int lineNo = 1; getline(in, raw); ++lineNo) {
		// 메모장에서 "UTF-8(BOM)"으로 저장하면 첫 줄 앞에 EF BB BF 3바이트가 붙는다
		if (lineNo == 1 && raw.starts_with("\xEF\xBB\xBF")) raw.erase(0, 3);

		// 빈 줄과 #으로 시작하는 줄은 건너뛴다. 줄 중간의 #은 주석이 아니다 (형식 규칙)
		string line = Trim(raw);
		if (line.empty() || line[0] == '#') continue;

		string error;
		if (!ParseLine(line, lineNo, level, error)) {
			cout << file << ":" << lineNo << ": " << error << endl;
			++skippedLines;
		}
	}
	return true;
}

void LevelLoader::Build(AppContext& ctx, const LevelData& level) {
	// 고정 발판·벽: 큐브 + BoxCollider. Game_Scene.json의 Ground와 같은 구성이라 충돌은 엔진 그대로다
	for (const LevelBox& b : level.blocks) {
		Object* obj = CreateBox(ctx, "block:" + to_string(b.line), b.center, b.size, FindMaterial(b.material, level, b.line));
		obj->AddComponent<BoxCollider>();
	}

	// 움직이는 발판: 고정 발판 + MovingPlatform(이동만). 위에 탄 플레이어도 같이 옮긴다(PlayerController::RideAlong, 기획서 4.3)
	for (const LevelMover& m : level.movers) {
		Object* obj = CreateBox(ctx, "mover:" + to_string(m.line), m.center, m.size, FindMaterial(m.material, level, m.line));
		obj->AddComponent<BoxCollider>();
		obj->AddComponent<MovingPlatform>(m.center, m.amplitude, m.period);
	}

	// ---- 아래는 임시 모양만 놓는다. 충돌체가 없어서 플레이어가 그대로 지나간다 ----

	// 가시: 빨간 상자. 적힌 재질은 LevelData에만 남긴다
	// [연결 지점] 닿으면 HP -1, 마지막 체크포인트로 (기획서 4.4)
	for (const LevelBox& s : level.spikes) {
		CreateBox(ctx, "spike:" + to_string(s.line), s.center, s.size, materialChache["Red"]);
	}

	// 책벌레: 납작한 보라 상자
	// [연결 지점] x ± range를 speed로 왕복, 위에서 밟으면 처치, 그 밖의 접촉은 피해 (기획서 5.2)
	for (const LevelWorm& w : level.worms) {
		CreateOnFloor(ctx, "worm:" + to_string(w.line), w.position, vec3(0.6f, 0.4f, 0.4f), materialChache["Purple"]);
	}

	// 체크포인트: 가는 초록 기둥
	// [연결 지점] 닿으면 위치 저장 (기획서 4.4). 게이지 0 + 2D 복귀 불가일 때 돌아갈 곳(DimensionSwitch::GetRespawnPoint)도 이 위치로
	for (const LevelPoint& c : level.checkpoints) {
		CreateOnFloor(ctx, "checkpoint:" + to_string(c.line), c.position, vec3(0.15f, 1.0f, 0.15f), materialChache["Green"]);
	}

	// 페이지 조각: 작은 주황 상자
	// [연결 지점] 얻으면 스테이지 클리어 (기획서 5.3)
	for (const LevelPoint& f : level.fragments) {
		CreateOnFloor(ctx, "fragment:" + to_string(f.line), f.position, vec3(0.4f), materialChache["Orange"]);
	}

	// 등불: 작은 노란 상자. 빛은 아직 내지 않는다
	// [연결 지점] 점광원(Light, Point). 밝기는 Light::intensity 값으로 쓴다 (형식 규칙, 기획서 5.3)
	for (const LevelLamp& l : level.lamps) {
		CreateOnFloor(ctx, "lamp:" + to_string(l.line), l.position, vec3(0.3f), materialChache["DarkYellow"]);
	}

	// hint, killY, theme, page, next, name, subtitle은 장면에 보이는 것이 없어 LevelInfo에만 둔다
	// [연결 지점] 힌트 구간 → 창 제목, killY 아래로 떨어짐 → HP -1, 조각 획득 → next 레벨 (기획서 4.4, 5.3)

	if (level.hasStart) PlacePlayer(ctx, level);
	else cout << "[레벨] 경고: " << level.file << "에 start가 없어 플레이어를 Game_Scene.json 위치에 둡니다" << endl;

	Object* levelObj = NewObject(ctx, "Level");
	levelObj->AddComponent<LevelInfo>()->data = level;
}
