#include "FadeRenderer.h"
#include "Material.h"
#include "Object.h"
#include "Transform.h"
#include "Camera.h"
#include <gl/glew.h>
#include <algorithm>

std::vector<FadeRenderer::Pending> FadeRenderer::pending;

FadeRenderer::FadeRenderer(Mesh* mesh, Material* source)
	: MeshRenderer3D(mesh, new Material(*source)) {
	ownMaterial = materials[0];
}

FadeRenderer::~FadeRenderer() {
	// 이번 프레임 목록에 남아 있으면 빼고 지운다
	pending.erase(std::remove_if(pending.begin(), pending.end(),
		[this](const Pending& p) { return p.renderer == this; }), pending.end());
	delete ownMaterial;
}

void FadeRenderer::DrawWithAlpha(Camera* activeCamera, float a) {
	vec4 color = ownMaterial->GetVec4("tColor");
	color.a = a;
	ownMaterial->SetVec4("tColor", color);
	MeshRenderer3D::Render(activeCamera);
}

void FadeRenderer::Render(Camera* activeCamera) {
	if (alpha >= 0.995f) {
		DrawWithAlpha(activeCamera, 1.0f);
		return;
	}
	if (alpha <= 0.0f || activeCamera == nullptr) return;

	vec3 camPos = activeCamera->gameObject->GetComponent<Transform>()->worldPosition;
	vec3 myPos = gameObject->GetComponent<Transform>()->worldPosition;
	pending.push_back({ this, glm::length(camPos - myPos) });
}

void FadeRenderer::FlushTransparent(Camera* activeCamera) {
	if (pending.empty()) return;
	std::stable_sort(pending.begin(), pending.end(),
		[](const Pending& a, const Pending& b) { return a.distance > b.distance; }); // 먼 것부터

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	for (const Pending& p : pending) p.renderer->DrawWithAlpha(activeCamera, p.renderer->alpha);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	pending.clear();
}
