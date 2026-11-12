#include "ProceduralTextures.h"
#include "CharacterArt.h"
#include "StoryArt.h"
#include <gl/glew.h>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
using namespace std;

namespace {
	// ---------------- 노이즈 ----------------
	float Hash(int x, int y, int seed) {
		uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(seed) * 2246822519u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return (h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
	}

	// period마다 반복되는 값 노이즈 → 텍스처를 이어 붙여도 이음매가 안 보인다
	float ValueNoise(float x, float y, int period, int seed) {
		int xi = static_cast<int>(floor(x)), yi = static_cast<int>(floor(y));
		float fx = x - xi, fy = y - yi;
		auto wrap = [period](int v) { return ((v % period) + period) % period; };
		float v00 = Hash(wrap(xi), wrap(yi), seed), v10 = Hash(wrap(xi + 1), wrap(yi), seed);
		float v01 = Hash(wrap(xi), wrap(yi + 1), seed), v11 = Hash(wrap(xi + 1), wrap(yi + 1), seed);
		float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
		return (v00 * (1 - sx) + v10 * sx) * (1 - sy) + (v01 * (1 - sx) + v11 * sx) * sy;
	}

	float Fbm(float x, float y, int period, int seed) {
		float sum = 0.0f, amp = 0.5f;
		for (int o = 0; o < 4; ++o) {
			sum += amp * ValueNoise(x, y, period, seed + o * 17);
			x *= 2; y *= 2; period *= 2; amp *= 0.5f;
		}
		return sum;
	}

	struct Image {
		int w, h;
		vector<unsigned char> px;
		Image(int w, int h) : w(w), h(h), px(static_cast<size_t>(w) * h * 4, 0) {}
		// (x, y)는 위에서부터 센 좌표. OpenGL 텍스처는 아래 줄이 먼저라서 뒤집어 저장한다.
		void Set(int x, int y, float r, float g, float b, float a) {
			size_t i = (static_cast<size_t>(h - 1 - y) * w + x) * 4;
			px[i] = static_cast<unsigned char>(std::clamp(r, 0.0f, 1.0f) * 255);
			px[i + 1] = static_cast<unsigned char>(std::clamp(g, 0.0f, 1.0f) * 255);
			px[i + 2] = static_cast<unsigned char>(std::clamp(b, 0.0f, 1.0f) * 255);
			px[i + 3] = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255);
		}
	};

	void Upload(int unit, const Image& img, bool repeat) {
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.px.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glActiveTexture(GL_TEXTURE0);
	}

	// ---------------- 반복 무늬 (흑백 밝기, 재질 색에 곱한다) ----------------
	Image MakePaper() {
		const int N = 128;
		Image img(N, N);
		for (int y = 0; y < N; ++y)
			for (int x = 0; x < N; ++x) {
				float grain = Fbm(x / 16.0f, y / 16.0f, 8, 11);
				float fiber = ValueNoise(x / 2.0f, y / 20.0f, 64, 23);    // 세로로 긴 섬유
				float v = 0.90f + 0.08f * grain - 0.04f * std::max(0.0f, fiber - 0.7f) * 3.0f;
				img.Set(x, y, v, v * 0.99f, v * 0.96f, 1.0f);
			}
		return img;
	}

	Image MakeCard() {
		const int N = 128;
		Image img(N, N);
		for (int y = 0; y < N; ++y)
			for (int x = 0; x < N; ++x) {
				float ridge = 0.5f + 0.5f * std::sin(x * 6.2831853f / 16.0f); // 골판지 골
				float grain = Fbm(x / 16.0f, y / 16.0f, 8, 37);
				float v = 0.84f + 0.10f * ridge + 0.06f * grain;
				img.Set(x, y, v, v * 0.98f, v * 0.94f, 1.0f);
			}
		return img;
	}

	Image MakeWall() {
		const int N = 128;
		Image img(N, N);
		for (int y = 0; y < N; ++y)
			for (int x = 0; x < N; ++x) {
				float a = std::fmod((x + y) / 32.0f, 1.0f), b = std::fmod((x - y + 128) / 32.0f, 1.0f);
				float lattice = std::min(std::abs(a - 0.5f), std::abs(b - 0.5f));    // 마름모 격자
				float line = lattice < 0.03f ? 1.0f : 0.0f;
				float dx = std::fmod(x + 16.0f, 32.0f) - 16.0f, dy = std::fmod(static_cast<float>(y), 32.0f) - 16.0f;
				float dot = (dx * dx + dy * dy) < 9.0f ? 1.0f : 0.0f;               // 격자 안 점
				float grain = Fbm(x / 16.0f, y / 16.0f, 8, 51);
				float v = 0.93f + 0.06f * grain - 0.08f * line - 0.06f * dot;
				img.Set(x, y, v, v * 0.98f, v * 0.95f, 1.0f);
			}
		return img;
	}

	// ---------------- 투명 배경 그림 (거리 함수로 도형을 그린다) ----------------
	float SdCircle(float x, float y, float cx, float cy, float r) { return std::hypot(x - cx, y - cy) - r; }
	float SdRoundBox(float x, float y, float cx, float cy, float hx, float hy, float r) {
		float qx = std::abs(x - cx) - hx + r, qy = std::abs(y - cy) - hy + r;
		float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
		return outside + std::min(std::max(qx, qy), 0.0f) - r;
	}

	// 종이 인형: 흰 종이 몸 + 검은 외곽선 + 빨간 목도리 + 오른쪽을 보는 눈
	Image MakeDoll() {
		const int W = 128, H = 192;
		Image img(W, H);
		for (int py = 0; py < H; ++py)
			for (int px = 0; px < W; ++px) {
				float x = px + 0.5f, y = py + 0.5f;
				float head = SdCircle(x, y, 64, 46, 30);
				float body = SdRoundBox(x, y, 64, 122, 31, 44, 14);
				float armL = SdRoundBox(x, y, 29, 112, 8, 22, 8);
				float armR = SdRoundBox(x, y, 99, 112, 8, 22, 8);
				float footL = SdRoundBox(x, y, 50, 174, 11, 10, 6);
				float footR = SdRoundBox(x, y, 78, 174, 11, 10, 6);
				float shape = std::min({ head, body, armL, armR, footL, footR });
				if (shape > 0.0f) continue; // 투명

				float paper = 0.95f + 0.05f * Fbm(x / 12.0f, y / 12.0f, 16, 71);
				float r = paper, g = paper, b = paper * 0.97f;

				bool scarfBand = (y > 76 && y < 92 && body < 2.0f) || SdRoundBox(x, y, 90, 104, 7, 15, 5) < 0.0f;
				if (scarfBand) { r = 0.86f; g = 0.20f; b = 0.20f; }
				if (SdCircle(x, y, 78, 44, 5.5f) < 0.0f) { r = g = b = 0.08f; }               // 눈
				if (SdCircle(x, y, 84, 58, 5.0f) < 0.0f) { r = 1.0f; g = 0.72f; b = 0.72f; }   // 볼
				if (shape > -4.0f) { r = g = b = 0.12f; }                                      // 외곽선
				img.Set(px, py, r, g, b, 1.0f);
			}
		return img;
	}

	// 찢어진 마지막 장 조각: 오른쪽·위가 찢긴 크림색 종이, 글줄, 금색 테두리
	Image MakePage() {
		const int W = 96, H = 128;
		Image img(W, H);
		for (int py = 0; py < H; ++py)
			for (int px = 0; px < W; ++px) {
				float x = px + 0.5f, y = py + 0.5f;
				float rightEdge = 82.0f + 7.0f * (ValueNoise(y / 6.0f, 0.0f, 64, 5) - 0.5f) * 2.0f;
				float topEdge = 10.0f + 4.0f * (ValueNoise(x / 5.0f, 3.0f, 64, 9) - 0.5f) * 2.0f;
				float d = std::max({ 8.0f - x, x - rightEdge, topEdge - y, y - 120.0f }); // 0 이하가 안쪽
				if (d > 0.0f) continue;

				float paper = 0.93f + 0.07f * Fbm(x / 10.0f, y / 10.0f, 16, 13);
				float r = paper, g = paper * 0.97f, b = paper * 0.86f;
				int row = static_cast<int>(y - 24) % 12;
				float lineEnd = rightEdge - 10.0f - 18.0f * Hash(static_cast<int>(y - 24) / 12, 0, 77);
				if (y > 24 && y < 112 && row < 3 && x > 18 && x < lineEnd) { r *= 0.55f; g *= 0.55f; b *= 0.6f; } // 글줄
				if (d > -4.0f) { r = 0.92f; g = 0.70f; b = 0.18f; }                                                  // 금색 테두리
				img.Set(px, py, r, g, b, 1.0f);
			}
		return img;
	}
}

void ProceduralTextures::CreateAll() {
	Upload(kPaper, MakePaper(), true);
	Upload(kCard, MakeCard(), true);
	Upload(kWall, MakeWall(), true);
	Upload(kDoll, MakeDoll(), false);
	Upload(kPage, MakePage(), false);
	Image props(StoryArt::kAtlasW, StoryArt::kAtlasH);
	props.px = StoryArt::DrawPropAtlas();
	Upload(kPropAtlas, props, false);
	Image back(StoryArt::kAtlasW, StoryArt::kAtlasH);
	back.px = StoryArt::DrawBackdropAtlas();
	Upload(kBackdropAtlas, back, false);
	// Packed cutout cells have no mip gutters: do not sample neighbouring cells.
	glActiveTexture(GL_TEXTURE0 + kBackdropAtlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glActiveTexture(GL_TEXTURE0);
	Image fringe(StoryArt::kFringeW, StoryArt::kFringeH);
	fringe.px = StoryArt::DrawFringe();
	Upload(kFringe, fringe, true);
	Image pageLines(256, 256);
	pageLines.px = StoryArt::DrawPageLines();
	Upload(kPageLines, pageLines, true);
	for (int i = 0; i < CharacterArt::kCount; ++i) {
		Image img(CharacterArt::kWidth, CharacterArt::kHeight);
		img.px = CharacterArt::Draw(i);
		Upload(kCharacterBase + i, img, false);
	}
}
