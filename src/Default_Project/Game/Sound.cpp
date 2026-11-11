#include "Sound.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <set>
#include <string>
#include <cstdio>
#pragma comment(lib, "winmm.lib")
using namespace std;

namespace {
	bool muted = true;
	bool musicWanted = false;
	set<string> opened;

	bool Mci(const string& command) {
		return mciSendStringA(command.c_str(), nullptr, 0, nullptr) == 0;
	}
}

void Sound::Init(bool startMuted) {
	muted = startMuted;
	const char* names[] = { "jump", "switch", "blocked", "hurt", "stomp", "checkpoint", "pickup", "clear", "select", "bgm" };
	for (const char* n : names) {
		// mpegvideo 장치는 별칭마다 따로 재생돼서 효과음이 서로 겹쳐도 된다
		if (Mci(string("open \"Sounds\\") + n + ".wav\" type mpegvideo alias " + n)) opened.insert(n);
	}
	if (opened.count("bgm")) Mci("setaudio bgm volume to 350");
	std::printf("[소리] %d/10개 파일 열림%s\n", static_cast<int>(opened.size()), muted ? " (음소거)" : "");
}

void Sound::Play(const char* name) {
	if (muted || !opened.count(name)) return;
	Mci(string("play ") + name + " from 0");
}

void Sound::PlayMusic() {
	musicWanted = true;
	if (muted || !opened.count("bgm")) return;
	Mci("play bgm from 0 repeat");
}

void Sound::SetMuted(bool value) {
	muted = value;
	if (!opened.count("bgm")) return;
	if (muted) Mci("stop bgm");
	else if (musicWanted) Mci("play bgm from 0 repeat");
}

bool Sound::IsMuted() { return muted; }

void Sound::Shutdown() {
	Mci("close all");
	opened.clear();
}
