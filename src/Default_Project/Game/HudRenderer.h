#pragma once
#include "Component.h"
#include <gl/glew.h>
#include <gl/glm/glm.hpp>
using namespace glm;

class GameManager;

// 화면에 고정된 UI: HP 하트, 3D 게이지, 2D/3D 아이콘, 조각 수, 클리어 띠,
// 타이틀(책 표지)·일시정지·게임 오버·엔딩 화면과 버튼.
// 픽셀 좌표(왼쪽 위가 원점)의 사각형만으로 그린다. 글자는 5x7 점 글꼴(영문 대문자·숫자).
// 렌더 큐 맨 끝에서 그리므로, 그 직전에 모아 둔 반투명 물체(FadeRenderer)를 먼저 그린다.
class HudRenderer : public Component {
public:
	GameManager* game = nullptr;

	~HudRenderer() override;
	void Awake() override;
	void Render(Camera* activeCamera) override;

private:
	GLuint program = 0, vao = 0, vbo = 0;
	GLint rectLoc = -1, colorLoc = -1, screenLoc = -1, modeLoc = -1;
	float W = 0.0f, H = 0.0f, ui = 1.0f;

	void Rect(float x, float y, float w, float h, const vec4& color);
	void Frame(float x, float y, float w, float h, float border, const vec4& line, const vec4& fill);
	void Text(const char* text, float x, float y, float cell, const vec4& color);
	void TextCentered(const char* text, float centerX, float y, float cell, const vec4& color);
	float TextWidth(const char* text, float cell) const;
	void Heart(float x, float y, float cell, const vec4& color);
	void PageIcon(float x, float y, float s);

	void DrawPlayHud();
	void DrawButtons();
	void DrawTitle();
	void DrawPause();
	void DrawGameOver();
	void DrawEnding();
	void DrawChapterBanner();
	void DrawPageTurn();
};
