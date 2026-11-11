#pragma once

// 코드로 만드는 프로토타입 텍스처 (제안서: 프로토타입 텍스처 → 이후 직접 그린 그림·CC0로 교체).
// 만든 텍스처는 아래 텍스처 유닛에 계속 묶어 두고, 재질에서는 샘플러 uniform에 유닛 번호만 넣는다.
namespace ProceduralTextures {
	enum Unit {
		kPaper = 1,   // 종이 결
		kCard = 2,    // 골판지 줄무늬
		kWall = 3,    // 벽지 무늬
		kDoll = 4,    // 종이 인형 (투명 배경)
		kPage = 5,    // 찢어진 페이지 조각 (투명 배경)
	};

	void CreateAll();
}
