#pragma once
#include "Component.h"

// 가시 하나 (레벨 파일의 spike, 기획서 4.4·5.3)
//  LevelLoader가 가시 상자마다 트리거 충돌 상자와 함께 붙인다. 하는 일은 없고 "이 상자는 가시"라는 표시만 한다.
//  닿았는지 보고 피해를 주는 일은 PlayerHealth가 한다(체크포인트 깃발과 같은 나눔).
//  트리거라서 플레이어를 막지 않는다. 이유는 PlayerHealth::TouchSpikes 설명.
//  3D → 2D 전환 검사(PlayerController::OverlapsAny2D)만은 이 표시를 보고 가시를 벽처럼 센다(2D로 바꾸면 가시 속이 되는 자리에서는 전환을 막는다).
class Spike : public Component {};
