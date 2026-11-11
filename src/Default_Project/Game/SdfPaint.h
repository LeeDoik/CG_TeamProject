#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <initializer_list>

// 그림을 코드로 그리는 도구: 거리 함수(0 이하가 도형 안쪽)로 도형을 만들고,
// Painter로 뒤에 칠한 것이 앞에 오도록 겹쳐 칠한다. (캐릭터·소품·배경 그림이 함께 쓴다)
namespace SdfPaint {
	struct V2 { float x, y; };
	struct Rgb { float r, g, b; };

	inline float Len(float x, float y) { return std::sqrt(x * x + y * y); }
	inline float Circle(V2 p, float cx, float cy, float r) { return Len(p.x - cx, p.y - cy) - r; }
	inline float Ellipse(V2 p, float cx, float cy, float rx, float ry) {
		return (Len((p.x - cx) / rx, (p.y - cy) / ry) - 1.0f) * std::min(rx, ry);
	}
	// 기울인 타원 (angle: 라디안, 화면 기준 시계 방향)
	inline float EllipseRot(V2 p, float cx, float cy, float rx, float ry, float angle) {
		float c = std::cos(angle), s = std::sin(angle);
		float dx = p.x - cx, dy = p.y - cy;
		return Ellipse({ c * dx + s * dy, -s * dx + c * dy }, 0, 0, rx, ry);
	}
	inline float RoundBox(V2 p, float cx, float cy, float hx, float hy, float r) {
		float qx = std::abs(p.x - cx) - hx + r, qy = std::abs(p.y - cy) - hy + r;
		return Len(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
	}
	inline float Segment(V2 p, float ax, float ay, float bx, float by, float r) {
		float px = p.x - ax, py = p.y - ay, dx = bx - ax, dy = by - ay;
		float h = std::clamp((px * dx + py * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
		return Len(px - dx * h, py - dy * h) - r;
	}
	// 볼록 다각형 (꼭짓점 순서는 어느 방향이든 된다)
	inline float Poly(V2 p, std::initializer_list<V2> pts) {
		std::vector<V2> v(pts);
		float area = 0.0f;
		for (size_t i = 0; i < v.size(); ++i) {
			const V2& a = v[i]; const V2& b = v[(i + 1) % v.size()];
			area += a.x * b.y - b.x * a.y;
		}
		float s = (area > 0.0f) ? 1.0f : -1.0f;
		float d = -1e9f;
		for (size_t i = 0; i < v.size(); ++i) {
			const V2& a = v[i]; const V2& b = v[(i + 1) % v.size()];
			float ex = b.x - a.x, ey = b.y - a.y, l = Len(ex, ey);
			float nx = s * ey / l, ny = -s * ex / l;
			d = std::max(d, (p.x - a.x) * nx + (p.y - a.y) * ny);
		}
		return d;
	}
	// 별 다섯 꼭지 (r: 바깥 반지름, rf: 오목한 정도 0.4~0.6)
	inline float Star5(V2 p, float cx, float cy, float r, float rf) {
		float x = std::abs(p.x - cx), y = cy - p.y;   // 위쪽이 +y
		const float k1x = 0.809016994f, k1y = -0.587785252f, k2x = -k1x, k2y = k1y;
		float d1 = std::max(k1x * x + k1y * y, 0.0f); x -= 2 * d1 * k1x; y -= 2 * d1 * k1y;
		float d2 = std::max(k2x * x + k2y * y, 0.0f); x -= 2 * d2 * k2x; y -= 2 * d2 * k2y;
		x = std::abs(x); y -= r;
		float bax = rf * -k1y, bay = rf * k1x - 1.0f;
		float h = std::clamp((x * bax + y * bay) / (bax * bax + bay * bay), 0.0f, r);
		float dx = x - bax * h, dy = y - bay * h;
		float sign = (y * bax - x * bay) > 0 ? 1.0f : -1.0f;
		return Len(dx, dy) * sign;
	}
	inline float Sub(float a, float b) { return std::max(a, -b); }

	inline float Grain(float x, float y) {
		uint32_t h = static_cast<uint32_t>(x * 0.5f) * 374761393u + static_cast<uint32_t>(y * 0.5f) * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
	}

	const Rgb kInk{ 0.13f, 0.11f, 0.10f };
	const Rgb kWhite{ 1.0f, 1.0f, 1.0f };
	const Rgb kSkin{ 1.0f, 0.89f, 0.79f };
	const Rgb kBlush{ 1.0f, 0.70f, 0.70f };

	inline Rgb Mix(Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

	// 한 픽셀을 칠하는 붓: 뒤에 칠한 도형이 앞에 온다
	struct Painter {
		V2 p{ 0, 0 };
		Rgb c{ 0, 0, 0 };
		bool on = false;
		float sil = 1e9f;           // 실루엣 거리 (바깥 외곽선용)

		// 외곽선이 있는 도형 (실루엣에 포함)
		void Shape(float d, Rgb col, float line = 2.2f) {
			sil = std::min(sil, d);
			if (d > 0.0f) return;
			on = true;
			c = (d > -line) ? kInk : col;
		}
		// 이미 칠한 곳 위에만 그리는 무늬·얼굴 (외곽선 없음)
		void Detail(float d, Rgb col) { if (d <= 0.0f && on) c = col; }
		// 아무 데나 칠하기 (외곽선 없음, 실루엣에 포함 안 함) — 오려 낸 흰 여백 등
		void Fill(float d, Rgb col) { if (d <= 0.0f) { on = true; c = col; } }
		// 구멍 뚫기
		void Cut(float d) { if (d <= 0.0f) on = false; sil = Sub(sil, d); }
	};

	inline void Eye(Painter& pt, float x, float y, float rx = 3.6f, float ry = 5.0f) {
		pt.Detail(Ellipse(pt.p, x, y, rx, ry), kInk);
		pt.Detail(Circle(pt.p, x + 1.0f, y - 2.0f, 1.3f), kWhite);
	}

	// RGBA 이미지 (위 줄부터 칠하고, 내보낼 때 OpenGL 순서(아래 줄부터)로 뒤집는다)
	struct Canvas {
		int w, h;
		std::vector<unsigned char> px;
		Canvas(int w, int h) : w(w), h(h), px(static_cast<size_t>(w) * h * 4, 0) {}
		void Put(int x, int y, Rgb c, float a = 1.0f) {
			size_t i = (static_cast<size_t>(h - 1 - y) * w + x) * 4;
			px[i] = static_cast<unsigned char>(std::clamp(c.r, 0.0f, 1.0f) * 255);
			px[i + 1] = static_cast<unsigned char>(std::clamp(c.g, 0.0f, 1.0f) * 255);
			px[i + 2] = static_cast<unsigned char>(std::clamp(c.b, 0.0f, 1.0f) * 255);
			px[i + 3] = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255);
		}
		// Painter 결과를 찍는다: 바깥 외곽선(outline px), 종이 결
		void PutPainter(int x, int y, const Painter& pt, float outline = 3.2f, float grain = 0.06f) {
			if (!pt.on) return;
			Rgb c = pt.c;
			if (outline > 0.0f && pt.sil <= 0.0f && pt.sil > -outline) c = kInk;
			float g = (1.0f - grain) + grain * Grain(pt.p.x + x * 0.37f, pt.p.y + y * 0.21f);
			Put(x, y, { c.r * g, c.g * g, c.b * g });
		}
	};
}
