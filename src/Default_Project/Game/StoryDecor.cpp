#include "StoryDecor.h"
#include "StoryArt.h"
#include "FadeRenderer.h"
#include "Object.h"
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include "MeshRenderer3D.h"
#include <map>
#include <cmath>
#include <cstdint>
#include <algorithm>
using namespace std;
using namespace glm;

extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;

namespace {
	// 같은 레벨이면 늘 같은 배치가 나오는 의사 난수 (0~1)
	float H(int i, int seed) {
		uint32_t h = static_cast<uint32_t>(i) * 2654435761u ^ static_cast<uint32_t>(seed) * 40503u;
		h = (h ^ (h >> 15)) * 2246822519u;
		h ^= h >> 13;
		return (h & 0xFFFF) / 65535.0f;
	}

	float Len3(vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

	float EaseOutBack(float t) {
		const float c1 = 1.70158f, c3 = c1 + 1.0f;
		float u = t - 1.0f;
		return 1.0f + c3 * u * u * u + c1 * u * u;
	}

	// 사각형 메시 (아래쪽 가운데가 원점이면 회전이 바닥을 축으로 된다). 앞뒤 양면.
	Mesh* QuadMesh(const string& key, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
		auto it = meshChache.find(key);
		if (it != meshChache.end()) return it->second;
		vector<Vertex> v = {
			{ { x0, y0, 0 }, { u0, v0, 0 }, { 0, 0, 1 } }, { { x1, y0, 0 }, { u1, v0, 0 }, { 0, 0, 1 } },
			{ { x1, y1, 0 }, { u1, v1, 0 }, { 0, 0, 1 } }, { { x0, y1, 0 }, { u0, v1, 0 }, { 0, 0, 1 } },
			{ { x0, y0, 0 }, { u0, v0, 0 }, { 0, 0, -1 } }, { { x1, y0, 0 }, { u1, v0, 0 }, { 0, 0, -1 } },
			{ { x1, y1, 0 }, { u1, v1, 0 }, { 0, 0, -1 } }, { { x0, y1, 0 }, { u0, v1, 0 }, { 0, 0, -1 } } };
		vector<GLuint> idx = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6 };
		Mesh* m = new Mesh(v, idx);
		meshChache[key] = m;
		return m;
	}

	// 소품 아틀라스의 한 칸 (정사각 소품은 칸 아래쪽 절반만)
	Mesh* PropMesh(int prop) {
		int col = prop % StoryArt::kPropCols, row = prop / StoryArt::kPropCols;
		float u0 = col * float(StoryArt::kPropCellW) / StoryArt::kAtlasW, u1 = u0 + float(StoryArt::kPropCellW) / StoryArt::kAtlasW;
		float vTop = 1.0f - row * float(StoryArt::kPropCellH) / StoryArt::kAtlasH;
		float vBottom = vTop - float(StoryArt::kPropCellH) / StoryArt::kAtlasH;
		if (StoryArt::IsSquareProp(prop)) vTop = vBottom + float(StoryArt::kPropCellW) / StoryArt::kAtlasH;
		return QuadMesh("Prop_" + to_string(prop), -0.5f, 0.0f, 0.5f, 1.0f, u0, vBottom, u1, vTop);
	}

	// 하늘 소품은 가운데가 원점 (회전용)
	Mesh* SkyMesh(int prop) {
		int col = prop % StoryArt::kPropCols, row = prop / StoryArt::kPropCols;
		float u0 = col * float(StoryArt::kPropCellW) / StoryArt::kAtlasW, u1 = u0 + float(StoryArt::kPropCellW) / StoryArt::kAtlasW;
		float vBottom = 1.0f - (row + 1) * float(StoryArt::kPropCellH) / StoryArt::kAtlasH;
		float vTop = vBottom + float(StoryArt::kPropCellW) / StoryArt::kAtlasH;
		return QuadMesh("SkyProp_" + to_string(prop), -0.5f, -0.5f, 0.5f, 0.5f, u0, vBottom, u1, vTop);
	}

	Mesh* BackdropMesh(int index) {
		int col = index % StoryArt::kBackCols, row = index / StoryArt::kBackCols;
		float u0 = (col * StoryArt::kBackCellW + 0.5f) / StoryArt::kAtlasW, u1 = ((col + 1) * StoryArt::kBackCellW - 0.5f) / StoryArt::kAtlasW;
		float vTop = 1.0f - (row * StoryArt::kBackCellH + 0.5f) / StoryArt::kAtlasH;
		float vBottom = 1.0f - ((row + 1) * StoryArt::kBackCellH - 0.5f) / StoryArt::kAtlasH;
		if (index == StoryArt::Town) return QuadMesh("Backdrop_" + to_string(index), -0.5f, 0.0f, 0.5f, 1.0f, u0, vBottom, u1, vTop);
		const string key = "BackdropSkirt_" + to_string(index);
		auto found = meshChache.find(key);
		if (found != meshChache.end()) return found->second;
		vector<Vertex> vertices;
		vector<GLuint> indices;
		// The vertex shader aligns this bottom row with the forest foot in screen space.
		for (int side = 0; side < 2; ++side) {
			vec3 n(0, 0, side == 0 ? 1.0f : -1.0f);
			for (int rowIndex = 0; rowIndex < 3; ++rowIndex) {
				float y = rowIndex == 0 ? -1.0f : (rowIndex == 1 ? 0.0f : 1.0f);
				float v = rowIndex == 2 ? vTop : vBottom;
				vertices.push_back({ { -0.5f, y, 0 }, { u0, v, rowIndex == 0 ? 1.0f : 0.0f }, n });
				vertices.push_back({ { 0.5f, y, 0 }, { u1, v, rowIndex == 0 ? 1.0f : 0.0f }, n });
			}
			for (GLuint rowIndex = 0; rowIndex < 2; ++rowIndex) {
				GLuint a = side * 6 + rowIndex * 2;
				if (side == 0) indices.insert(indices.end(), { a, a + 1, a + 3, a, a + 3, a + 2 });
				else indices.insert(indices.end(), { a, a + 3, a + 1, a, a + 2, a + 3 });
			}
		}
		Mesh* mesh = new Mesh(vertices, indices);
		meshChache[key] = mesh;
		return mesh;
	}

	// 발판 앞면 위쪽에 늘어뜨리는 물결 잔디 (1칸에 무늬 한 번 반복)
	Mesh* FringeMesh(float width) {
		float w = std::round(width * 4.0f) / 4.0f;
		return QuadMesh("Fringe_" + to_string(static_cast<int>(w * 100)), -w * 0.5f, -0.3f, w * 0.5f, 0.0f, 0.0f, 0.0f, w, 1.0f);
	}

	StoryTheme MakeTheme(const string& name) {
		StoryTheme t;
		t.name = name;
		if (name == "afternoon") {
			t.skyTop = { 0.52f, 0.74f, 0.93f }; t.skyBottom = { 1.0f, 0.90f, 0.74f };
			t.sunColor = { 1.0f, 0.93f, 0.80f }; t.spriteTint = { 1.0f, 0.97f, 0.92f };
		}
		else if (name == "sunset") {
			t.skyTop = { 0.44f, 0.40f, 0.68f }; t.skyBottom = { 1.0f, 0.64f, 0.46f };
			t.sunColor = { 1.0f, 0.76f, 0.56f }; t.sunIntensity = 0.78f; t.fillIntensity = 0.32f;
			t.spriteTint = { 1.0f, 0.86f, 0.80f }; t.lowSun = true;
		}
		else if (name == "night") {
			t.skyTop = { 0.06f, 0.09f, 0.24f }; t.skyBottom = { 0.24f, 0.24f, 0.46f };
			t.sunColor = { 0.62f, 0.70f, 1.0f }; t.sunIntensity = 0.48f; t.fillIntensity = 0.20f;
			t.spriteTint = { 0.55f, 0.58f, 0.82f }; t.night = true;
		}
		else {
			t.name = "day";
		}
		return t;
	}
}

Object* StoryDecor::SpawnQuad(const string& name, Mesh* mesh, const string& material, vec3 pos, vec3 scale, bool isLevelObject, bool fade) {
	Object* o = spawn(name, isLevelObject);
	Transform* t = o->GetComponent<Transform>();
	t->SetLocalScale(scale.x, scale.y, scale.z);
	t->SetLocalPosition(pos);
	Material* m = materialChache[material];
	if (fade) o->AddComponent<FadeRenderer>(mesh, m);
	else o->AddComponent<MeshRenderer3D>(mesh, m);
	return o;
}

void StoryDecor::Init(SpawnFn spawnFn) {
	spawn = spawnFn;
	sky = SpawnQuad("Sky", meshChache["FlatCube"], "Sky", vec3(0, 0, -80), vec3(900, 400, 0.1f), false, false);
	skyMaterial = materialChache["Sky"];

	sun.obj = SpawnQuad("Sky_Sun", SkyMesh(StoryArt::Sun), "SkySprite", vec3(0, 0, -40), vec3(3.4f, 3.4f, 1), false, false);
	sun.size = 3.4f;
	moon.obj = SpawnQuad("Sky_Moon", SkyMesh(StoryArt::Moon), "SkySprite", vec3(0, 0, -44), vec3(3.0f, 3.0f, 1), false, false);
	moon.size = 3.0f;
	for (int i = 0; i < 26; ++i) {
		SkyThing s;
		s.size = 0.32f + 0.28f * H(i, 3);
		s.offset = vec3(-11.0f + 22.0f * H(i, 1), 0.8f + 4.6f * H(i, 2), -46.0f);
		s.obj = SpawnQuad("Sky_Star", SkyMesh(StoryArt::Star), "SkySprite", vec3(0, 0, -46), vec3(s.size, s.size, 1), false, false);
		stars.push_back(s);
	}
	for (int i = 0; i < 6; ++i) {
		SkyThing c;
		c.size = 2.6f + 1.2f * H(i, 7);
		c.offset = vec3(-15.0f + 5.0f * i + 2.0f * H(i, 8), 2.4f + 2.2f * H(i, 9), -42.0f);
		c.obj = SpawnQuad("Sky_Cloud", SkyMesh(StoryArt::Cloud), "SkySprite", vec3(0, 0, -42), vec3(c.size, c.size, 1), false, false);
		clouds.push_back(c);
	}
	for (int i = 0; i < 30; ++i) {
		Particle p;
		p.rel = vec3(-12.0f + 24.0f * H(i, 11), -6.0f + 13.0f * H(i, 12), -5.0f + 6.5f * H(i, 13));
		p.speed = 0.35f + 0.5f * H(i, 14);
		p.phase = 6.28f * H(i, 15);
		p.spin = (H(i, 16) - 0.5f) * 4.0f;
		p.obj = SpawnQuad("Particle", SkyMesh(StoryArt::Petal), "Petal", vec3(0), vec3(0.3f, 0.3f, 1), false, false);
		p.renderer = p.obj->GetComponent<MeshRenderer3D>();
		particles.push_back(p);
	}
}

const StoryTheme& StoryDecor::ApplyTheme(const string& name) {
	theme = MakeTheme(name);
	if (skyMaterial) {
		skyMaterial->SetVec3("skyTop", theme.skyTop);
		skyMaterial->SetVec3("skyBottom", theme.skyBottom);
	}
	vec4 tint(theme.spriteTint, 1.0f);
	for (const char* m : { "PropAtlas", "BackAtlas", "Fringe" }) materialChache[m]->SetVec4("tColor", tint);
	materialChache["DollSprite"]->SetVec4("tColor", vec4(mix(vec3(1.0f), theme.spriteTint, 0.5f), 1.0f));
	materialChache["Petal"]->SetVec4("tColor", theme.name == "sunset" ? vec4(1.0f, 0.78f, 0.56f, 1.0f) : vec4(1.0f));

	// 밤에는 꽃잎 대신 반딧불
	Mesh* particleMesh = SkyMesh(theme.night ? StoryArt::Firefly : StoryArt::Petal);
	for (auto& p : particles) p.renderer->mesh = particleMesh;
	return theme;
}

FadeRenderer* StoryDecor::PlaceProp(int kind, float height, vec3 base, bool fade) {
	vec3 scale = StoryArt::IsSquareProp(kind) ? vec3(height, height, 1) : vec3(height * 0.5f, height, 1);
	Object* o = SpawnQuad("Prop", PropMesh(kind), "PropAtlas", base, scale, true, fade);
	o->GetComponent<Transform>()->SetLocalRotation(-88.0f, 0.0f, 0.0f);   // 처음에는 접혀 누워 있다
	props.push_back({ o, base, 0.0f, 0.0f });
	return o->GetComponent<FadeRenderer>();
}

void StoryDecor::BuildLevel(const LevelData& level, vector<vector<FadeRenderer*>>& blockCaps) {
	blockCaps.assign(level.blocks.size(), {});
	if (level.blocks.empty()) return;

	float minX = 1e9f, maxX = -1e9f;
	for (const auto& b : level.blocks) {
		minX = std::min(minX, b.center.x - b.size.x * 0.5f);
		maxX = std::max(maxX, b.center.x + b.size.x * 0.5f);
	}
	float len = maxX - minX, mid = (minX + maxX) * 0.5f;

	// 아래에 펼쳐진 책장과 겹친 종이 단면 (3D에서 구덩이 아래로 보인다)
	SpawnQuad("Book_Page", meshChache["FlatCube"], "BookPage", vec3(mid, -18.5f, -18.0f), vec3(len + 90.0f, 1.0f, 64.0f), true, false);
	for (int k = 0; k < 3; ++k)
		SpawnQuad("Book_Edge", meshChache["FlatCube"], (k % 2) ? "BookEdge" : "BookEdge2",
			vec3(mid, -19.15f - k * 0.3f, -18.0f + 0.08f * (k + 1)), vec3(len + 90.0f, 0.28f, 64.0f), true, false);

	// 오려 낸 배경 판: 2D에서는 거의 붙어 있다가 3D에서 깊이 방향으로 벌어진다
	auto layer = [&](int index, float x, float y, float w, float h, float z2D, float z3D) {
		Object* o = SpawnQuad("Backdrop", BackdropMesh(index), "BackAtlas", vec3(x, y, z2D), vec3(w, h, 1), true, false);
		layers.push_back({ o, vec3(x, y, z2D), z2D, z3D });
	};
	// Matching periodic silhouettes meet exactly: no random gaps or coplanar overlaps.
	for (float x = minX - 60; x < maxX + 60; x += 18.0f) layer(StoryArt::Mountains, x, -5.5f, 18.0f, 9.0f, -11.8f, -36.0f);
	layer(StoryArt::Town, minX + len * 0.38f, -5.2f, 15.0f, 7.5f, -11.5f, -27.0f);
	layer(StoryArt::Town, minX + len * 0.86f, -5.2f, 15.0f, 7.5f, -11.5f, -27.0f);
	for (float x = minX - 60; x < maxX + 60; x += 13.0f) layer(StoryArt::Hills, x, -5.2f, 13.0f, 6.5f, -11.1f, -20.0f);
	for (float x = minX - 60; x < maxX + 60; x += 12.0f) layer(StoryArt::Forest, x, -6.0f, 12.0f, 5.5f, -10.7f, -14.0f);

	// 페이지 조각 주위를 도는 반짝이 별 3개
	for (const vec3& f : level.fragments) {
		for (int k = 0; k < 3; ++k) {
			Object* o = SpawnQuad("Sparkle", SkyMesh(StoryArt::Star), "SkySprite", f, vec3(0.32f, 0.32f, 1), true, false);
			sparkles.push_back({ o, f, k * 2.094f });
		}
	}
	// 등불 둘레 빛무리
	for (const auto& l : level.lamps) {
		Object* o = SpawnQuad("Lamp_Glow", SkyMesh(StoryArt::Firefly), "SkySprite", l.position + vec3(0, 0, -0.05f), vec3(1.6f, 1.6f, 1), true, false);
		glows.push_back({ o, l.position + vec3(0, 0, -0.05f), 0.0f });
	}

	auto onSpike = [&](float x, float z, float halfW) {
		for (const auto& s : level.spikes) {
			if (std::abs(x - s.center.x) < s.size.x * 0.5f + halfW && z > s.center.z - s.size.z * 0.5f - 0.3f && z < s.center.z + s.size.z * 0.5f + 0.3f) return true;
		}
		return false;
	};

	int seed = 0;
	for (size_t bi = 0; bi < level.blocks.size(); ++bi) {
		const LevelBlock& b = level.blocks[bi];
		if (!(b.material.empty() || b.material == "Paper")) continue;
		float x0 = b.center.x - b.size.x * 0.5f, x1 = b.center.x + b.size.x * 0.5f;
		float top = b.center.y + b.size.y * 0.5f;
		float z0 = b.center.z - b.size.z * 0.5f, z1 = b.center.z + b.size.z * 0.5f;

		// 잔디 종이 + 앞면 물결 끝 (벽처럼 반투명해질 수 있게 FadeRenderer)
		Object* cap = SpawnQuad("Grass_Cap", meshChache["FlatCube"], "GrassPaper", vec3(b.center.x, top + 0.04f, b.center.z), vec3(b.size.x + 0.04f, 0.08f, b.size.z + 0.04f), true, true);
		Object* fringe = SpawnQuad("Grass_Fringe", FringeMesh(b.size.x + 0.04f), "Fringe", vec3(b.center.x, top + 0.06f, z1 + 0.03f), vec3(1.0f), true, true);
		blockCaps[bi] = { cap->GetComponent<FadeRenderer>(), fringe->GetComponent<FadeRenderer>() };

		// Low interior accents reveal depth without concealing the walking lane or hazards.
		for (float z = z0 + 0.8f; z < z1 - 0.6f; z += 1.5f) {
			for (float x = x0 + 0.8f; x < x1 - 0.6f; x += 2.1f) {
				int id = static_cast<int>(bi) * 10007 + static_cast<int>((x - x0) * 13 + (z - z0) * 197);
				vec3 at(x + (H(id, 60) - 0.5f) * 0.4f, top + 0.085f, z);
				if (std::abs(z) < 0.65f || onSpike(at.x, at.z, 0.45f) || H(id, 61) > 0.6f) continue;
				bool blocked = false;
				for (size_t other = 0; other < level.blocks.size(); ++other) {
					if (other == bi) continue;
					const auto& box = level.blocks[other];
					if (box.center.y + box.size.y * 0.5f > top + 0.1f &&
						box.center.y - box.size.y * 0.5f < top + 0.6f &&
						std::abs(at.x - box.center.x) < box.size.x * 0.5f + 0.45f &&
						std::abs(at.z - box.center.z) < box.size.z * 0.5f + 0.45f) blocked = true;
				}
				for (const auto& point : level.checkpoints) if (Len3(at - point) < 1.1f) blocked = true;
				for (const auto& point : level.fragments) if (Len3(at - point) < 1.3f) blocked = true;
				if (Len3(at - level.start) < 1.1f || blocked) continue;
				int kind = H(id, 62) < 0.75f ? StoryArt::GrassTuft : StoryArt::Daisy;
				blockCaps[bi].push_back(PlaceProp(kind, 0.3f + 0.13f * H(id, 63), at, true));
			}
		}

		// 앞줄: 작은 풀·꽃 (발판 앞 가장자리)
		for (float x = x0 + 0.45f; x < x1 - 0.35f; x += 0.8f + 1.5f * H(seed, 31), ++seed) {
			if (onSpike(x, z1 - 0.12f, 0.3f)) continue;
			float r = H(seed, 32);
			if (r < 0.5f) PlaceProp(StoryArt::GrassTuft, 0.42f + 0.12f * H(seed, 33), vec3(x, top + 0.08f, z1 - 0.12f));
			else if (r < 0.78f) PlaceProp(StoryArt::Daisy, 0.7f + 0.2f * H(seed, 34), vec3(x, top + 0.08f, z1 - 0.12f));
			else PlaceProp(StoryArt::Tulips, 0.68f + 0.15f * H(seed, 35), vec3(x, top + 0.08f, z1 - 0.12f));
		}
		// 뒷줄: 덤불·버섯·울타리, 넓은 바닥에는 나무·오두막, 밤에는 가로등도
		if (b.size.z < 3.0f) continue;
		float zb = z0 + 0.35f;
		for (float x = x0 + 0.9f; x < x1 - 0.7f; x += 1.7f + 2.4f * H(seed, 41), ++seed) {
			if (onSpike(x, zb, 0.5f)) continue;
			float r = H(seed, 42), big = H(seed, 43);
			if (b.size.z >= 5.5f && big < 0.26f) {
				float k = H(seed, 44);
				if (theme.night && k < 0.35f) PlaceProp(StoryArt::LampPost, 2.3f, vec3(x, top + 0.08f, zb));
				else if (k < 0.55f) PlaceProp(StoryArt::Pine, 3.0f + 0.6f * H(seed, 45), vec3(x, top + 0.08f, zb));
				else if (k < 0.82f) PlaceProp(StoryArt::RoundTree, 2.8f + 0.5f * H(seed, 46), vec3(x, top + 0.08f, zb));
				else PlaceProp(StoryArt::Cottage, 2.6f, vec3(x, top + 0.08f, zb));
			}
			else if (r < 0.32f) PlaceProp(StoryArt::Bush, 0.95f + 0.25f * H(seed, 47), vec3(x, top + 0.08f, zb));
			else if (r < 0.52f) PlaceProp(StoryArt::Mushroom, 0.7f + 0.2f * H(seed, 48), vec3(x, top + 0.08f, zb));
			else if (r < 0.70f) PlaceProp(StoryArt::Fence, 0.95f, vec3(x, top + 0.08f, zb));
			else if (r < 0.86f) PlaceProp(StoryArt::Tulips, 0.8f, vec3(x, top + 0.08f, zb));
			else PlaceProp(StoryArt::Daisy, 0.9f, vec3(x, top + 0.08f, zb));
		}
	}
}

void StoryDecor::ClearLevel() {
	props.clear();
	layers.clear();
	sparkles.clear();
	glows.clear();
}

void StoryDecor::Update(float dt, vec3 player, vec3 camFocus, float viewT, float time, bool showWorld) {
	vec3 look = camFocus + vec3(0.0f, 1.0f, 0.0f);

	// 하늘은 카메라를 따라다닌다
	if (sky) {
		sky->GetComponent<Transform>()->SetLocalPosition(look.x, look.y, -80.0f);
		skyMaterial->SetFloat("skyY0", look.y - 6.5f);
		skyMaterial->SetFloat("skyY1", look.y + 6.0f);
	}
	sun.obj->isValid = !theme.night;
	if (sun.obj->isValid) {
		Transform* t = sun.obj->GetComponent<Transform>();
		t->SetLocalRotation(0.0f, 0.0f, time * 8.0f);
		t->SetLocalPosition(look.x + 7.2f, look.y + (theme.lowSun ? 0.4f : 3.3f), -40.0f);
	}
	moon.obj->isValid = theme.night;
	if (moon.obj->isValid) moon.obj->GetComponent<Transform>()->SetLocalPosition(look.x + 7.0f, look.y + 3.4f, -44.0f);
	for (size_t i = 0; i < stars.size(); ++i) {
		SkyThing& s = stars[i];
		s.obj->isValid = theme.night;
		if (!theme.night) continue;
		float tw = s.size * (0.8f + 0.25f * std::sin(time * 2.2f + i * 1.7f));
		Transform* t = s.obj->GetComponent<Transform>();
		t->SetLocalScale(tw, tw, 1.0f);
		t->SetLocalPosition(look.x + s.offset.x, look.y + s.offset.y, s.offset.z);
	}
	for (size_t i = 0; i < clouds.size(); ++i) {
		SkyThing& c = clouds[i];
		c.obj->isValid = !theme.night;
		if (theme.night) continue;
		float x = std::fmod(c.offset.x + 15.0f + time * 0.25f, 30.0f);
		if (x < 0) x += 30.0f;
		c.obj->GetComponent<Transform>()->SetLocalPosition(look.x + x - 15.0f, look.y + c.offset.y, c.offset.z);
	}

	// 흩날리는 꽃잎 (밤: 떠다니는 반딧불)
	for (size_t i = 0; i < particles.size(); ++i) {
		Particle& p = particles[i];
		p.obj->isValid = showWorld;
		if (!showWorld) continue;
		if (theme.night) {
			p.rel.x += std::sin(time * 0.7f + p.phase) * 0.5f * dt;
			p.rel.y += std::cos(time * 0.9f + p.phase * 1.3f) * 0.4f * dt;
		}
		else {
			p.rel.y -= p.speed * dt;
			p.rel.x += (std::sin(time * 1.3f + p.phase) * 0.6f - 0.3f) * dt;
		}
		if (p.rel.y < -6.5f) { p.rel.y = 7.0f; p.rel.x = -12.0f + 24.0f * H(static_cast<int>(i) + static_cast<int>(time), 17); }
		if (p.rel.y > 7.5f) p.rel.y = -6.0f;
		if (p.rel.x < -12.5f) p.rel.x += 25.0f;
		if (p.rel.x > 12.5f) p.rel.x -= 25.0f;
		Transform* t = p.obj->GetComponent<Transform>();
		if (theme.night) {
			float glow = 0.22f + 0.08f * std::sin(time * 3.0f + p.phase);
			t->SetLocalScale(glow, glow, 1.0f);
			t->SetLocalRotation(0.0f, 0.0f, 0.0f);
		}
		else {
			t->SetLocalScale(0.3f, 0.3f, 1.0f);
			t->SetLocalRotation(std::sin(time + p.phase) * 50.0f, time * p.spin * 60.0f, time * p.spin * 40.0f);
		}
		t->SetLocalPosition(look.x + p.rel.x, look.y + p.rel.y, p.rel.z);
	}

	// 팝업 소품: 플레이어가 다가가면 접혀 있다가 "툭" 일어선다
	for (auto& pr : props) {
		bool near = showWorld && std::abs(pr.base.x - player.x) < 10.5f;
		pr.pop = std::clamp(pr.pop + (near ? dt * 2.2f : -dt * 1.6f), 0.0f, 1.0f);
		float angle = -88.0f * (1.0f - EaseOutBack(pr.pop));
		pr.obj->GetComponent<Transform>()->SetLocalRotation(angle, 0.0f, 0.0f);
	}

	// 페이지 조각 반짝이: 빙글빙글 돌며 반짝, 플레이어가 집으면 사라짐
	for (auto& sp : sparkles) {
		bool taken = Len3(player - sp.center) < 1.0f;
		if (taken) sp.obj->isValid = false;
		if (!sp.obj->isValid) continue;
		float a = time * 1.8f + sp.phase;
		float sz = 0.26f + 0.08f * std::sin(time * 4.0f + sp.phase * 3.0f);
		Transform* t = sp.obj->GetComponent<Transform>();
		t->SetLocalScale(sz, sz, 1.0f);
		t->SetLocalPosition(sp.center + vec3(std::cos(a) * 0.75f, 0.15f + std::sin(a * 1.3f) * 0.35f, std::sin(a) * 0.3f + 0.1f));
	}
	for (auto& g : glows) {
		float sz = 1.5f + 0.15f * std::sin(time * 2.0f);
		g.obj->GetComponent<Transform>()->SetLocalScale(sz, sz, 1.0f);
	}

	// 배경 판 벌어짐
	float s = smoothstep(0.0f, 1.0f, viewT);
	materialChache["BackAtlas"]->SetVec3("backdropFoot", vec3(0.0f, -6.0f, mix(-10.7f, -14.0f, s)));
	for (auto& l : layers) l.obj->GetComponent<Transform>()->SetLocalPosition(l.pos.x, l.pos.y, mix(l.z2D, l.z3D, s));
}
