#pragma once
#include <vector>

// 플레이어 캐릭터 그림 시안 5종 (투명 배경, 오른쪽을 보는 모습).
// 거리 함수로 도형을 겹쳐 그린다. 게임에서 C 키로 바꿔 볼 수 있다.
// 정한 뒤에는 직접 그린 그림으로 바꿀 수 있도록 크기(128x192)와 발 위치(아래에서 8px)를 맞춰 둔다.
namespace CharacterArt {
	const int kWidth = 128;
	const int kHeight = 192;
	const int kCount = 5;

	const char* Name(int style);                       // 시안 이름 (UTF-8)
	std::vector<unsigned char> Draw(int style);        // RGBA, 아래 줄부터 (OpenGL 텍스처 순서)
}
