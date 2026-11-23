#include "SpriteRenderer.h"
#include "Texture.h"
#include "Object.h"
#include "Transform.h"
#include "Camera.h"
#include "BoxCollider.h"
#include <cmath>
#include <gl/glm/gtc/type_ptr.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
using namespace std;

//main에서 만든 셰이더 프로그램 맵 ("Sprite" = Sprite_vertex.glsl + Sprite_fragment.glsl)
extern map<string, GLuint> shaders;

GLuint SpriteRenderer::quadVAO = 0;
GLuint SpriteRenderer::quadVBO = 0;
GLuint SpriteRenderer::quadEBO = 0;

void SpriteRenderer::EnsureQuad() {
	if (quadVAO != 0) return;

	// 위치(x, y, z) + UV(u, v). 사각형 (0,0)~(1,1)이 그림 전체와 같다.
	// 이미지 위/아래는 Texture::Load에서 이미 뒤집었으므로 여기서는 v = y 그대로 쓴다.
	const float vertices[] = {
		0.0f, 0.0f, 0.0f,	0.0f, 0.0f,	// 왼쪽 아래
		1.0f, 0.0f, 0.0f,	1.0f, 0.0f,	// 오른쪽 아래
		1.0f, 1.0f, 0.0f,	1.0f, 1.0f,	// 오른쪽 위
		0.0f, 1.0f, 0.0f,	0.0f, 1.0f	// 왼쪽 위
	};
	const GLuint indices[] = { 0, 1, 2, 0, 2, 3 };	// +z에서 볼 때 반시계 방향

	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	glGenBuffers(1, &quadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &quadEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//위치: 속성 0
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	//UV: 속성 1
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

vec3 SpriteRenderer::GetAnchor() {
	// 충돌 상자가 있으면 바닥 가운데 = 발이 닿는 점
	BoxCollider* box = gameObject->GetComponent<BoxCollider>();
	if (box) {
		return vec3((box->minPos.x + box->maxPos.x) * 0.5f, box->minPos.y, (box->minPos.z + box->maxPos.z) * 0.5f);
	}
	return gameObject->GetComponent<Transform>()->worldPosition;
}

mat4 SpriteRenderer::GetModelMatrix(Camera* activeCamera) {
	vec3 anchor = GetAnchor();

	// UV를 좌우로 뒤집으면 그림 안의 기준점도 반대쪽(1 - u)으로 옮겨 간다.
	// 그 점을 anchor에 맞춰야 방향을 바꿔도 발 위치가 그대로다.
	vec2 p = flipX ? vec2(1.0f - pivot.x, pivot.y) : pivot;

	mat4 model = translate(mat4(1.0f), anchor);

	if (billboardY && activeCamera) {
		vec3 camPos = activeCamera->gameObject->GetComponent<Transform>()->worldPosition;
		vec3 toCam = camPos - anchor;
		if (toCam.x * toCam.x + toCam.z * toCam.z > 1e-6f) {
			// 세로축으로 돌려 앞면(+z)이 카메라의 수평 방향을 보게 한다
			model = rotate(model, std::atan2(toCam.x, toCam.z), vec3(0.0f, 1.0f, 0.0f));
		}
	}

	model = scale(model, vec3(worldSize, 1.0f));
	model = translate(model, vec3(-p, 0.0f));	// 기준점을 원점으로 옮긴 뒤 크기·위치 적용
	return model;
}

void SpriteRenderer::Render(Camera* activeCamera) {
	if (!texture || !activeCamera) return;

	auto it = shaders.find("Sprite");
	if (it == shaders.end()) return;
	GLuint program = it->second;

	EnsureQuad();
	glUseProgram(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, value_ptr(GetModelMatrix(activeCamera)));
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, value_ptr(activeCamera->GetViewMatrix()));
	glUniformMatrix4fv(glGetUniformLocation(program, "proj"), 1, GL_FALSE, value_ptr(activeCamera->GetProjectionMatrix()));
	glUniform1i(glGetUniformLocation(program, "flipX"), flipX ? 1 : 0);
	glUniform4fv(glGetUniformLocation(program, "tint"), 1, value_ptr(tint));
	glUniform1f(glGetUniformLocation(program, "alphaCutoff"), alphaCutoff);
	glUniform1i(glGetUniformLocation(program, "spriteTex"), 0);

	texture->Bind(0);

	// 뒷면에서 봐도 사라지지 않게 컬링을 끄고, straight alpha 블렌딩을 켠다. 끝나면 원래대로 돌려놓는다.
	GLboolean cullWasOn = glIsEnabled(GL_CULL_FACE);
	GLboolean blendWasOn = glIsEnabled(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glBindVertexArray(quadVAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	if (cullWasOn) glEnable(GL_CULL_FACE);
	if (!blendWasOn) glDisable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, 0);
}

Component* SpriteRenderer::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	SpriteRenderer* sr = obj->AddComponent<SpriteRenderer>();

	// "texture": 그림 경로 (실행 폴더 기준, 예: "Assets/Characters/PaperChild/paper_child_idle_right.png")
	if (props.contains("texture")) sr->texture = Texture::Load(props["texture"].get<string>());
	if (props.contains("pivot")) sr->pivot = vec2(props["pivot"][0], props["pivot"][1]);
	// "worldSize": [가로, 세로] 또는 "height": 세로만 주고 가로는 그림 비율로
	if (props.contains("worldSize")) sr->worldSize = vec2(props["worldSize"][0], props["worldSize"][1]);
	else if (props.contains("height") && sr->texture && sr->texture->height > 0) {
		float h = props["height"];
		sr->worldSize = vec2(h * sr->texture->width / sr->texture->height, h);
	}
	if (props.contains("flipX")) sr->flipX = props["flipX"];
	if (props.contains("billboardY")) sr->billboardY = props["billboardY"];
	if (props.contains("alphaCutoff")) sr->alphaCutoff = props["alphaCutoff"];
	if (props.contains("tint")) sr->tint = vec4(props["tint"][0], props["tint"][1], props["tint"][2], props["tint"][3]);

	return sr;
}

REGISTER_COMPONENT(SpriteRenderer)
