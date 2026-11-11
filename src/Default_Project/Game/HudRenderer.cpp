#include "HudRenderer.h"
#include "GameManager.h"
#include "ShaderManager.h"
#include "ScreenSize.h"
#include <map>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace std;

extern map<string, GLuint> vertexShaders;
extern map<string, GLuint> fragmentShaders;
extern map<string, GLuint> shaders;
extern ScreenSize screenSize;

namespace {
	struct Glyph { char c; const char* rows[7]; };

	// 5x7 점 글꼴: HUD에 쓰는 글자만 넣었다. '1'이 칠하는 칸.
	const Glyph kGlyphs[] = {
		{ '0', { "01110", "10001", "10011", "10101", "11001", "10001", "01110" } },
		{ '1', { "00100", "01100", "00100", "00100", "00100", "00100", "01110" } },
		{ '2', { "01110", "10001", "00001", "00010", "00100", "01000", "11111" } },
		{ '3', { "11110", "00001", "00001", "01110", "00001", "00001", "11110" } },
		{ '4', { "00010", "00110", "01010", "10010", "11111", "00010", "00010" } },
		{ '5', { "11111", "10000", "11110", "00001", "00001", "10001", "01110" } },
		{ '6', { "00110", "01000", "10000", "11110", "10001", "10001", "01110" } },
		{ '7', { "11111", "00001", "00010", "00100", "01000", "01000", "01000" } },
		{ '8', { "01110", "10001", "10001", "01110", "10001", "10001", "01110" } },
		{ '9', { "01110", "10001", "10001", "01111", "00001", "00010", "01100" } },
		{ '/', { "00001", "00010", "00010", "00100", "01000", "01000", "10000" } },
		{ '!', { "00100", "00100", "00100", "00100", "00100", "00000", "00100" } },
		{ 'A', { "01110", "10001", "10001", "11111", "10001", "10001", "10001" } },
		{ 'C', { "01110", "10001", "10000", "10000", "10000", "10001", "01110" } },
		{ 'D', { "11110", "10001", "10001", "10001", "10001", "10001", "11110" } },
		{ 'E', { "11111", "10000", "10000", "11110", "10000", "10000", "11111" } },
		{ 'G', { "01110", "10001", "10000", "10111", "10001", "10001", "01111" } },
		{ 'L', { "10000", "10000", "10000", "10000", "10000", "10000", "11111" } },
		{ 'M', { "10001", "11011", "10101", "10101", "10001", "10001", "10001" } },
		{ 'O', { "01110", "10001", "10001", "10001", "10001", "10001", "01110" } },
		{ 'R', { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } },
		{ 'S', { "01111", "10000", "10000", "01110", "00001", "00001", "11110" } },
		{ 'T', { "11111", "00100", "00100", "00100", "00100", "00100", "00100" } },
		{ 'V', { "10001", "10001", "10001", "10001", "10001", "01010", "00100" } },
	};

	const char* kHeart[6] = { "0110110", "1111111", "1111111", "0111110", "0011100", "0001000" };

	const Glyph* FindGlyph(char c) {
		for (const auto& g : kGlyphs) if (g.c == c) return &g;
		return nullptr;
	}

	const vec4 kInk(0.16f, 0.13f, 0.11f, 1.0f);
	const vec4 kPaper(1.0f, 0.99f, 0.96f, 1.0f);
	const vec4 kHeartOn(0.86f, 0.22f, 0.20f, 1.0f);
	const vec4 kHeartOff(0.74f, 0.70f, 0.64f, 1.0f);
	const vec4 kGauge2D(0.36f, 0.68f, 0.89f, 1.0f);
	const vec4 kGauge3D(0.96f, 0.69f, 0.25f, 1.0f);
	const vec4 kBlocked(0.85f, 0.16f, 0.16f, 1.0f);
	const vec4 kLine(0.62f, 0.60f, 0.58f, 1.0f);
}

HudRenderer::~HudRenderer() {
	if (vao) glDeleteVertexArrays(1, &vao);
	if (vbo) glDeleteBuffers(1, &vbo);
}

void HudRenderer::Awake() {
	if (shaders.find("Hud") == shaders.end()) {
		make_vertexShaders(vertexShaders, "Hud_vertex.glsl");
		make_fragmentShaders(fragmentShaders, "Hud_fragment.glsl");
		make_shaderProgram("Hud", shaders, vertexShaders["Hud_vertex.glsl"], fragmentShaders["Hud_fragment.glsl"]);
	}
	program = shaders["Hud"];
	rectLoc = glGetUniformLocation(program, "rect");
	colorLoc = glGetUniformLocation(program, "color");
	screenLoc = glGetUniformLocation(program, "screen");

	// 0~1 단위 사각형 (삼각형 2개)
	const float quad[] = { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void HudRenderer::Rect(float x, float y, float w, float h, const vec4& color) {
	if (w <= 0.0f || h <= 0.0f) return;
	glUniform4f(rectLoc, x, y, w, h);
	glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

float HudRenderer::TextWidth(const char* text, float cell) const {
	size_t n = strlen(text);
	if (n == 0) return 0.0f;
	float w = 0.0f;
	for (size_t i = 0; i < n; ++i) w += (text[i] == ' ') ? 4.0f * cell : 6.0f * cell;
	return w - cell; // 마지막 글자 뒤 간격 제외
}

void HudRenderer::Text(const char* text, float x, float y, float cell, const vec4& color) {
	for (const char* p = text; *p; ++p) {
		if (*p == ' ') { x += 4.0f * cell; continue; }
		const Glyph* g = FindGlyph(*p);
		if (g) {
			for (int r = 0; r < 7; ++r)
				for (int c = 0; c < 5; ++c)
					if (g->rows[r][c] == '1') Rect(x + c * cell, y + r * cell, cell, cell, color);
		}
		x += 6.0f * cell;
	}
}

void HudRenderer::Heart(float x, float y, float cell, const vec4& color) {
	for (int r = 0; r < 6; ++r)
		for (int c = 0; c < 7; ++c)
			if (kHeart[r][c] == '1') Rect(x + c * cell, y + r * cell, cell, cell, color);
}

void HudRenderer::Render(Camera* activeCamera) {
	if (!game || activeCamera != game->GetCamera()) return;
	float W = static_cast<float>(screenSize.width);
	float H = static_cast<float>(screenSize.height);
	if (W <= 0.0f || H <= 0.0f) return;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUseProgram(program);
	glUniform2f(screenLoc, W, H);
	glBindVertexArray(vao);

	float ui = std::max(0.7f, H / 1080.0f);
	float x0 = 24.0f * ui, y0 = 22.0f * ui;

	// HP 하트 3개
	for (int i = 0; i < game->GetMaxHp(); ++i)
		Heart(x0 + i * 46.0f * ui, y0, 5.0f * ui, i < game->GetHp() ? kHeartOn : kHeartOff);

	// 3D 게이지: 2D는 파랑, 3D는 주황, 3D에서 20% 이하면 깜빡임
	bool is3D = game->GetMode() == ViewMode::View3D;
	float gx = x0, gy = y0 + 44.0f * ui, gw = 220.0f * ui, gh = 22.0f * ui, pad = 3.0f * ui;
	Rect(gx, gy, gw, gh, kInk);
	Rect(gx + pad, gy + pad, gw - 2 * pad, gh - 2 * pad, kPaper);
	bool blinkOff = is3D && game->GetGauge() <= 0.2f && std::fmod(game->GetTime() * 6.0f, 2.0f) < 1.0f;
	if (!blinkOff) Rect(gx + pad, gy + pad, (gw - 2 * pad) * game->GetGauge(), gh - 2 * pad, is3D ? kGauge3D : kGauge2D);

	// 2D/3D 아이콘: 전환이 막히면 빨간색
	bool blocked = game->IsSwitchBlocked();
	float bx = gx + gw + 12.0f * ui, by = gy - 8.0f * ui, bw = 70.0f * ui, bh = 38.0f * ui;
	Rect(bx, by, bw, bh, kInk);
	Rect(bx + pad, by + pad, bw - 2 * pad, bh - 2 * pad, blocked ? kBlocked : kPaper);
	const char* label = is3D ? "3D" : "2D";
	float lc = 3.6f * ui;
	Text(label, bx + (bw - TextWidth(label, lc)) * 0.5f, by + (bh - 7.0f * lc) * 0.5f, lc, blocked ? kPaper : kInk);

	// 오른쪽 위: 페이지 조각 n/4
	char buf[16];
	snprintf(buf, sizeof(buf), "%d/%d", game->GetFragments(), game->GetTotalFragments());
	float fc = 4.5f * ui;
	float fx = W - 24.0f * ui - TextWidth(buf, fc), fy = y0;
	Text(buf, fx, fy, fc, kInk);
	float ix = fx - 40.0f * ui;
	Rect(ix, fy - 2.0f * ui, 26.0f * ui, 34.0f * ui, kInk);
	Rect(ix + 2.0f * ui, fy, 22.0f * ui, 30.0f * ui, kPaper);
	for (int i = 0; i < 3; ++i) Rect(ix + 5.0f * ui, fy + (7.0f + i * 7.0f) * ui, 16.0f * ui, 2.0f * ui, kLine);

	// 스테이지 클리어 / 게임 오버 띠
	GameState st = game->GetState();
	if (st == GameState::StageClear || st == GameState::GameOver) {
		bool clear = (st == GameState::StageClear);
		const char* msg = clear ? "STAGE CLEAR!" : "GAME OVER";
		float bandH = 110.0f * ui, bandY = (H - bandH) * 0.5f, mc = 9.0f * ui;
		Rect(0.0f, bandY, W, bandH, clear ? vec4(0.95f, 0.78f, 0.25f, 1.0f) : vec4(0.45f, 0.12f, 0.12f, 1.0f));
		Text(msg, (W - TextWidth(msg, mc)) * 0.5f, bandY + (bandH - 7.0f * mc) * 0.5f, mc, clear ? kInk : kPaper);
	}

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// HUD가 마지막으로 그려지므로 여기서 한 프레임이 완성된다 (스크린샷 시점)
	game->OnFrameRendered();
}
