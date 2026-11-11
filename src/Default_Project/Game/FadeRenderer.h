#pragma once
#include <vector>
#include "MeshRenderer3D.h"

// 투명도를 따로 가지는 렌더러 (반투명 벽, 발밑 그림자).
// [AI 임시 코드] 제안서상 "반투명 그리기 순서"는 직접 작성할 부분이다.
//
// 불투명하면 바로 그리고, 반투명하면 목록에 모아 두었다가 HUD 직전에
// 카메라에서 먼 것부터 블렌딩으로 그린다 (깊이 쓰기는 끔).
class FadeRenderer : public MeshRenderer3D {
public:
	float alpha = 1.0f;

	// 재질을 복사해 오브젝트마다 투명도를 따로 둔다
	FadeRenderer(Mesh* mesh, Material* source);
	~FadeRenderer() override;

	void Render(Camera* activeCamera) override;

	// 모아 둔 반투명 물체를 먼 것부터 그린다. 모든 불투명 물체를 그린 뒤 한 번 부른다.
	static void FlushTransparent(Camera* activeCamera);

private:
	Material* ownMaterial = nullptr;
	struct Pending { FadeRenderer* renderer; float distance; };
	static std::vector<Pending> pending;

	void DrawWithAlpha(Camera* activeCamera, float a);
};
