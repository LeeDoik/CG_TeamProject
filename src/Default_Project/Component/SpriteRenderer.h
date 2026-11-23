#pragma once
#include "Component.h"
#include <gl/glew.h>
#include <gl/glm/glm.hpp>
using namespace glm;

class Texture;

// 종이 스프라이트(텍스처를 입힌 사각형 한 장)를 그리는 컴포넌트
//  - 사각형은 XY 평면에 세로로 선 종이 한 장이다 (앞면이 +z, 2D 카메라 쪽).
//    2D 화면에서는 그림이 그대로 보이고, 3D 화면에서는 그 자리에 선 종이 인형처럼 보인다.
//    billboardY를 켜면 세로축으로만 돌아 카메라 쪽을 본다 (지금 카메라는 좌우로 돌지 않아 결과가 같다).
//  - 그림 안의 기준점(pivot)이 월드 기준점(anchor)에 오도록 놓는다.
//    anchor: 같은 오브젝트에 BoxCollider가 있으면 충돌 상자 바닥 가운데, 없으면 Transform 위치
//  - Transform의 회전·크기는 쓰지 않는다 (플레이어 Transform의 크기는 충돌 상자 크기다)
//  - 앞·뒷면을 모두 그린다 (그리는 동안 면 컬링을 끈다)
//  - 반투명 그림이라 불투명 물체를 다 그린 뒤에 그린다 (main.cpp DrawScene이 순서를 맞춘다)
class SpriteRenderer : public Component {
public:
	Texture* texture = nullptr;
	vec2 pivot = vec2(0.5f, 0.0f);		// 그림 안 기준점, UV 좌표 ((0,0) = 그림 왼쪽 아래)
	vec2 worldSize = vec2(0.75f, 1.0f);	// 그림(캔버스) 전체의 월드 크기 (가로, 세로)
	bool flipX = false;					// true면 좌우 반전 (UV만 뒤집는다)
	bool billboardY = false;			// true면 세로축으로 돌아 카메라를 본다
	vec4 tint = vec4(1.0f);				// 그림 색에 곱하는 색
	float alphaCutoff = 0.1f;			// 이보다 투명한 픽셀은 버린다 (깊이도 안 씀)

	void Render(Camera* activeCamera) override;

	vec3 GetAnchor();
	mat4 GetModelMatrix(Camera* activeCamera);

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	// 모든 스프라이트가 같이 쓰는 (0,0)~(1,1) 사각형. 위치와 UV가 같다.
	static GLuint quadVAO;
	static GLuint quadVBO;
	static GLuint quadEBO;
	static void EnsureQuad();
};
