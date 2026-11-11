#pragma once
#include "MeshRenderer3D.h"

// 2D 모드에서 캐릭터를 항상 맨 앞에 그리는 렌더러 (제안서 2번 "가시성 보조").
// 깊이 값의 범위를 화면 바로 앞(0 ~ 0.02)으로 좁혀서 그리면,
// 그리는 순서와 상관없이 다른 물체보다 앞에 보인다.
class OnTopRenderer : public MeshRenderer3D {
public:
	static bool drawOnTop; // GameManager가 매 프레임 정한다 (2D일 때 true)

	OnTopRenderer(Mesh* mesh, Material* material) : MeshRenderer3D(mesh, material) {}

	void Render(Camera* activeCamera) override;
};
