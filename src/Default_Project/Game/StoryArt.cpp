#include "StoryArt.h"
#include "SdfPaint.h"
#include <cmath>
using namespace std;
using namespace SdfPaint;

namespace {
	const float kPi = 3.14159265f;
	const Rgb kLeaf{ 0.42f, 0.66f, 0.36f }, kLeafDark{ 0.30f, 0.52f, 0.30f }, kLeafLite{ 0.56f, 0.78f, 0.46f };
	const Rgb kWood{ 0.80f, 0.62f, 0.42f }, kWoodLite{ 0.90f, 0.74f, 0.52f }, kTrunk{ 0.55f, 0.38f, 0.24f };
	const Rgb kWarm{ 1.0f, 0.86f, 0.42f };

	// ================= 소품 (칸 좌표: x 0~128, y 0~256, 바닥 y=250) =================
	void Daisy(Painter& pt) {
		V2 p = pt.p;
		pt.Shape(Segment(p, 64, 250, 64, 116, 3.0f), kLeaf);
		pt.Shape(EllipseRot(p, 50, 206, 16, 6, -0.5f), kLeaf);
		pt.Shape(EllipseRot(p, 79, 184, 16, 6, 0.5f), kLeaf);
		for (int k = 0; k < 10; ++k) {
			float a = k * 2.0f * kPi / 10.0f;
			pt.Shape(EllipseRot(p, 64 + std::cos(a) * 21, 108 + std::sin(a) * 21, 13, 7, a), { 1.0f, 0.99f, 0.96f }, 1.6f);
		}
		pt.Shape(Circle(p, 64, 108, 11), { 0.98f, 0.80f, 0.25f });
		pt.Detail(Circle(p, 60, 104, 3), { 1.0f, 0.90f, 0.50f });
	}

	void Tulips(Painter& pt) {
		V2 p = pt.p;
		struct T { float x, h; Rgb c; };
		const T ts[] = { { 40, 150, { 0.92f, 0.32f, 0.32f } }, { 64, 128, { 1.0f, 0.62f, 0.72f } }, { 88, 160, { 1.0f, 0.82f, 0.30f } } };
		for (const T& t : ts) {
			pt.Shape(Segment(p, t.x, 250, t.x, t.h + 10, 2.6f), kLeaf);
			pt.Shape(EllipseRot(p, t.x + 9, 220, 14, 5, 0.9f), kLeaf);
		}
		for (const T& t : ts) {
			float cup = std::min({ Ellipse(p, t.x, t.h, 13, 15),
				Poly(p, { { t.x - 13, t.h - 2 }, { t.x - 9, t.h - 22 }, { t.x - 3, t.h - 6 } }),
				Poly(p, { { t.x + 13, t.h - 2 }, { t.x + 9, t.h - 22 }, { t.x + 3, t.h - 6 } }),
				Poly(p, { { t.x - 5, t.h - 6 }, { t.x, t.h - 24 }, { t.x + 5, t.h - 6 } }) });
			pt.Shape(cup, t.c);
			pt.Detail(std::max(cup + 4.0f, p.x - t.x + 2.0f), Mix(t.c, kWhite, 0.25f));
		}
	}

	void Mushroom(Painter& pt, V2 q) {
		pt.Shape(RoundBox(q, 64, 96, 17, 28, 8), { 0.98f, 0.94f, 0.84f });
		float cap = std::max(Ellipse(q, 64, 66, 56, 42), q.y - 70.0f);
		pt.Shape(cap, { 0.88f, 0.26f, 0.22f });
		const float dots[][3] = { { 38, 52, 7 }, { 64, 38, 8 }, { 90, 52, 7 }, { 52, 62, 5 }, { 78, 62, 5 } };
		for (auto& d : dots) if (cap <= -2.0f && Circle(q, d[0], d[1], d[2]) <= 0.0f) pt.c = kWhite;
	}

	void Bush(Painter& pt, V2 q) {
		float b = std::min({ Circle(q, 36, 88, 30), Circle(q, 64, 70, 37), Circle(q, 93, 88, 30), Circle(q, 64, 100, 30) });
		pt.Shape(b, kLeaf);
		pt.Detail(std::max(Circle(q, 54, 60, 20), b + 4.0f), kLeafLite);
		const float berries[][2] = { { 44, 84 }, { 76, 64 }, { 92, 94 }, { 62, 98 }, { 30, 98 } };
		for (auto& e : berries) pt.Detail(Circle(q, e[0], e[1], 4.5f), { 0.86f, 0.22f, 0.28f });
	}

	void Pine(Painter& pt) {
		V2 p = pt.p;
		pt.Shape(RoundBox(p, 64, 232, 8, 20, 3), kTrunk);
		const float tri[][3] = { { 140, 216, 56 }, { 92, 166, 48 }, { 40, 118, 38 } };   // 꼭대기 y, 밑 y, 반폭
		const Rgb cols[] = { { 0.25f, 0.50f, 0.32f }, { 0.30f, 0.56f, 0.36f }, { 0.36f, 0.62f, 0.40f } };
		for (int i = 0; i < 3; ++i) {
			float d = Poly(p, { { 64, tri[i][0] }, { 64 - tri[i][2], tri[i][1] }, { 64 + tri[i][2], tri[i][1] } });
			pt.Shape(d, cols[i]);
			if (d <= -2.2f && p.x > 64) pt.c = Mix(cols[i], kWhite, 0.15f);
		}
	}

	void RoundTree(Painter& pt) {
		V2 p = pt.p;
		pt.Shape(RoundBox(p, 64, 206, 10, 44, 4), kTrunk);
		float crown = std::min({ Circle(p, 64, 96, 46), Circle(p, 30, 120, 30), Circle(p, 98, 120, 30), Circle(p, 64, 134, 34) });
		pt.Shape(crown, { 0.47f, 0.70f, 0.38f });
		pt.Detail(std::max(Circle(p, 50, 80, 24), crown + 4.0f), { 0.60f, 0.80f, 0.48f });
		const float apples[][2] = { { 40, 116 }, { 84, 104 }, { 66, 140 }, { 96, 130 } };
		for (auto& a : apples) pt.Detail(Circle(p, a[0], a[1], 6), { 0.90f, 0.28f, 0.25f });
	}

	void Cottage(Painter& pt) {
		V2 p = pt.p;
		pt.Shape(RoundBox(p, 94, 104, 8, 22, 2), { 0.72f, 0.38f, 0.28f });                       // 굴뚝
		pt.Shape(RoundBox(p, 64, 198, 44, 52, 3), { 0.98f, 0.92f, 0.80f });                       // 벽
		float roof = Poly(p, { { 64, 70 }, { 6, 148 }, { 122, 148 } });
		pt.Shape(roof, { 0.80f, 0.30f, 0.25f });
		if (roof <= -2.2f && std::fmod(p.y, 12.0f) < 1.5f) pt.c = { 0.66f, 0.22f, 0.20f };        // 기와 줄
		pt.Shape(Circle(p, 64, 120, 9), kWarm);                                                    // 다락 창
		pt.Shape(RoundBox(p, 46, 226, 12, 24, 5), { 0.52f, 0.34f, 0.22f });                       // 문
		pt.Detail(Circle(p, 53, 228, 2.2f), kWarm);
		pt.Shape(RoundBox(p, 88, 188, 14, 14, 3), kWarm);                                          // 창문
		pt.Detail(RoundBox(p, 88, 188, 1.2f, 14, 0), kInk);
		pt.Detail(RoundBox(p, 88, 188, 14, 1.2f, 0), kInk);
		pt.Detail(RoundBox(p, 64, 246, 44, 4, 1), { 0.80f, 0.70f, 0.55f });                       // 토대
	}

	void Fence(Painter& pt, V2 q) {
		pt.Shape(RoundBox(q, 64, 76, 62, 5, 2), kWood);
		pt.Shape(RoundBox(q, 64, 102, 62, 5, 2), kWood);
		for (float x : { 22.0f, 64.0f, 106.0f }) {
			float picket = std::min(RoundBox(q, x, 98, 9, 28, 2), Poly(q, { { x - 9, 72 }, { x + 9, 72 }, { x, 54 } }));
			pt.Shape(picket, kWoodLite);
		}
	}

	void LampPost(Painter& pt) {
		V2 p = pt.p;
		Rgb iron{ 0.26f, 0.26f, 0.32f };
		pt.Shape(RoundBox(p, 64, 244, 15, 6, 3), iron);
		pt.Shape(RoundBox(p, 64, 172, 4, 76, 2), iron);
		pt.Shape(RoundBox(p, 64, 96, 13, 4, 1), iron);
		float lamp = RoundBox(p, 64, 70, 17, 23, 5);
		pt.Shape(lamp, kWarm);
		if (lamp <= -2.2f && std::abs(p.x - 64) < 1.5f) pt.c = iron;
		pt.Detail(Circle(p, 64, 72, 7), { 1.0f, 0.97f, 0.75f });
		pt.Shape(Poly(p, { { 64, 30 }, { 40, 50 }, { 88, 50 } }), iron);
	}

	void GrassTuft(Painter& pt, V2 q) {
		for (int k = 0; k < 7; ++k) {
			float bx = 28 + k * 12.0f, tx = bx + (k - 3) * 6.0f, ty = 46 + (k % 3) * 16.0f;
			pt.Shape(Poly(q, { { bx - 6, 126 }, { bx + 6, 126 }, { tx, ty } }), (k % 2) ? kLeaf : kLeafLite, 1.6f);
		}
	}

	void StarProp(Painter& pt, V2 q) {
		float s = Star5(q, 64, 66, 48, 0.5f);
		pt.Shape(s, { 1.0f, 0.88f, 0.40f });
		pt.Detail(std::max(Star5(q, 58, 58, 22, 0.5f), s + 4.0f), { 1.0f, 0.95f, 0.70f });
		pt.Detail(Ellipse(q, 56, 68, 2.6f, 3.6f), kInk);
		pt.Detail(Ellipse(q, 72, 68, 2.6f, 3.6f), kInk);
		pt.Detail(Sub(Circle(q, 64, 72, 6), Circle(q, 64, 69, 6)), kInk);
	}

	void PetalProp(Painter& pt, V2 q) {
		pt.Shape(EllipseRot(q, 64, 64, 40, 20, 0.6f), { 1.0f, 0.78f, 0.86f }, 1.8f);
		pt.Detail(Segment(q, 34, 44, 92, 84, 1.4f), { 1.0f, 0.90f, 0.94f });
	}

	void FireflyProp(Painter& pt, V2 q) {
		pt.Fill(Circle(q, 64, 64, 34), { 1.0f, 0.97f, 0.66f });
		pt.Fill(Circle(q, 64, 64, 18), { 1.0f, 1.0f, 0.88f });
	}

	void CloudProp(Painter& pt, V2 q) {
		float c = std::max(std::min({ Circle(q, 34, 80, 26), Circle(q, 62, 60, 34), Circle(q, 94, 74, 28), Circle(q, 64, 88, 28) }), q.y - 104.0f);
		pt.Shape(c, { 1.0f, 1.0f, 1.0f }, 1.8f);
		pt.Detail(std::max(c + 3.0f, 96.0f - q.y), { 0.88f, 0.92f, 0.98f });
	}

	void SunProp(Painter& pt, V2 q) {
		float dx = q.x - 64, dy = q.y - 64, r = Len(dx, dy), a = std::atan2(dy, dx);
		float ray = std::max({ r - 61.0f, 43.0f - r, (0.35f - std::cos(a * 12.0f)) * 9.0f });
		pt.Shape(ray, { 1.0f, 0.66f, 0.24f }, 1.8f);
		pt.Shape(Circle(q, 64, 64, 40), { 1.0f, 0.85f, 0.30f });
		pt.Detail(Sub(Circle(q, 50, 62, 6), Circle(q, 50, 58, 6)), kInk);                // 감은 눈
		pt.Detail(Sub(Circle(q, 78, 62, 6), Circle(q, 78, 58, 6)), kInk);
		pt.Detail(std::max(Sub(Circle(q, 64, 70, 13), Circle(q, 64, 66, 13)), 72.0f - q.y), kInk); // 웃는 입
		pt.Detail(Circle(q, 42, 74, 5), { 1.0f, 0.62f, 0.45f });
		pt.Detail(Circle(q, 86, 74, 5), { 1.0f, 0.62f, 0.45f });
	}

	void MoonProp(Painter& pt, V2 q) {
		float m = Sub(Circle(q, 60, 64, 48), Circle(q, 84, 50, 42));
		pt.Shape(m, { 1.0f, 0.95f, 0.70f });
		pt.Detail(Sub(Circle(q, 36, 66, 5), Circle(q, 36, 62, 5)), kInk);                // 졸린 눈
		pt.Detail(Circle(q, 30, 80, 4), { 1.0f, 0.78f, 0.62f });
		pt.Detail(Circle(q, 44, 92, 4), { 0.92f, 0.86f, 0.60f });                         // 분화구
		pt.Detail(Circle(q, 56, 104, 3), { 0.92f, 0.86f, 0.60f });
	}

	void DrawProp(int prop, Painter& pt) {
		V2 q{ pt.p.x, pt.p.y - 128.0f };   // 정사각 소품용 좌표 (칸 아래쪽 128x128)
		switch (prop) {
		case StoryArt::Daisy: Daisy(pt); break;
		case StoryArt::Tulips: Tulips(pt); break;
		case StoryArt::Mushroom: Mushroom(pt, q); break;
		case StoryArt::Bush: Bush(pt, q); break;
		case StoryArt::Pine: Pine(pt); break;
		case StoryArt::RoundTree: RoundTree(pt); break;
		case StoryArt::Cottage: Cottage(pt); break;
		case StoryArt::Fence: Fence(pt, q); break;
		case StoryArt::LampPost: LampPost(pt); break;
		case StoryArt::GrassTuft: GrassTuft(pt, q); break;
		case StoryArt::Star: StarProp(pt, q); break;
		case StoryArt::Petal: PetalProp(pt, q); break;
		case StoryArt::Firefly: FireflyProp(pt, q); break;
		case StoryArt::Cloud: CloudProp(pt, q); break;
		case StoryArt::Sun: SunProp(pt, q); break;
		default: MoonProp(pt, q); break;
		}
	}

	// ================= 배경 판 (칸 좌표: x 0~512, y 0~256, 아래가 땅) =================
	// 오려 낸 흰 여백(margin) + 얇은 잉크 선 + 색 면
	void Cutout(Painter& pt, float shape, Rgb col, float margin = 8.0f) {
		pt.Fill(shape - margin, { 0.99f, 0.98f, 0.95f });
		pt.Shape(shape, col, 2.0f);
	}

	void HillsBack(Painter& pt) {
		V2 p = pt.p;
		float h = std::min({ Circle(p, 110, 340, 168), Circle(p, 300, 372, 196), Circle(p, 470, 340, 156) });
		Cutout(pt, h, { 0.66f, 0.82f, 0.52f });
		if (h <= -2.0f && h > -14.0f) pt.c = { 0.76f, 0.88f, 0.60f };                      // 위쪽 밝은 띠
		for (int k = 0; k < 14; ++k) {
			float fx = 30 + k * 34.0f, fy = 230 - (k % 3) * 14.0f;
			if (h <= -16.0f && Circle(p, fx, fy, 3.2f) <= 0.0f) pt.c = (k % 2) ? Rgb{ 1.0f, 0.95f, 0.70f } : Rgb{ 1.0f, 0.78f, 0.84f };
		}
	}

	void MountainsBack(Painter& pt) {
		V2 p = pt.p;
		struct M { float px, py, l, r; Rgb c; };
		const M ms[] = { { 120, 70, 0, 250, { 0.64f, 0.68f, 0.84f } }, { 440, 92, 320, 560, { 0.60f, 0.64f, 0.80f } }, { 290, 36, 140, 450, { 0.56f, 0.60f, 0.78f } } };
		float all = 1e9f;
		for (const M& m : ms) all = std::min(all, Poly(p, { { m.px, m.py }, { m.l, 262 }, { m.r, 262 } }));
		pt.Fill(all - 8.0f, { 0.99f, 0.98f, 0.95f });
		for (const M& m : ms) {
			float d = Poly(p, { { m.px, m.py }, { m.l, 262 }, { m.r, 262 } });
			pt.Shape(d, m.c, 2.0f);
			float snow = m.py + 42.0f + 6.0f * std::sin(p.x / 7.0f);
			if (d <= -2.0f && p.y < snow) pt.c = { 0.97f, 0.97f, 1.0f };
			if (d <= -2.0f && p.y >= snow && p.x > m.px) pt.c = Mix(m.c, { 0.4f, 0.42f, 0.6f }, 0.18f);   // 그늘 면
		}
	}

	void ForestBack(Painter& pt) {
		V2 p = pt.p;
		float all = RoundBox(p, 256, 238, 260, 22, 0);
		for (int k = 0; k < 9; ++k) all = std::min(all, Circle(p, 30 + k * 58.0f, 168 + (k % 2) * 20.0f, 44));
		for (int k = 0; k < 8; ++k) {
			float x = 58 + k * 58.0f;
			all = std::min(all, Poly(p, { { x, 96 + (k % 3) * 10.0f }, { x - 30, 200 }, { x + 30, 200 } }));
		}
		pt.Fill(all - 8.0f, { 0.99f, 0.98f, 0.95f });
		pt.Shape(RoundBox(p, 256, 238, 260, 22, 0), { 0.42f, 0.62f, 0.46f }, 2.0f);
		for (int k = 0; k < 8; ++k) {
			float x = 58 + k * 58.0f;
			pt.Shape(Poly(p, { { x, 96 + (k % 3) * 10.0f }, { x - 30, 200 }, { x + 30, 200 } }), { 0.40f, 0.60f, 0.46f }, 2.0f);
		}
		for (int k = 0; k < 9; ++k) {
			Rgb c = (k % 2) ? Rgb{ 0.52f, 0.72f, 0.52f } : Rgb{ 0.47f, 0.68f, 0.50f };
			float d = Circle(p, 30 + k * 58.0f, 168 + (k % 2) * 20.0f, 44);
			pt.Shape(d, c, 2.0f);
			if (d <= -2.0f && Circle(p, 18 + k * 58.0f, 152 + (k % 2) * 20.0f, 18) <= 0.0f) pt.c = Mix(c, kWhite, 0.18f);
		}
	}

	void TownBack(Painter& pt) {
		V2 p = pt.p;
		Rgb wall{ 0.92f, 0.88f, 0.80f }, roofP{ 0.62f, 0.44f, 0.66f }, roofR{ 0.80f, 0.42f, 0.34f }, win{ 1.0f, 0.86f, 0.45f };
		// 실루엣(흰 여백용)
		float keep = RoundBox(p, 256, 190, 74, 70, 2);
		float towerL = RoundBox(p, 170, 160, 26, 100, 2), towerR = RoundBox(p, 342, 160, 26, 100, 2);
		float coneL = Poly(p, { { 170, 20 }, { 138, 64 }, { 202, 64 } }), coneR = Poly(p, { { 342, 20 }, { 310, 64 }, { 374, 64 } });
		float coneM = Poly(p, { { 256, 54 }, { 222, 124 }, { 290, 124 } });
		float h1 = RoundBox(p, 62, 222, 40, 38, 2), r1 = Poly(p, { { 62, 140 }, { 14, 186 }, { 110, 186 } });
		float h2 = RoundBox(p, 450, 226, 40, 34, 2), r2 = Poly(p, { { 450, 150 }, { 402, 194 }, { 498, 194 } });
		float all = std::min({ keep, towerL, towerR, coneL, coneR, coneM, h1, r1, h2, r2 });
		pt.Fill(all - 8.0f, { 0.99f, 0.98f, 0.95f });
		pt.Shape(h1, wall, 2.0f); pt.Shape(r1, roofR, 2.0f);
		pt.Shape(h2, wall, 2.0f); pt.Shape(r2, roofR, 2.0f);
		pt.Shape(towerL, wall, 2.0f); pt.Shape(towerR, wall, 2.0f);
		pt.Shape(keep, wall, 2.0f);
		pt.Shape(coneL, roofP, 2.0f); pt.Shape(coneR, roofP, 2.0f); pt.Shape(coneM, roofP, 2.0f);
		pt.Shape(Segment(p, 170, 22, 170, 2, 1.4f), kInk, 0.0f);                              // 깃대·깃발
		pt.Shape(Poly(p, { { 171, 4 }, { 194, 9 }, { 171, 14 } }), { 0.90f, 0.30f, 0.30f }, 1.2f);
		pt.Shape(Segment(p, 342, 22, 342, 2, 1.4f), kInk, 0.0f);
		pt.Shape(Poly(p, { { 343, 4 }, { 366, 9 }, { 343, 14 } }), { 0.95f, 0.75f, 0.25f }, 1.2f);
		// 창문 (밤에 불 켜진 느낌)
		const float wins[][4] = { { 170, 90, 7, 11 }, { 342, 90, 7, 11 }, { 170, 150, 7, 11 }, { 342, 150, 7, 11 },
			{ 226, 170, 9, 13 }, { 286, 170, 9, 13 }, { 62, 218, 9, 9 }, { 450, 224, 9, 9 } };
		for (auto& w : wins) pt.Detail(RoundBox(p, w[0], w[1], w[2], w[3], 3), win);
		pt.Detail(RoundBox(p, 256, 232, 18, 28, 14), { 0.50f, 0.34f, 0.24f });               // 성문
	}

	void DrawBackdrop(int index, Painter& pt) {
		switch (index) {
		case StoryArt::Hills: HillsBack(pt); break;
		case StoryArt::Mountains: MountainsBack(pt); break;
		case StoryArt::Forest: ForestBack(pt); break;
		default: TownBack(pt); break;
		}
	}
}

bool StoryArt::IsSquareProp(int prop) {
	switch (prop) {
	case Mushroom: case Bush: case Fence: case GrassTuft: case Star: case Petal: case Firefly: case Cloud: case Sun: case Moon:
		return true;
	default:
		return false;
	}
}

std::vector<unsigned char> StoryArt::DrawPropAtlas() {
	Canvas cv(kAtlasW, kAtlasH);
	for (int y = 0; y < kAtlasH; ++y)
		for (int x = 0; x < kAtlasW; ++x) {
			int col = x / kPropCellW, row = y / kPropCellH;
			int prop = row * kPropCols + col;
			if (prop >= kPropCount) continue;
			Painter pt;
			pt.p = { x - col * kPropCellW + 0.5f, y - row * kPropCellH + 0.5f };
			DrawProp(prop, pt);
			float outline = (prop == Firefly) ? 0.0f : 3.0f;
			cv.PutPainter(x, y, pt, outline, 0.05f);
		}
	return cv.px;
}

std::vector<unsigned char> StoryArt::DrawBackdropAtlas() {
	Canvas cv(kAtlasW, kAtlasH);
	for (int y = 0; y < kAtlasH; ++y)
		for (int x = 0; x < kAtlasW; ++x) {
			int col = x / kBackCellW, row = y / kBackCellH;
			Painter pt;
			pt.p = { x - col * kBackCellW + 0.5f, y - row * kBackCellH + 0.5f };
			DrawBackdrop(row * kBackCols + col, pt);
			cv.PutPainter(x, y, pt, 0.0f, 0.06f);
		}
	return cv.px;
}

// 책장 글줄: 크림색 종이에 옅은 회색 글줄 (단어 길이는 무작위), 가로세로 반복
std::vector<unsigned char> StoryArt::DrawPageLines() {
	const int N = 256;
	Canvas cv(N, N);
	for (int y = 0; y < N; ++y) {
		int line = y / 16, inLine = y % 16;
		for (int x = 0; x < N; ++x) {
			float v = 0.96f + 0.04f * Grain(x * 1.0f, y * 1.0f);
			if (inLine >= 9 && inLine <= 11) {
				int word = (x + line * 37) / 22;
				int gap = (x + line * 37) % 22;
				bool ink = gap < 14 + static_cast<int>(6 * Grain(word * 7.0f, line * 3.0f));
				if (ink) v *= 0.80f;
			}
			cv.Put(x, y, { v, v * 0.98f, v * 0.94f });
		}
	}
	return cv.px;
}

std::vector<unsigned char> StoryArt::DrawFringe() {
	Canvas cv(kFringeW, kFringeH);
	for (int y = 0; y < kFringeH; ++y)
		for (int x = 0; x < kFringeW; ++x) {
			float fx = x + 0.5f, fy = y + 0.5f;
			float xm = std::fmod(fx, 16.0f) - 8.0f;
			float edge = 10.0f + std::sqrt(std::max(0.0f, 64.0f - xm * xm));   // 물결 아래쪽 끝
			if (fy > edge) continue;
			Rgb c{ 0.50f, 0.72f, 0.38f };
			if (fy > edge - 2.0f) c = { 0.30f, 0.48f, 0.25f };                  // 가장자리 선
			else if (fy < 3.0f) c = { 0.64f, 0.84f, 0.50f };                     // 위쪽 밝은 줄
			cv.Put(x, y, c);
		}
	return cv.px;
}
