#pragma once
#include "Component.h"
#include <gl/glew.h>
#include <gl/glm/glm.hpp>
using namespace glm;

class GameManager;

// 화면에 고정된 UI: HP 하트, 3D 게이지, 2D/3D 아이콘, 조각 수, 클리어/게임 오버 띠.
// 픽셀 좌표(왼쪽 위가 원점)의 사각형만으로 그린다. 글자는 5x7 점 글꼴.
class HudRenderer : public Component {
public:
	GameManager* game = nullptr;

	~HudRenderer() override;
	void Awake() override;
	void Render(Camera* activeCamera) override;

private:
	GLuint program = 0, vao = 0, vbo = 0;
	GLint rectLoc = -1, colorLoc = -1, screenLoc = -1;

	void Rect(float x, float y, float w, float h, const vec4& color);
	void Text(const char* text, float x, float y, float cell, const vec4& color);
	float TextWidth(const char* text, float cell) const;
	void Heart(float x, float y, float cell, const vec4& color);
};
