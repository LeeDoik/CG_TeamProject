#include "StageScene.h"
#include "GameManager.h"
#include "ProceduralTextures.h"

extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;
extern map<string, GLuint> vertexShaders;
extern map<string, GLuint> fragmentShaders;
extern map<string, GLuint> shaders;

namespace {
	// 면마다 법선이 따로 있는 한 변 1인 육면체 (중심 원점).
	// 실습 26~28 Lighting.h의 MakeLitCube를 엔진 Mesh(위치·색·법선) 형식으로 옮겼다.
	// Assets/Default_Cube.obj는 꼭짓점 법선을 평균 내서 면이 둥글게 보이므로 지형에는 이것을 쓴다.
	Mesh* MakeFlatCube() {
		const vec3 p[8] = {
			{ -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f },
			{ -0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f }, { 0.5f, 0.5f,  0.5f }, { -0.5f, 0.5f,  0.5f } };
		const int idx[36] = { 0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,4,7, 0,7,3, 1,2,6, 1,6,5, 3,7,6, 3,6,2, 0,1,5, 0,5,4 };

		vector<Vertex> vertices;
		vector<GLuint> indices;
		for (int i = 0; i < 36; i += 3) {
			vec3 a = p[idx[i]], b = p[idx[i + 1]], c = p[idx[i + 2]];
			vec3 n = normalize(cross(b - a, c - a)); // CCW 삼각형의 바깥쪽 법선
			for (const vec3& q : { a, b, c }) {
				vertices.push_back(Vertex{ q, vec3(1.0f), n });
				indices.push_back(static_cast<GLuint>(indices.size()));
			}
		}
		return new Mesh(vertices, indices);
	}

	// 앞뒤 양면 사각형 (한 변 1, 중심 원점). 색 속성 xy에 텍스처 좌표를 넣는다 (Sprite 셰이더).
	// 뒷면은 좌우를 뒤집은 좌표라서, y축으로 180도 돌리면 그림이 반대쪽을 보게 된다.
	Mesh* MakeSpriteQuad() {
		vector<Vertex> v = {
			{ { -0.5f, -0.5f, 0.0f }, { 0, 0, 0 }, { 0, 0, 1 } }, { { 0.5f, -0.5f, 0.0f }, { 1, 0, 0 }, { 0, 0, 1 } },
			{ { 0.5f, 0.5f, 0.0f }, { 1, 1, 0 }, { 0, 0, 1 } },   { { -0.5f, 0.5f, 0.0f }, { 0, 1, 0 }, { 0, 0, 1 } },
			{ { -0.5f, -0.5f, 0.0f }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0.5f, -0.5f, 0.0f }, { 0, 0, 0 }, { 0, 0, -1 } },
			{ { 0.5f, 0.5f, 0.0f }, { 0, 1, 0 }, { 0, 0, -1 } },   { { -0.5f, 0.5f, 0.0f }, { 1, 1, 0 }, { 0, 0, -1 } } };
		vector<GLuint> i = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6 };
		return new Mesh(v, i);
	}

	void MakeProgram(const string& name) {
		if (shaders.find(name) != shaders.end()) return;
		make_vertexShaders(vertexShaders, name + "_vertex.glsl");
		make_fragmentShaders(fragmentShaders, name + "_fragment.glsl");
		make_shaderProgram(name, shaders, vertexShaders[name + "_vertex.glsl"], fragmentShaders[name + "_fragment.glsl"]);
	}

	// 종이 질감 재질: 색 × 텍스처(유닛) × 조명. scale은 1칸당 텍스처 반복 수.
	void AddPaper(const string& name, vec4 color, int unit = ProceduralTextures::kPaper, float scale = 0.5f) {
		if (materialChache.find(name) != materialChache.end()) return;
		Material* m = new Material(shaders["Paper"]);
		m->SetVec4("tColor", color);
		m->SetInt("paperTex", unit);
		m->SetFloat("texScale", scale);
		materialChache[name] = m;
	}

	void AddSprite(const string& name, int unit) {
		if (materialChache.find(name) != materialChache.end()) return;
		Material* m = new Material(shaders["Sprite"]);
		m->SetVec4("tColor", vec4(1.0f));
		m->SetInt("spriteTex", unit);
		materialChache[name] = m;
	}
}

StageScene::StageScene(ScreenSize screenSize) {
	name = "StageScene";
	this->screenSize = screenSize;
}

void StageScene::LoadScene(AppContext& ctx) {
	using namespace ProceduralTextures;
	if (meshChache.find("FlatCube") == meshChache.end()) meshChache["FlatCube"] = MakeFlatCube();
	if (meshChache.find("SpriteQuad") == meshChache.end()) meshChache["SpriteQuad"] = MakeSpriteQuad();
	MakeProgram("Paper");   // Standard 조명 + 종이 텍스처
	MakeProgram("Sprite");  // 투명 배경 그림 (알파 테스트)
	ProceduralTextures::CreateAll();

	// 레벨 파일에서 이름으로 고르는 재질 (프로토타입 텍스처. 이후 직접 그린 그림으로 교체)
	AddPaper("Paper", vec4(0.95f, 0.91f, 0.82f, 1.0f));                 // 종이 바닥
	AddPaper("Card", vec4(0.80f, 0.62f, 0.42f, 1.0f), kCard, 0.5f);      // 골판지
	AddPaper("CardDark", vec4(0.62f, 0.47f, 0.32f, 1.0f), kCard, 0.5f);  // 짙은 골판지 (깊이가 다른 발판 구분용)
	AddPaper("Wall", vec4(0.86f, 0.72f, 0.54f, 1.0f), kWall, 0.5f);      // 벽·상자
	AddPaper("Mover", vec4(0.52f, 0.68f, 0.86f, 1.0f), kCard, 0.5f);     // 움직이는 발판

	// 게임 코드가 쓰는 재질
	AddPaper("Spike", vec4(0.60f, 0.60f, 0.64f, 1.0f));
	AddPaper("SpikeBase", vec4(0.38f, 0.38f, 0.42f, 1.0f));
	AddPaper("Worm", vec4(0.52f, 0.75f, 0.42f, 1.0f));
	AddPaper("WormHead", vec4(0.40f, 0.60f, 0.32f, 1.0f));
	AddPaper("Ink", vec4(0.08f, 0.08f, 0.08f, 1.0f));
	AddPaper("Shadow", vec4(0.10f, 0.08f, 0.06f, 1.0f));
	AddPaper("Pole", vec4(0.38f, 0.28f, 0.19f, 1.0f));
	AddPaper("CheckOff", vec4(0.66f, 0.66f, 0.66f, 1.0f));
	AddPaper("CheckOn", vec4(0.98f, 0.78f, 0.18f, 1.0f));
	AddPaper("Lamp", vec4(1.0f, 0.88f, 0.48f, 1.0f));
	AddSprite("DollSprite", kDoll);
	AddSprite("PageSprite", kPage);

	// 팝업북 배경 판
	AddPaper("BgMountain", vec4(0.76f, 0.79f, 0.87f, 1.0f));
	AddPaper("BgCloud", vec4(1.0f, 1.0f, 1.0f, 1.0f));
	AddPaper("BgHill", vec4(0.82f, 0.89f, 0.76f, 1.0f));     // 옅게: 2D에서 구덩이 뒤가 땅처럼 보이지 않도록
	AddPaper("BgTrunk", vec4(0.58f, 0.44f, 0.31f, 1.0f));
	AddPaper("BgLeaf", vec4(0.50f, 0.70f, 0.43f, 1.0f));

	ctx.bgColor = vec4(0.96f, 0.92f, 0.84f, 1.0f);

	// 메인 루프 시작 전이므로 Hierarchy에 바로 넣어도 된다.
	Object* manager = Instantiate(ctx, "GameManager");
	manager->AddComponent<GameManager>();
}
