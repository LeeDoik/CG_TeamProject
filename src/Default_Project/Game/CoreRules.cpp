#include "CoreRules.h"
#include <cmath>

// ======================================================================
// [AI 임시 코드 · 2026-11-11]
//   튜토리얼·스테이지 1을 제안서 설계대로 끝까지 동작시키려고 네 함수 모두 AI가 채웠다(팀원 결정).
//   제안서 4.2("투영 전환·충돌·전환 조건은 직접 작성")대로 면담·발표 전에 직접 다시 작성하고,
//   Tests/clear_all.txt 클리어 테스트로 같은 결과가 나오는지 확인한다. (docs/AI_활용_기록.md)
// ======================================================================

// ----------------------------------------------------------------------
// 1. 차원별 충돌 규칙 (AABB)
//   - 2D: x·y 구간만 비교 (z 무시 → 화면에서 겹치면 이어진 길)
//   - 3D: x·y·z 구간 모두 비교
// ----------------------------------------------------------------------
bool BoxesOverlap(const Box& a, const Box& b, ViewMode mode) {
	bool overlapXY = a.min.x < b.max.x && a.max.x > b.min.x
		&& a.min.y < b.max.y && a.max.y > b.min.y;
	if (mode == ViewMode::View2D) return overlapXY;
	return overlapXY && a.min.z < b.max.z && a.max.z > b.min.z;
}

// ----------------------------------------------------------------------
// 2. 직교 ↔ 원근 투영 행렬 보간
//   두 행렬을 원소별로 선형 보간한다 = 클립 좌표를 보간하는 것과 같다.
//   GameManager가 원근 행렬의 화각을 "초점 거리에서 직교 화면과 같은 크기"가 되도록 맞춰 주므로
//   (카메라가 다가가며 화각이 넓어지는 돌리 줌), 전환 내내 플레이어 크기가 그대로이고
//   앞뒤 물체만 원근감이 생겼다 사라진다.
// ----------------------------------------------------------------------
glm::mat4 BlendProjection(const glm::mat4& ortho2D, const glm::mat4& persp3D, float t) {
	return ortho2D * (1.0f - t) + persp3D * t;
}

// ----------------------------------------------------------------------
// 3. 2D→3D: 서 있는 발판의 깊이로 맞추기
//   2D 규칙으로 발밑(윗면이 발 높이)에 있는 발판을 찾는다.
//   - 지금 깊이가 그 발판 안에 들어 있으면 그대로 둔다 (넓은 바닥에서는 깊이 유지)
//   - 아니면 그 발판의 깊이 중심으로 옮긴다
//   발판이 여러 개면 지금 깊이에 가장 가까운 것을 고른다. 공중이면 깊이를 바꾸지 않는다.
// ----------------------------------------------------------------------
float ChooseDepthFor3D(const Box& player, const std::vector<Box>& solids, float currentZ) {
	const float feet = player.min.y;
	const float halfDepth = (player.max.z - player.min.z) * 0.5f;
	bool found = false;
	float bestZ = currentZ;
	float bestDistance = 1e9f;

	for (const Box& s : solids) {
		bool underFeet = player.min.x < s.max.x && player.max.x > s.min.x && std::abs(s.max.y - feet) < 0.05f;
		if (!underFeet) continue;

		float lo = s.min.z + halfDepth, hi = s.max.z - halfDepth;
		float z = (currentZ >= lo && currentZ <= hi) ? currentZ : (s.min.z + s.max.z) * 0.5f;
		float distance = std::abs(z - currentZ);
		if (distance < bestDistance) {
			bestDistance = distance;
			bestZ = z;
			found = true;
		}
	}
	return found ? bestZ : currentZ;
}

// ----------------------------------------------------------------------
// 4. 3D→2D: 겹침 검사
//   2D 규칙(z 무시)으로 봤을 때 플레이어와 겹치는 발판·벽이 하나라도 있으면 돌아갈 수 없다.
//   (밟고 있는 바닥은 윗면에서 조금 떨어져 있으므로 겹침으로 치지 않는다)
// ----------------------------------------------------------------------
bool CanReturnTo2D(const Box& player, const std::vector<Box>& solids) {
	for (const Box& s : solids) {
		if (BoxesOverlap(player, s, ViewMode::View2D)) return false;
	}
	return true;
}
