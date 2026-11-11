#include "CharacterArt.h"
#include "SdfPaint.h"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <initializer_list>
using namespace std;

using namespace SdfPaint;

namespace {
	// ---------------- 시안 1: 종이 인형 개선판 ----------------
	// 책에서 오려 낸 듯한 흰 여백 + 점선, 빨간 베레모·목도리, 노란 우비
	Rgb DrawPaperDoll(Painter& pt, bool& outlined) {
		V2 p = pt.p;
		float head = Circle(p, 64, 56, 27);
		float body = RoundBox(p, 64, 122, 26, 37, 12);
		float legL = RoundBox(p, 54, 168, 7, 12, 5), legR = RoundBox(p, 74, 168, 7, 12, 5);
		float armL = Segment(p, 41, 100, 31, 128, 6), armR = Segment(p, 87, 100, 97, 126, 6);
		float beret = Ellipse(p, 70, 30, 25, 9);
		float figure = std::min({ head, body, legL, legR, armL, armR, beret });

		// 오려 낸 여백: 흰 종이 + 바깥쪽 점선
		float margin = figure - 7.0f;
		if (margin <= 0.0f) {
			pt.on = true;
			bool dash = ((static_cast<int>((p.x + p.y * 0.8f) / 4.0f)) & 1) == 0;
			pt.c = (margin > -2.0f && dash) ? Rgb{ 0.55f, 0.55f, 0.55f } : Rgb{ 0.99f, 0.98f, 0.95f };
		}
		pt.Shape(armL, kSkin); pt.Shape(armR, kSkin);
		pt.Shape(legL, { 0.26f, 0.31f, 0.48f }); pt.Shape(legR, { 0.26f, 0.31f, 0.48f });
		pt.Detail(RoundBox(p, 54, 178, 8, 4, 3), { 0.22f, 0.18f, 0.16f });
		pt.Detail(RoundBox(p, 74, 178, 8, 4, 3), { 0.22f, 0.18f, 0.16f });
		pt.Shape(RoundBox(p, 40, 106, 5, 13, 4), { 0.86f, 0.2f, 0.2f });                 // 목도리 꼬리
		pt.Shape(body, { 0.98f, 0.80f, 0.26f });
		pt.Detail(Segment(p, 64, 94, 64, 156, 1.0f), { 0.75f, 0.55f, 0.15f });              // 우비 솔기
		for (float by : { 110.0f, 126.0f, 142.0f }) pt.Detail(Circle(p, 70, by, 2.6f), { 0.45f, 0.32f, 0.12f });
		pt.Shape(RoundBox(p, 64, 89, 25, 6, 4), { 0.86f, 0.2f, 0.2f });                    // 목도리
		pt.Shape(head, kSkin);
		pt.Detail(std::max(Circle(p, 64, 52, 28.5f), p.y - 46.0f), { 0.42f, 0.27f, 0.18f }); // 머리카락
		pt.Shape(beret, { 0.80f, 0.18f, 0.20f });
		pt.Shape(Circle(p, 72, 19, 3.5f), { 0.80f, 0.18f, 0.20f });
		Eye(pt, 78, 58);
		pt.Detail(Circle(p, 86, 68, 4.5f), kBlush);
		pt.Detail(Segment(p, 81, 74, 87, 73, 1.1f), kInk);
		outlined = true;
		return pt.c;
	}

	// ---------------- 시안 2: 꼬마 책 ----------------
	// 빨간 표지가 몸통, 오른쪽에 페이지 단면, 표지 위 큰 눈, 초록 책갈피 끈
	Rgb DrawLittleBook(Painter& pt, bool& outlined) {
		V2 p = pt.p;
		Rgb red{ 0.74f, 0.20f, 0.18f }, darkRed{ 0.52f, 0.13f, 0.12f }, gold{ 0.96f, 0.78f, 0.30f }, cream{ 0.98f, 0.95f, 0.86f };
		pt.Shape(Segment(p, 28, 104, 16, 126, 4.5f), darkRed);
		pt.Shape(Segment(p, 96, 100, 110, 120, 4.5f), darkRed);
		pt.Shape(RoundBox(p, 48, 172, 6, 10, 4), { 0.35f, 0.25f, 0.18f });
		pt.Shape(RoundBox(p, 70, 172, 6, 10, 4), { 0.35f, 0.25f, 0.18f });
		float pages = RoundBox(p, 95, 116, 6, 46, 2);
		pt.Shape(pages, cream);
		if (pages <= -2.2f && std::fmod(p.y, 4.0f) < 1.0f) pt.c = { 0.72f, 0.70f, 0.64f };        // 페이지 결
		pt.Shape(Segment(p, 77, 66, 84, 40, 3.0f), { 0.25f, 0.60f, 0.40f });                         // 책갈피 끈
		pt.Shape(Poly(p, { { 78, 30 }, { 92, 34 }, { 84, 44 } }), { 0.25f, 0.60f, 0.40f });
		float cover = RoundBox(p, 58, 114, 34, 50, 6);
		pt.Shape(cover, red);
		pt.Detail(std::max(cover, p.x - 32.0f), darkRed);                                            // 책등
		float frame = RoundBox(p, 61, 114, 25, 42, 4);
		if (frame <= 0.0f && frame > -2.5f && cover <= 0.0f) pt.c = gold;                           // 금색 테두리
		for (float ex : { 52.0f, 78.0f }) {
			float eye = Circle(p, ex, 90, 9.5f);
			pt.Detail(eye, kWhite);
			if (eye <= 0.0f && eye > -1.4f) pt.c = kInk;
			pt.Detail(Circle(p, ex + 3.5f, 91, 4.5f), kInk);
			pt.Detail(Circle(p, ex + 4.5f, 89, 1.4f), kWhite);
		}
		pt.Detail(Segment(p, 62, 108, 70, 108, 1.3f), kInk);
		pt.Detail(RoundBox(p, 64, 134, 15, 8, 2), cream);                                           // 제목 딱지
		pt.Detail(RoundBox(p, 64, 131, 10, 0.8f, 0.4f), { 0.55f, 0.5f, 0.45f });
		pt.Detail(RoundBox(p, 62, 137, 7, 0.8f, 0.4f), { 0.55f, 0.5f, 0.45f });
		outlined = true;
		return pt.c;
	}

	// ---------------- 시안 3: 종이접기 여우 ----------------
	// 접힌 면마다 밝기가 다른 각진 여우, 꼬리는 뒤(왼쪽)
	Rgb DrawOrigamiFox(Painter& pt, bool& outlined) {
		V2 p = pt.p;
		Rgb o{ 0.95f, 0.55f, 0.20f }, l{ 1.0f, 0.72f, 0.40f }, cr{ 1.0f, 0.95f, 0.86f }, dk{ 0.78f, 0.40f, 0.14f };
		pt.Shape(Poly(p, { { 42, 118 }, { 6, 104 }, { 14, 150 }, { 46, 150 } }), o);                  // 꼬리
		pt.Detail(Poly(p, { { 6, 104 }, { 26, 110 }, { 14, 132 } }), cr);
		pt.Detail(Poly(p, { { 26, 110 }, { 42, 118 }, { 46, 150 }, { 14, 132 } }), l);
		pt.Shape(Poly(p, { { 48, 164 }, { 58, 164 }, { 56, 186 }, { 46, 186 } }), dk);               // 다리
		pt.Shape(Poly(p, { { 68, 164 }, { 78, 164 }, { 80, 186 }, { 70, 186 } }), dk);
		pt.Shape(Poly(p, { { 44, 86 }, { 84, 86 }, { 92, 140 }, { 64, 172 }, { 36, 140 } }), o);     // 몸
		pt.Detail(Poly(p, { { 44, 86 }, { 62, 92 }, { 36, 140 } }), l);
		pt.Detail(Poly(p, { { 56, 98 }, { 80, 98 }, { 66, 152 } }), cr);                             // 배
		pt.Shape(Poly(p, { { 38, 48 }, { 46, 12 }, { 62, 44 } }), o);                                  // 귀
		pt.Shape(Poly(p, { { 58, 44 }, { 74, 8 }, { 82, 46 } }), o);
		pt.Detail(Poly(p, { { 44, 44 }, { 47, 24 }, { 56, 44 } }), { 0.45f, 0.20f, 0.10f });
		pt.Detail(Poly(p, { { 64, 43 }, { 73, 20 }, { 77, 44 } }), { 0.45f, 0.20f, 0.10f });
		pt.Shape(Poly(p, { { 32, 48 }, { 70, 40 }, { 112, 66 }, { 70, 90 }, { 36, 76 } }), o);        // 머리
		pt.Detail(Poly(p, { { 32, 48 }, { 70, 40 }, { 60, 64 } }), l);
		pt.Detail(Poly(p, { { 70, 90 }, { 112, 66 }, { 78, 70 } }), cr);                             // 주둥이 아래
		pt.Detail(Segment(p, 70, 40, 70, 88, 0.8f), dk);                                              // 접힌 선
		pt.Detail(Segment(p, 60, 64, 78, 70, 0.8f), dk);
		pt.Detail(Segment(p, 64, 92, 64, 168, 0.8f), dk);
		pt.Detail(Ellipse(p, 84, 60, 3.0f, 4.2f), kInk);
		pt.Detail(Circle(p, 108, 66, 3.2f), kInk);                                                    // 코
		outlined = true;
		return pt.c;
	}

	// ---------------- 시안 4: 책갈피 요정 ----------------
	// 민트색 책갈피 몸, 위에 구멍과 금색 술, 아래는 V자로 잘린 끝
	Rgb DrawBookmark(Painter& pt, bool& outlined) {
		V2 p = pt.p;
		Rgb teal{ 0.33f, 0.70f, 0.68f }, lite{ 0.55f, 0.84f, 0.80f }, deep{ 0.18f, 0.42f, 0.42f };
		Rgb gold{ 0.96f, 0.76f, 0.25f }, gold2{ 0.90f, 0.62f, 0.18f };
		pt.Shape(Poly(p, { { 86, 22 }, { 96, 22 }, { 102, 48 }, { 82, 48 } }), gold2);                // 술
		if (Poly(p, { { 86, 22 }, { 96, 22 }, { 102, 48 }, { 82, 48 } }) <= -2.2f && std::fmod(p.x, 4.0f) < 1.2f) pt.c = { 0.76f, 0.50f, 0.12f };
		pt.Shape(Segment(p, 64, 42, 90, 20, 1.8f), gold, 0.6f);                                       // 끈
		pt.Shape(Circle(p, 91, 20, 5.0f), gold);
		pt.Shape(Segment(p, 41, 106, 27, 126, 5.0f), teal);                                           // 팔
		pt.Shape(Segment(p, 87, 104, 101, 122, 5.0f), teal);
		pt.Shape(Ellipse(p, 48, 181, 9, 5), deep);                                                    // 발
		pt.Shape(Ellipse(p, 80, 181, 9, 5), deep);
		float body = Sub(RoundBox(p, 64, 104, 26, 70, 10), Poly(p, { { 34, 200 }, { 94, 200 }, { 64, 166 } }));
		pt.Shape(body, teal);
		if (body <= -2.2f && std::fmod(p.x + p.y, 16.0f) < 5.0f) pt.c = lite;                        // 사선 줄무늬
		pt.Detail(RoundBox(p, 64, 82, 18, 14, 7), lite);                                              // 얼굴 칸
		Eye(pt, 70, 80, 3.4f, 4.6f);
		Eye(pt, 84, 80, 3.4f, 4.6f);
		pt.Detail(Circle(p, 63, 89, 3.2f), kBlush);
		pt.Detail(Circle(p, 90, 89, 3.2f), kBlush);
		pt.Detail(Segment(p, 75, 90, 80, 90, 1.1f), kInk);
		pt.Detail(Poly(p, { { 64, 116 }, { 73, 127 }, { 64, 138 }, { 55, 127 } }), gold);             // 별 장식
		pt.Cut(Circle(p, 64, 46, 5.5f));                                                              // 구멍
		outlined = true;
		return pt.c;
	}

	// ---------------- 시안 5: 신문지 모자 소년 ----------------
	// 신문지로 접은 모자, 페이지 망토, 파란 옷, 연필 칼
	Rgb DrawPaperHatKid(Painter& pt, bool& outlined) {
		V2 p = pt.p;
		Rgb cream{ 0.97f, 0.94f, 0.84f }, blue{ 0.30f, 0.48f, 0.78f }, news{ 0.93f, 0.93f, 0.90f };
		float cape = Poly(p, { { 46, 88 }, { 72, 88 }, { 60, 172 }, { 12, 160 } });
		pt.Shape(cape, cream);
		if (cape <= -2.2f && std::fmod(p.y, 7.0f) < 1.3f && p.x > 22 && p.x < 58) pt.c = { 0.62f, 0.60f, 0.55f }; // 글줄
		pt.Shape(RoundBox(p, 56, 170, 7, 11, 4), { 0.30f, 0.22f, 0.16f });
		pt.Shape(RoundBox(p, 74, 170, 7, 11, 4), { 0.30f, 0.22f, 0.16f });
		pt.Shape(RoundBox(p, 64, 128, 22, 32, 9), blue);
		pt.Detail(RoundBox(p, 64, 141, 22, 3, 1), { 0.45f, 0.30f, 0.18f });                          // 허리띠
		pt.Shape(Circle(p, 66, 66, 26), kSkin);
		pt.Detail(std::max(Circle(p, 66, 64, 27.5f), p.y - 56.0f), { 0.38f, 0.25f, 0.16f });          // 머리카락
		float hat = Poly(p, { { 30, 50 }, { 102, 50 }, { 66, 8 } });
		pt.Shape(hat, news);
		if (hat <= -2.2f && std::fmod(p.y, 6.0f) < 1.2f && std::abs(p.x - 66) < (p.y - 8) * 0.7f) pt.c = { 0.62f, 0.62f, 0.60f };
		pt.Shape(RoundBox(p, 66, 50, 37, 5, 2), { 0.84f, 0.84f, 0.80f });                             // 모자 챙
		Eye(pt, 80, 68);
		pt.Detail(Circle(p, 88, 78, 4.2f), kBlush);
		pt.Detail(Segment(p, 83, 84, 89, 83, 1.1f), kInk);
		pt.Shape(Segment(p, 104, 100, 123, 78, 3.6f), { 0.98f, 0.82f, 0.25f });                      // 연필
		pt.Detail(Circle(p, 122, 79, 3.0f), { 0.40f, 0.30f, 0.20f });
		pt.Shape(Segment(p, 80, 114, 101, 102, 5.0f), blue);                                          // 팔
		pt.Shape(Circle(p, 104, 101, 5.5f), kSkin);                                                   // 손
		outlined = true;
		return pt.c;
	}
}

const char* CharacterArt::Name(int style) {
	static const char8_t* names[kCount] = { u8"종이 인형 개선판", u8"꼬마 책", u8"종이접기 여우", u8"책갈피 요정", u8"신문지 모자 소년" };
	return reinterpret_cast<const char*>(names[((style % kCount) + kCount) % kCount]);
}

std::vector<unsigned char> CharacterArt::Draw(int style) {
	vector<unsigned char> rgba(static_cast<size_t>(kWidth) * kHeight * 4, 0);
	for (int py = 0; py < kHeight; ++py)
		for (int px = 0; px < kWidth; ++px) {
			Painter pt;
			pt.p = { px + 0.5f, py + 0.5f };
			bool outlined = false;
			switch (((style % kCount) + kCount) % kCount) {
			case 0: DrawPaperDoll(pt, outlined); break;
			case 1: DrawLittleBook(pt, outlined); break;
			case 2: DrawOrigamiFox(pt, outlined); break;
			case 3: DrawBookmark(pt, outlined); break;
			default: DrawPaperHatKid(pt, outlined); break;
			}
			if (!pt.on) continue;
			Rgb c = pt.c;
			if (outlined && pt.sil <= 0.0f && pt.sil > -3.2f) c = kInk;                // 바깥 외곽선 (조금 굵게)
			float g = 0.94f + 0.06f * Grain(pt.p.x, pt.p.y);                             // 종이 결
			size_t i = (static_cast<size_t>(kHeight - 1 - py) * kWidth + px) * 4;       // 아래 줄부터 저장
			rgba[i] = static_cast<unsigned char>(std::clamp(c.r * g, 0.0f, 1.0f) * 255);
			rgba[i + 1] = static_cast<unsigned char>(std::clamp(c.g * g, 0.0f, 1.0f) * 255);
			rgba[i + 2] = static_cast<unsigned char>(std::clamp(c.b * g, 0.0f, 1.0f) * 255);
			rgba[i + 3] = 255;
		}
	return rgba;
}
