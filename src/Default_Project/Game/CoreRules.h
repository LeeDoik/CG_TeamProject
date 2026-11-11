#pragma once
#include <vector>
#include <gl/glm/glm.hpp>
#include "GameTypes.h"

// ======================================================================
//  [직접 작성할 부분]  제안서 4.2 "본인이 직접 설계·구현할 부분"
//
//  이 파일의 함수 4개는 팀원이 직접 작성한다.
//  지금 CoreRules.cpp에 들어 있는 코드는 AI가 임시로 채운 것이다(2026-11-11, 팀원 결정).
//  면담·발표 전에 직접 다시 작성하고 Tests/clear_all.txt로 확인한다.
//  게임 쪽(GameManager)은 충돌·전환을 모두 이 함수들로만 판단한다.
//  자세한 설명: docs/스테이지1_구성.md
// ======================================================================

// 1. 차원별 충돌 규칙: 두 상자가 겹치는지 판단한다.
//    2D 모드에서는 깊이(z)를 무시한다 → 화면에서 겹쳐 보이면 이어진 길이 된다.
//    3D 모드에서는 x·y·z를 모두 비교한다.
bool BoxesOverlap(const Box& a, const Box& b, ViewMode mode);

// 2. 투영 행렬 보간: t = 0이면 2D(직교), t = 1이면 3D(원근) 행렬이 나와야 한다.
//    E를 누르면 0.5초 동안 t가 0→1(2D→3D) 또는 1→0(3D→2D)으로 바뀐다.
glm::mat4 BlendProjection(const glm::mat4& ortho2D, const glm::mat4& persp3D, float t);

// 3. 2D→3D 전환 조건: 3D로 바뀔 때 플레이어가 설 깊이(z)를 정해 돌려준다.
//    (제안서: "서 있는 발판의 깊이로 맞춘 뒤 3D로 바뀐다")
float ChooseDepthFor3D(const Box& player, const std::vector<Box>& solids, float currentZ);

// 4. 3D→2D 전환 조건: 지금 자리에서 2D로 돌아가도 되는지 판단한다.
//    (제안서: "물체와 겹치는 자리에서는 막힌다")
bool CanReturnTo2D(const Box& player, const std::vector<Box>& solids);
