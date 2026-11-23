#include "CoreRules.h"

// [AI 임시 코드] 직접 다시 작성할 것 (CoreRules.h 참고)
//
// 두 행렬을 원소마다 선형 보간한다.
//  - 직교 행렬은 w = 1, 원근 행렬은 w = -z(눈 좌표 깊이)를 만든다.
//    섞으면 w = (1 - t) + t * (-z)가 되어 원근감이 t만큼 서서히 생긴다.
//  - 바라보는 점의 깊이 D에서 직교 화면 절반 높이 h와 원근 화각 fov가
//    h = D * tan(fov / 2)를 만족하면, 그 평면의 물체는 t와 상관없이 크기가 그대로다.
//    ViewTransition이 fov를 이렇게 맞춰서 넘기므로 전환 도중 플레이어 크기가 튀지 않는다.
glm::mat4 BlendProjection(const glm::mat4& ortho2D, const glm::mat4& persp3D, float t) {
	return ortho2D * (1.0f - t) + persp3D * t;
}
