#pragma once
#include <functional>
#include <string>
#include <vector>
#include "GameTypes.h"
#include "LevelLoader.h"

class Object;
class Material;
class Mesh;
class FadeRenderer;
class MeshRenderer3D;

// 스테이지 분위기 (레벨 파일의 theme 줄: day, afternoon, sunset, night)
struct StoryTheme {
	std::string name = "day";
	vec3 skyTop{ 0.6f, 0.79f, 0.94f };
	vec3 skyBottom{ 0.99f, 0.94f, 0.84f };
	vec3 sunColor{ 1.0f };
	float sunIntensity = 0.8f;
	float fillIntensity = 0.25f;
	vec3 spriteTint{ 1.0f };   // 조명을 받지 않는 그림(소품·배경 판·잔디 끝)에 곱하는 색
	bool night = false;
	bool lowSun = false;       // 노을: 해가 지평선 가까이
};

// 팝업북 꾸미기: 하늘, 해·달·별·구름, 흩날리는 꽃잎(밤에는 반딧불), 아래에 펼쳐진 책장,
// 오려 낸 배경 판(산·성 마을·언덕·숲), 발판 위 잔디 종이, 다가가면 일어서는 팝업 소품.
// 게임 규칙과는 상관없는 장식만 다룬다 (충돌 없음).
class StoryDecor {
public:
	using SpawnFn = std::function<Object* (const std::string&, bool)>;

	// 레벨이 바뀌어도 남는 것 (하늘, 해·달, 별, 구름, 꽃잎)
	void Init(SpawnFn spawnFn);

	// 분위기 적용 (하늘·그림 색). 조명 값은 돌려준 테마로 GameManager가 맞춘다.
	const StoryTheme& ApplyTheme(const std::string& name);

	// 레벨 꾸미기. blockCaps[i]: level.blocks[i] 위에 올린 잔디 렌더러 (벽처럼 반투명해질 때 같이 맞춤)
	void BuildLevel(const LevelData& level, std::vector<std::vector<FadeRenderer*>>& blockCaps);

	// 레벨 오브젝트는 GameManager가 지운다. 여기서는 들고 있던 포인터만 비운다.
	void ClearLevel();

	void Update(float dt, vec3 player, vec3 camFocus, float viewT, float time, bool showWorld);

private:
	struct Prop { Object* obj = nullptr; vec3 base{ 0.0f }; float pop = 0.0f; float delay = 0.0f; };
	struct Layer { Object* obj = nullptr; vec3 pos{ 0.0f }; float z2D = 0.0f, z3D = 0.0f; };
	struct Particle { Object* obj = nullptr; MeshRenderer3D* renderer = nullptr; vec3 rel{ 0.0f }; float speed = 0.5f, phase = 0.0f, spin = 0.0f; };
	struct SkyThing { Object* obj = nullptr; vec3 offset{ 0.0f }; float size = 1.0f; };

	SpawnFn spawn;
	StoryTheme theme;

	Object* sky = nullptr;
	Material* skyMaterial = nullptr;
	SkyThing sun, moon;
	std::vector<SkyThing> stars, clouds;
	std::vector<Particle> particles;
	std::vector<Prop> props;
	std::vector<Layer> layers;
	struct Orbit { Object* obj = nullptr; vec3 center{ 0.0f }; float phase = 0.0f; };
	std::vector<Orbit> sparkles;   // 페이지 조각 반짝이
	std::vector<Orbit> glows;      // 등불 빛무리

	Object* SpawnQuad(const std::string& name, Mesh* mesh, const std::string& material, vec3 pos, vec3 scale, bool isLevelObject, bool fade);
	void PlaceProp(int kind, float height, vec3 base);
};
