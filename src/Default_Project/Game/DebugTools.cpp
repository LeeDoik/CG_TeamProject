#include "DebugTools.h"
#include "InputManager.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <filesystem>
#include <cstdint>
using namespace std;

string GetEnvVar(const char* name) {
	char* buffer = nullptr;
	size_t length = 0;
	string value;
	if (_dupenv_s(&buffer, &length, name) == 0 && buffer) {
		value = buffer;
		free(buffer);
	}
	return value;
}

bool SaveScreenshotBMP(const string& path, int width, int height) {
	if (width <= 0 || height <= 0) return false;
	filesystem::path p(path);
	if (p.has_parent_path()) {
		error_code ec;
		filesystem::create_directories(p.parent_path(), ec);
	}

	// BMP는 한 줄을 4바이트 단위로 맞추고, 아래 줄부터 저장한다 (glReadPixels와 같은 순서)
	int rowSize = (width * 3 + 3) & ~3;
	vector<unsigned char> pixels(static_cast<size_t>(rowSize) * height);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

	ofstream out(p, ios::binary);
	if (!out) return false;
	auto put16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
	auto put32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
	uint32_t dataSize = static_cast<uint32_t>(pixels.size());
	out.write("BM", 2);
	put32(54 + dataSize); put16(0); put16(0); put32(54);                 // 파일 헤더
	put32(40); put32(width); put32(height); put16(1); put16(24);         // 정보 헤더
	put32(0); put32(dataSize); put32(2835); put32(2835); put32(0); put32(0);
	out.write(reinterpret_cast<const char*>(pixels.data()), dataSize);
	return static_cast<bool>(out);
}

bool InputReplay::Load(const string& path) {
	static const map<string, int> keys = {
		{ "A", GLFW_KEY_A }, { "D", GLFW_KEY_D }, { "W", GLFW_KEY_W }, { "S", GLFW_KEY_S },
		{ "E", GLFW_KEY_E }, { "SPACE", GLFW_KEY_SPACE }, { "R", GLFW_KEY_R },
		{ "F1", GLFW_KEY_F1 }, { "F2", GLFW_KEY_F2 } };

	ifstream in(path);
	if (!in) {
		cout << "[재생] 스크립트를 열 수 없습니다: " << path << endl;
		return false;
	}
	events.clear();
	string line;
	int lineNo = 0;
	while (getline(in, line)) {
		++lineNo;
		size_t hash = line.find('#');
		if (hash != string::npos) line = line.substr(0, hash);
		istringstream ss(line);
		double t;
		string key, action;
		if (!(ss >> t)) continue;
		ss >> key;
		if (key == "quit") { events.push_back({ t, 0, false, true }); continue; }
		ss >> action;
		auto it = keys.find(key);
		if (it == keys.end() || (action != "down" && action != "up")) {
			cout << "[재생] " << path << ":" << lineNo << " 형식 오류" << endl;
			continue;
		}
		events.push_back({ t, it->second, action == "down", false });
	}
	stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.time < b.time; });
	next = 0;
	active = true;
	cout << "[재생] " << path << " 이벤트 " << events.size() << "개" << endl;
	return true;
}

bool InputReplay::Apply(double now, InputManager& input) {
	bool quit = false;
	while (next < events.size() && events[next].time <= now) {
		const Event& e = events[next++];
		if (e.quit) quit = true;
		else input.SetKey(e.key, e.down);
	}
	return quit;
}
