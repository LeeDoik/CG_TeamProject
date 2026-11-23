#include "Texture.h"
#include <iostream>
#include <vector>
#include <stb/stb_image.h>

namespace {
	// 투명 텍셀(알파 0)의 RGB를 이웃한 불투명 텍셀 색으로 채운다 (alpha bleeding)
	//  PNG의 투명 부분 RGB가 검정이라, GL_LINEAR로 가장자리를 보간하면 검은 테두리가 생긴다.
	//  알파는 그대로 두므로 투명한 곳은 여전히 안 보이고, 가장자리 보간 색만 그림 색에 가까워진다.
	void BleedTransparentColor(unsigned char* rgba, int width, int height, int passes) {
		vector<unsigned char> filled(static_cast<size_t>(width) * height);
		for (int i = 0; i < width * height; ++i) filled[i] = rgba[i * 4 + 3] > 0 ? 1 : 0;

		vector<int> newlyFilled;
		for (int pass = 0; pass < passes; ++pass) {
			newlyFilled.clear();
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					int idx = y * width + x;
					if (filled[idx]) continue;

					int sum[3] = { 0, 0, 0 };
					int count = 0;
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							int nx = x + dx, ny = y + dy;
							if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
							int n = ny * width + nx;
							if (!filled[n]) continue;
							for (int c = 0; c < 3; ++c) sum[c] += rgba[n * 4 + c];
							++count;
						}
					}
					if (count == 0) continue;
					for (int c = 0; c < 3; ++c) rgba[idx * 4 + c] = static_cast<unsigned char>(sum[c] / count);
					newlyFilled.push_back(idx);
				}
			}
			if (newlyFilled.empty()) break;
			for (int idx : newlyFilled) filled[idx] = 1;	// 한 바퀴가 끝난 뒤에 표시해야 한 칸씩 고르게 번진다
		}
	}
}

map<string, Texture*>& Texture::Cache() {
	static map<string, Texture*> cache;
	return cache;
}

Texture* Texture::Load(const string& path) {
	auto& cache = Cache();
	auto it = cache.find(path);
	if (it != cache.end()) return it->second;

	// 이미지 파일은 첫 줄이 맨 위, OpenGL 텍스처는 첫 줄이 v = 0(아래)이다. 여기서 한 번만 뒤집는다.
	stbi_set_flip_vertically_on_load(1);

	int width = 0, height = 0, channels = 0;
	unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);	// 항상 RGBA 4채널로 받는다
	if (!pixels) {
		cout << "[Texture] 이미지를 읽지 못했습니다: " << path << " (" << stbi_failure_reason() << ")" << endl;
		return nullptr;
	}

	// GL_LINEAR 보간은 이웃 1텍셀만 섞으므로 몇 칸만 번지게 하면 충분하다
	BleedTransparentColor(pixels, width, height, 4);

	Texture* texture = new Texture();
	texture->width = width;
	texture->height = height;
	texture->path = path;

	glGenTextures(1, &texture->id);
	glBindTexture(GL_TEXTURE_2D, texture->id);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);	// 한 줄 바이트 수가 4의 배수가 아니어도 안전하게
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(pixels);

	cache[path] = texture;
	return texture;
}

void Texture::Bind(int unit) const {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, id);
}
