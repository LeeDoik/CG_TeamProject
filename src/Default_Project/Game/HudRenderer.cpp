#include "HudRenderer.h"
#include "GameManager.h"
#include "FadeRenderer.h"
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

	// 5x7 점 글꼴: '1'이 칠하는 칸
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
		{ 'A', { "01110", "10001", "10001", "11111", "10001", "10001", "10001" } },
		{ 'B', { "11110", "10001", "10001", "11110", "10001", "10001", "11110" } },
		{ 'C', { "01110", "10001", "10000", "10000", "10000", "10001", "01110" } },
		{ 'D', { "11110", "10001", "10001", "10001", "10001", "10001", "11110" } },
		{ 'E', { "11111", "10000", "10000", "11110", "10000", "10000", "11111" } },
		{ 'F', { "11111", "10000", "10000", "11110", "10000", "10000", "10000" } },
		{ 'G', { "01110", "10001", "10000", "10111", "10001", "10001", "01111" } },
		{ 'H', { "10001", "10001", "10001", "11111", "10001", "10001", "10001" } },
		{ 'I', { "01110", "00100", "00100", "00100", "00100", "00100", "01110" } },
		{ 'J', { "00111", "00010", "00010", "00010", "00010", "10010", "01100" } },
		{ 'K', { "10001", "10010", "10100", "11000", "10100", "10010", "10001" } },
		{ 'L', { "10000", "10000", "10000", "10000", "10000", "10000", "11111" } },
		{ 'M', { "10001", "11011", "10101", "10101", "10001", "10001", "10001" } },
		{ 'N', { "10001", "10001", "11001", "10101", "10011", "10001", "10001" } },
		{ 'O', { "01110", "10001", "10001", "10001", "10001", "10001", "01110" } },
		{ 'P', { "11110", "10001", "10001", "11110", "10000", "10000", "10000" } },
		{ 'Q', { "01110", "10001", "10001", "10001", "10101", "10010", "01101" } },
		{ 'R', { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } },
		{ 'S', { "01111", "10000", "10000", "01110", "00001", "00001", "11110" } },
		{ 'T', { "11111", "00100", "00100", "00100", "00100", "00100", "00100" } },
		{ 'U', { "10001", "10001", "10001", "10001", "10001", "10001", "01110" } },
		{ 'V', { "10001", "10001", "10001", "10001", "10001", "01010", "00100" } },
		{ 'W', { "10001", "10001", "10001", "10101", "10101", "10101", "01010" } },
		{ 'X', { "10001", "10001", "01010", "00100", "01010", "10001", "10001" } },
		{ 'Y', { "10001", "10001", "01010", "00100", "00100", "00100", "00100" } },
		{ 'Z', { "11111", "00001", "00010", "00100", "01000", "10000", "11111" } },
		{ '/', { "00001", "00010", "00010", "00100", "01000", "01000", "10000" } },
		{ '!', { "00100", "00100", "00100", "00100", "00100", "00000", "00100" } },
		{ ':', { "00000", "01100", "01100", "00000", "01100", "01100", "00000" } },
		{ '-', { "00000", "00000", "00000", "11111", "00000", "00000", "00000" } },
		{ '.', { "00000", "00000", "00000", "00000", "00000", "01100", "01100" } },
		{ '>', { "01000", "00100", "00010", "00001", "00010", "00100", "01000" } },
		{ '<', { "00010", "00100", "01000", "10000", "01000", "00100", "00010" } },
	};

	const char* kHeart[6] = { "0110110", "1111111", "1111111", "0111110", "0011100", "0001000" };

	const Glyph* FindGlyph(char c) {
		for (const auto& g : kGlyphs) if (g.c == c) return &g;
		return nullptr;
	}

	const vec4 kInk(0.16f, 0.13f, 0.11f, 1.0f);
	const vec4 kPaper(1.0f, 0.99f, 0.96f, 1.0f);
	const vec4 kCream(0.97f, 0.93f, 0.84f, 1.0f);
	const vec4 kHeartOn(0.86f, 0.22f, 0.20f, 1.0f);
	const vec4 kHeartOff(0.74f, 0.70f, 0.64f, 1.0f);
	const vec4 kGauge2D(0.36f, 0.68f, 0.89f, 1.0f);
	const vec4 kGauge3D(0.96f, 0.69f, 0.25f, 1.0f);
	const vec4 kBlocked(0.85f, 0.16f, 0.16f, 1.0f);
	const vec4 kLine(0.62f, 0.60f, 0.58f, 1.0f);
	const vec4 kGold(0.95f, 0.76f, 0.24f, 1.0f);
	const vec4 kCover(0.55f, 0.16f, 0.14f, 1.0f);     // 책 표지 (짙은 빨강)
	const vec4 kCoverDark(0.36f, 0.09f, 0.08f, 1.0f);
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

// ---------------- 그리기 도구 ----------------

void HudRenderer::Rect(float x, float y, float w, float h, const vec4& color) {
	if (w <= 0.0f || h <= 0.0f) return;
	glUniform4f(rectLoc, x, y, w, h);
	glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void HudRenderer::Frame(float x, float y, float w, float h, float border, const vec4& line, const vec4& fill) {
	Rect(x, y, w, h, line);
	Rect(x + border, y + border, w - 2 * border, h - 2 * border, fill);
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

void HudRenderer::TextCentered(const char* text, float centerX, float y, float cell, const vec4& color) {
	Text(text, centerX - TextWidth(text, cell) * 0.5f, y, cell, color);
}

void HudRenderer::Heart(float x, float y, float cell, const vec4& color) {
	for (int r = 0; r < 6; ++r)
		for (int c = 0; c < 7; ++c)
			if (kHeart[r][c] == '1') Rect(x + c * cell, y + r * cell, cell, cell, color);
}

void HudRenderer::PageIcon(float x, float y, float s) {
	Rect(x, y, 26.0f * s, 34.0f * s, kInk);
	Rect(x + 2.0f * s, y + 2.0f * s, 22.0f * s, 30.0f * s, kPaper);
	for (int i = 0; i < 3; ++i) Rect(x + 5.0f * s, y + (9.0f + i * 7.0f) * s, 16.0f * s, 2.0f * s, kLine);
}

// ---------------- 화면별 ----------------

void HudRenderer::DrawPlayHud() {
	float x0 = 24.0f * ui, y0 = 22.0f * ui;

	// HP 하트 3개
	for (int i = 0; i < game->GetMaxHp(); ++i)
		Heart(x0 + i * 46.0f * ui, y0, 5.0f * ui, i < game->GetHp() ? kHeartOn : kHeartOff);

	// 3D 게이지: 2D는 파랑, 3D는 주황, 3D에서 20% 이하면 깜빡임
	bool is3D = game->GetMode() == ViewMode::View3D;
	float gx = x0, gy = y0 + 44.0f * ui, gw = 220.0f * ui, gh = 22.0f * ui, pad = 3.0f * ui;
	Frame(gx, gy, gw, gh, pad, kInk, kPaper);
	bool blinkOff = is3D && game->GetGauge() <= 0.2f && std::fmod(game->GetRealTime() * 6.0f, 2.0f) < 1.0f;
	if (!blinkOff) Rect(gx + pad, gy + pad, (gw - 2 * pad) * game->GetGauge(), gh - 2 * pad, is3D ? kGauge3D : kGauge2D);

	// 2D/3D 아이콘: 전환이 막히면 빨간색
	bool blocked = game->IsSwitchBlocked();
	float bx = gx + gw + 12.0f * ui, by = gy - 8.0f * ui, bw = 70.0f * ui, bh = 38.0f * ui;
	Frame(bx, by, bw, bh, pad, kInk, blocked ? kBlocked : kPaper);
	const char* label = is3D ? "3D" : "2D";
	float lc = 3.6f * ui;
	TextCentered(label, bx + bw * 0.5f, by + (bh - 7.0f * lc) * 0.5f, lc, blocked ? kPaper : kInk);

	// 가운데 위: 스테이지 이름
	char stage[24];
	if (game->GetPage() <= 1) snprintf(stage, sizeof(stage), "TUTORIAL");
	else snprintf(stage, sizeof(stage), "STAGE %d", game->GetPage() - 1);
	TextCentered(stage, W * 0.5f, y0 + 4.0f * ui, 3.4f * ui, kInk);

	// 오른쪽 위: 페이지 조각 n/4
	char buf[16];
	snprintf(buf, sizeof(buf), "%d/%d", game->GetFragments(), game->GetTotalFragments());
	float fc = 4.5f * ui;
	float fx = W - 24.0f * ui - TextWidth(buf, fc);
	Text(buf, fx, y0, fc, kInk);
	PageIcon(fx - 40.0f * ui, y0 - 2.0f * ui, ui);

	if (game->IsMuted()) Text("MUTE", W - 24.0f * ui - TextWidth("MUTE", 2.4f * ui), y0 + 44.0f * ui, 2.4f * ui, kLine);

	// 스테이지 클리어 띠
	if (game->GetState() == GameState::StageClear) {
		float bandH = 110.0f * ui, bandY = (H - bandH) * 0.5f, mc = 9.0f * ui;
		Rect(0.0f, bandY, W, bandH, kGold);
		TextCentered("STAGE CLEAR!", W * 0.5f, bandY + (bandH - 7.0f * mc) * 0.5f, mc, kInk);
	}
}

void HudRenderer::DrawButtons() {
	vector<MenuButton> buttons = game->GetMenuButtons();
	int selected = game->GetSelectedButton();
	float bc = 4.6f * ui, border = 4.0f * ui;
	for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
		const MenuButton& b = buttons[i];
		bool on = (i == selected);
		Frame(b.x, b.y, b.w, b.h, border, kInk, on ? kGold : kPaper);
		TextCentered(b.label.c_str(), b.x + b.w * 0.5f, b.y + (b.h - 7.0f * bc) * 0.5f, bc, kInk);
		if (on) {
			bool blink = std::fmod(game->GetRealTime() * 2.0f, 2.0f) < 1.4f;
			if (blink) Text(">", b.x - 40.0f * ui, b.y + (b.h - 7.0f * bc) * 0.5f, bc, kInk);
		}
	}
}

// 타이틀: 덮인 책 표지
void HudRenderer::DrawTitle() {
	Rect(0, 0, W, H, kCoverDark);
	float m = 40.0f * ui;
	Frame(m, m, W - 2 * m, H - 2 * m, 10.0f * ui, kGold, kCover);
	Frame(m + 30 * ui, m + 30 * ui, W - 2 * m - 60 * ui, H - 2 * m - 60 * ui, 3.0f * ui, kGold, kCover);

	float tc = 14.0f * ui;
	TextCentered("POP-UP BOOK", W * 0.5f, H * 0.22f, tc, kGold);
	TextCentered("FIND THE 4 TORN PAGES", W * 0.5f, H * 0.22f + 7.0f * tc + 30.0f * ui, 4.0f * ui, kCream);

	// 표지 가운데 장식: 페이지 조각 4장
	for (int i = 0; i < 4; ++i) PageIcon(W * 0.5f - 90.0f * ui + i * 46.0f * ui, H * 0.47f, 1.2f * ui);

	DrawButtons();
	TextCentered("MOVE A D   JUMP SPACE   2D 3D E   DEPTH W S   PAUSE ESC   SOUND M", W * 0.5f, H - m - 80.0f * ui, 2.6f * ui, kCream);
}

void HudRenderer::DrawPause() {
	DrawPlayHud();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Rect(0, 0, W, H, vec4(0.1f, 0.08f, 0.06f, 0.55f));
	glDisable(GL_BLEND);
	float pc = 10.0f * ui;
	TextCentered("PAUSED", W * 0.5f, H * 0.30f, pc, kPaper);
	DrawButtons();
}

void HudRenderer::DrawGameOver() {
	DrawPlayHud();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Rect(0, 0, W, H, vec4(0.25f, 0.05f, 0.05f, 0.55f));
	glDisable(GL_BLEND);
	float bandH = 120.0f * ui, bandY = H * 0.30f, mc = 10.0f * ui;
	Rect(0.0f, bandY, W, bandH, vec4(0.45f, 0.12f, 0.12f, 1.0f));
	TextCentered("GAME OVER", W * 0.5f, bandY + (bandH - 7.0f * mc) * 0.5f, mc, kPaper);
	DrawButtons();
}

// 엔딩: 마지막 장 완성, 클리어 시간
void HudRenderer::DrawEnding() {
	Rect(0, 0, W, H, kCoverDark);
	float m = 60.0f * ui;
	Frame(m, m, W - 2 * m, H - 2 * m, 6.0f * ui, kGold, kCream);

	TextCentered("THE LAST PAGE IS COMPLETE!", W * 0.5f, H * 0.16f, 8.0f * ui, kInk);
	for (int i = 0; i < 4; ++i) PageIcon(W * 0.5f - 120.0f * ui + i * 62.0f * ui, H * 0.32f, 1.7f * ui);

	int total = static_cast<int>(game->GetPlayTime());
	char line[48];
	snprintf(line, sizeof(line), "PAGES %d/%d", game->GetFragments(), game->GetTotalFragments());
	TextCentered(line, W * 0.5f, H * 0.48f, 5.0f * ui, kInk);
	snprintf(line, sizeof(line), "TIME %02d:%02d", total / 60, total % 60);
	TextCentered(line, W * 0.5f, H * 0.48f + 56.0f * ui, 5.0f * ui, kInk);
	TextCentered("THANK YOU FOR PLAYING", W * 0.5f, H * 0.48f + 120.0f * ui, 3.4f * ui, kLine);
	DrawButtons();
}

void HudRenderer::Render(Camera* activeCamera) {
	if (!game || activeCamera != game->GetCamera()) return;

	// 불투명 물체를 다 그렸으니 모아 둔 반투명 물체를 먼 것부터 그린다
	FadeRenderer::FlushTransparent(activeCamera);

	W = static_cast<float>(screenSize.width);
	H = static_cast<float>(screenSize.height);
	if (W <= 0.0f || H <= 0.0f) return;
	ui = std::max(0.7f, H / 1080.0f);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUseProgram(program);
	glUniform2f(screenLoc, W, H);
	glBindVertexArray(vao);

	switch (game->GetState()) {
	case GameState::Title:    DrawTitle(); break;
	case GameState::Paused:   DrawPause(); break;
	case GameState::GameOver: DrawGameOver(); break;
	case GameState::Ending:   DrawEnding(); break;
	default:                  DrawPlayHud(); break;
	}

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// HUD가 마지막으로 그려지므로 여기서 한 프레임이 완성된다 (스크린샷 시점)
	game->OnFrameRendered();
}
