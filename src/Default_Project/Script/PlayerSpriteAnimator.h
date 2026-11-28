#pragma once
#include "Component.h"
#include <string>
using namespace std;

class Texture;
class SpriteRenderer;
class PlayerController;
class PlayerHealth;
class BoxCollider;
class ViewTransition;

enum class PlayerSpriteState {
	Idle,		// 가만히
	Walk,		// 걷기 (walk_01→02→03→04 반복)
	JumpRise,	// 공중, 올라가는 중
	JumpFall,	// 공중, 내려가는 중
	Hurt,		// 피격: 책벌레에 맞아 넉백 중(조작 불가 0.25초)
	Clear		// 스테이지 클리어 (게임 상태 없음, 그림만 불러 둠)
};

// 플레이어 상태에 맞춰 PaperChild 그림을 고르는 컴포넌트
//  - 가만히: idle / 걷기: walk 4장을 8fps로 반복 / 공중: 올라가면 jump_rise, 내려가면 jump_fall
//  - 왼쪽으로 움직이면 좌우 반전(SpriteRenderer::flipX), 멈추면 마지막 방향을 유지한다
//  - 책벌레에 맞아 넉백 중이면 hurt, 적·가시에 맞은 뒤 무적 1초 동안은 반투명으로 깜빡인다(기획서 4.4)
//  - manifest.json의 공통 pivot을 발 기준점으로 쓰고, 그림 크기는 충돌 상자 높이에 맞춘다
//  - 이동·충돌은 건드리지 않고 PlayerController 값(속도, 땅 위인지)을 읽기만 한다
//  - 같은 오브젝트에 SpriteRenderer, PlayerController가 있어야 하고,
//    JSON에서 PlayerController보다 뒤에 붙여야 같은 프레임의 속도를 읽는다
class PlayerSpriteAnimator : public Component {
public:
	string folder = "Assets/Characters/PaperChild/";	// 그림과 manifest.json이 있는 폴더 (실행 폴더 기준)
	float walkFps = 8.0f;				// 걷기 그림 전환 속도(장/초)
	float walkSpeedThreshold = 0.1f;	// 수평 속도가 이보다 작으면 가만히 서 있는 것으로 본다
	float airborneDelay = 0.06f;		// 발판에서 떨어진 뒤 이만큼 지나야 낙하 그림으로 바꾼다 (땅 판정이 한 프레임 끊겨도 깜빡이지 않게)
	float blinkInterval = 0.1f;			// 무적 중 깜빡임 간격(초): 0.1초 반투명, 0.1초 보통을 되풀이
	float blinkAlpha = 0.3f;			// 깜빡일 때 그림의 불투명도 (SpriteRenderer::tint의 알파)

	void Start() override;
	void Update(float deltaTime) override;

	// [연결 지점] 클리어처럼 이동과 상관없이 그림을 정해야 할 때 쓴다.
	//  스테이지 클리어 처리가 생기면 SetOverrideState(PlayerSpriteState::Clear)를 부르고,
	//  상태가 끝나면 ClearOverrideState()를 부른다. 지금은 부르는 곳이 없다.
	//  피격(hurt)은 PlayerController::IsControlLocked()를 읽어서 정하므로 이 함수를 쓰지 않는다.
	void SetOverrideState(PlayerSpriteState s);
	void ClearOverrideState();

	PlayerSpriteState GetState() const { return state; }
	bool IsFacingLeft() const { return facingLeft; }
	string GetFrameName() const { return frameName; }	// 지금 그리는 그림 이름 (디버그 출력용)

	static Component* CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects);

private:
	SpriteRenderer* sprite = nullptr;
	PlayerController* controller = nullptr;
	PlayerHealth* health = nullptr;		// 무적 중인지 읽는다 (없으면 깜빡이지 않는다)
	BoxCollider* box = nullptr;
	ViewTransition* view = nullptr;

	Texture* idleTex = nullptr;
	Texture* walkTex[4] = { nullptr, nullptr, nullptr, nullptr };
	Texture* jumpRiseTex = nullptr;
	Texture* jumpFallTex = nullptr;
	Texture* hurtTex = nullptr;		// 책벌레에 맞아 넉백 중
	Texture* clearTex = nullptr;	// 아직 쓰는 곳 없음 (SetOverrideState로 연결)

	PlayerSpriteState state = PlayerSpriteState::Idle;
	bool hasOverride = false;
	PlayerSpriteState overrideState = PlayerSpriteState::Idle;

	float walkTimer = 0.0f;
	int walkFrame = 0;
	float airTimer = 0.0f;
	float blinkTimer = 0.0f;		// 무적이 시작된 뒤 지난 시간 (깜빡임 박자)
	bool facingLeft = false;
	string frameName = "idle";

	Texture* LoadFrame(const string& name);
	void FitToManifest();
	void ApplyFrame();
};
