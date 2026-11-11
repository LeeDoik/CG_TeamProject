#pragma once
#include <vector>

// 팝업북 꾸미기 그림 (코드로 그린 프로토타입, 투명 배경).
//  - 소품 아틀라스: 1024x512, 칸 128x256 (8열 x 2행). 정사각 소품은 칸의 아래쪽 128x128에 그린다.
//  - 배경 아틀라스: 1024x512, 칸 512x256 (2열 x 2행): 언덕, 산, 숲, 성 마을. 오려 낸 흰 테두리 포함.
//  - 잔디 가장자리: 64x32, 가로로 반복되는 물결 모양 잔디 끝
namespace StoryArt {
	enum Prop {
		Daisy, Tulips, Mushroom, Bush, Pine, RoundTree, Cottage, Fence,
		LampPost, GrassTuft, Star, Petal, Firefly, Cloud, Sun, Moon,
		kPropCount
	};
	enum Backdrop { Hills, Mountains, Forest, Town, kBackdropCount };

	const int kAtlasW = 1024, kAtlasH = 512;
	const int kPropCellW = 128, kPropCellH = 256, kPropCols = 8;
	const int kBackCellW = 512, kBackCellH = 256, kBackCols = 2;
	const int kFringeW = 64, kFringeH = 32;

	bool IsSquareProp(int prop);                  // 칸 아래쪽 정사각형만 쓰는 소품
	std::vector<unsigned char> DrawPropAtlas();   // RGBA, 아래 줄부터 (OpenGL 텍스처 순서)
	std::vector<unsigned char> DrawBackdropAtlas();
	std::vector<unsigned char> DrawFringe();
	std::vector<unsigned char> DrawPageLines();     // 256x256, 반복되는 책장 글줄
}
