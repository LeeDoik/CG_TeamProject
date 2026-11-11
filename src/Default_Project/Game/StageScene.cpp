#include "StageScene.h"
#include "GameManager.h"

extern map<string, Mesh*> meshChache;
extern map<string, Material*> materialChache;
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

	void AddMaterial(const string& name, vec4 color) {
		if (materialChache.find(name) != materialChache.end()) return;
		Material* m = new Material(shaders["Standard"]);
		m->SetVec4("tColor", color);
		materialChache[name] = m;
	}
}

StageScene::StageScene(ScreenSize screenSize) {
	name = "StageScene";
	this->screenSize = screenSize;
}

void StageScene::LoadScene(AppContext& ctx) {
	if (meshChache.find("FlatCube") == meshChache.end()) meshChache["FlatCube"] = MakeFlatCube();

	// 레벨 파일에서 이름으로 고르는 재질 (프로토타입 색. 이후 텍스처로 교체)
	AddMaterial("Paper", vec4(0.93f, 0.89f, 0.80f, 1.0f));      // 종이 바닥
	AddMaterial("Card", vec4(0.72f, 0.56f, 0.38f, 1.0f));       // 골판지
	AddMaterial("CardDark", vec4(0.55f, 0.42f, 0.29f, 1.0f));   // 짙은 골판지 (깊이가 다른 발판 구분용)
	AddMaterial("Wall", vec4(0.80f, 0.66f, 0.48f, 1.0f));       // 벽·상자
	AddMaterial("Mover", vec4(0.47f, 0.63f, 0.80f, 1.0f));      // 움직이는 발판

	// 게임 코드가 쓰는 재질
	AddMaterial("Spike", vec4(0.58f, 0.58f, 0.62f, 1.0f));
	AddMaterial("SpikeBase", vec4(0.36f, 0.36f, 0.40f, 1.0f));
	AddMaterial("Worm", vec4(0.50f, 0.72f, 0.40f, 1.0f));
	AddMaterial("WormHead", vec4(0.38f, 0.58f, 0.30f, 1.0f));
	AddMaterial("Ink", vec4(0.08f, 0.08f, 0.08f, 1.0f));
	AddMaterial("PlayerPaper", vec4(1.0f, 1.0f, 1.0f, 1.0f));
	AddMaterial("Scarf", vec4(0.85f, 0.20f, 0.20f, 1.0f));
	AddMaterial("Shadow", vec4(0.14f, 0.12f, 0.10f, 1.0f));
	AddMaterial("Pole", vec4(0.36f, 0.26f, 0.18f, 1.0f));
	AddMaterial("CheckOff", vec4(0.62f, 0.62f, 0.62f, 1.0f));
	AddMaterial("CheckOn", vec4(0.96f, 0.76f, 0.16f, 1.0f));
	AddMaterial("Fragment", vec4(1.0f, 0.98f, 0.92f, 1.0f));
	AddMaterial("Lamp", vec4(1.0f, 0.86f, 0.45f, 1.0f));

	ctx.bgColor = vec4(0.96f, 0.92f, 0.84f, 1.0f);

	// 메인 루프 시작 전이므로 Hierarchy에 바로 넣어도 된다.
	Object* manager = Instantiate(ctx, "GameManager");
	manager->AddComponent<GameManager>();
}
