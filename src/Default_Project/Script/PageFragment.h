#pragma once
#include "Component.h"

// 페이지 조각 하나 (레벨 파일의 fragment, 기획서 3.3·5.3)
//  LevelLoader가 조각 상자마다 트리거 충돌 상자와 함께 붙인다. 하는 일은 없고 "이 상자는 조각"이라는 표시만 한다.
//  닿았는지 보고 스테이지를 클리어하는 일은 StageFlow가 한다(체크포인트 깃발을 PlayerHealth가 보는 것과 같은 나눔).
class PageFragment : public Component {};
