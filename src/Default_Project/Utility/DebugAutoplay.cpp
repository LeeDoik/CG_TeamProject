#include "DebugAutoplay.h"
#include "AppContext.h"
#include "Object.h"
#include "Transform.h"
#include "PlayerController.h"
#include "PlayerSpriteAnimator.h"
#include "ViewTransition.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <map>
#include <stb/stb_image_write.h>

namespace {
	string GetEnv(const char* name) {
		char* buffer = nullptr;
		size_t length = 0;
		string value;
		if (_dupenv_s(&buffer, &length, name) == 0 && buffer) {
			value = buffer;
			free(buffer);
		}
		return value;
	}

	int KeyFromName(const string& name) {
		static const map<string, int> keys = {
			{ "A", GLFW_KEY_A }, { "D", GLFW_KEY_D }, { "W", GLFW_KEY_W }, { "S", GLFW_KEY_S },
			{ "E", GLFW_KEY_E }, { "SPACE", GLFW_KEY_SPACE } };
		auto it = keys.find(name);
		return it == keys.end() ? -1 : it->second;
	}
}

void DebugAutoplay::Init() {
	headless = !GetEnv("POPUP_HEADLESS").empty();

	string dtText = GetEnv("POPUP_FIXED_DT");
	if (!dtText.empty()) {
		fixedDt = atof(dtText.c_str());
		if (fixedDt <= 0.0 || fixedDt > 0.05) {
			cout << "[Autoplay] POPUP_FIXED_DT는 0보다 크고 0.05 이하여야 합니다. 무시합니다: " << dtText << endl;
			fixedDt = 0.0;
		}
	}

	string dir = GetEnv("POPUP_SHOT_DIR");
	if (!dir.empty()) shotDir = dir;

	string scriptPath = GetEnv("POPUP_AUTOPLAY");
	if (scriptPath.empty()) return;

	ifstream in(scriptPath);
	if (!in) {
		cout << "[Autoplay] 스크립트를 열 수 없습니다: " << scriptPath << endl;
		return;
	}
	string line;
	while (getline(in, line)) {
		size_t hash = line.find('#');
		if (hash != string::npos) line = line.substr(0, hash);
		stringstream ss(line);
		Command cmd;
		if (!(ss >> cmd.frame >> cmd.op)) continue;
		ss >> cmd.arg;
		commands.push_back(cmd);
	}
	cout << "[Autoplay] 명령 " << commands.size() << "개 (" << scriptPath << "), 고정 dt " << fixedDt
		<< ", 창 " << (headless ? "숨김" : "보임") << endl;
}

void DebugAutoplay::ApplyInput(AppContext& ctx) {
	for (const Command& cmd : commands) {
		if (cmd.frame != frame) continue;
		if (cmd.op != "down" && cmd.op != "up") continue;
		int key = KeyFromName(cmd.arg);
		if (key < 0) {
			cout << "[Autoplay] 알 수 없는 키: " << cmd.arg << endl;
			continue;
		}
		ctx.inputManager.SetKey(key, cmd.op == "down");
	}
}

void DebugAutoplay::Capture(GLFWwindow* window, AppContext& ctx) {
	for (const Command& cmd : commands) {
		if (cmd.frame != frame) continue;

		if (cmd.op == "log") PrintPlayerState(ctx, "log");
		if (cmd.op != "shot") continue;

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		if (width <= 0 || height <= 0) continue;

		vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

		error_code ec;
		filesystem::create_directories(shotDir, ec);
		string path = shotDir + "/" + cmd.arg;

		// glReadPixels는 아래 줄부터 읽으므로 PNG로 쓸 때 위/아래를 뒤집는다
		stbi_flip_vertically_on_write(1);
		bool ok = stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3) != 0;
		PrintPlayerState(ctx, string(ok ? "shot " : "shot 실패 ") + cmd.arg + " " + to_string(width) + "x" + to_string(height));
	}
}

void DebugAutoplay::EndFrame(GLFWwindow* window, AppContext& ctx) {
	// 자동 재생 중에는 플레이어 그림이 바뀔 때마다 기록한다 (걷기 8fps 확인용)
	if (!commands.empty()) {
		for (auto& obj : ctx.Hierarchy) {
			PlayerSpriteAnimator* anim = obj->GetComponent<PlayerSpriteAnimator>();
			if (!anim) continue;
			if (anim->GetFrameName() != lastSpriteFrame || anim->IsFacingLeft() != lastFacingLeft) {
				lastSpriteFrame = anim->GetFrameName();
				lastFacingLeft = anim->IsFacingLeft();
				PrintPlayerState(ctx, "change");
			}
			break;
		}
	}

	for (const Command& cmd : commands) {
		if (cmd.frame == frame && cmd.op == "quit") {
			cout << "[Autoplay] f" << frame << " quit" << endl;
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}
	++frame;
}

void DebugAutoplay::PrintPlayerState(AppContext& ctx, const string& label) {
	ViewTransition* view = nullptr;
	for (auto& obj : ctx.Hierarchy) {
		view = obj->GetComponent<ViewTransition>();
		if (view) break;
	}

	for (auto& obj : ctx.Hierarchy) {
		PlayerSpriteAnimator* anim = obj->GetComponent<PlayerSpriteAnimator>();
		if (!anim) continue;
		Transform* tr = obj->GetComponent<Transform>();
		PlayerController* pc = obj->GetComponent<PlayerController>();

		cout << fixed << setprecision(3)
			<< "[Autoplay] f" << frame << " t=" << frame * fixedDt << "s " << label
			<< " | sprite=" << anim->GetFrameName() << (anim->IsFacingLeft() ? " (좌우반전)" : "")
			<< " grounded=" << (pc && pc->IsGrounded() ? 1 : 0)
			<< " pos=(" << tr->worldPosition.x << ", " << tr->worldPosition.y << ", " << tr->worldPosition.z << ")";
		if (view) cout << " view=" << (view->GetMode() == ViewMode::View3D ? "3D" : "2D") << " viewT=" << view->GetViewT();
		cout << defaultfloat << endl;
		return;
	}
}
