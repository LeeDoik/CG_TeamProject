#pragma once
#include "Component.h"
#include <gl/glm/glm.hpp>
using namespace glm;

// 체크포인트 깃발 하나 (레벨 파일의 checkpoint, 기획서 4.4)
//  LevelLoader가 깃발(가는 기둥)마다 트리거 충돌 상자와 함께 붙인다.
//  닿았는지 보고 위치를 저장하는 일은 PlayerHealth가 한다. 여기는 돌아올 자리를 들고 깃발 색만 바꾼다.
class Checkpoint : public Component {
public:
	vec3 foot;		// 돌아올 자리(발 위치). 레벨 파일의 checkpoint x y z 그대로(y는 바닥 기준)

	Checkpoint(vec3 foot) : foot(foot) {}

	// 켜면 노랑, 끄면 처음 색인 초록 (임시 모양. 깃발 그림이 생기면 그림을 바꾼다)
	void SetActive(bool on);
};
