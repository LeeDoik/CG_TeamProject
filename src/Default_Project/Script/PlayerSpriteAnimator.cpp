#include "PlayerSpriteAnimator.h"
#include "Object.h"
#include "Texture.h"
#include "SpriteRenderer.h"
#include "PlayerController.h"
#include "PlayerHealth.h"
#include "BoxCollider.h"
#include "ViewTransition.h"
#include <cmath>
#include <algorithm>
#include <fstream>

Texture* PlayerSpriteAnimator::LoadFrame(const string& name) {
	Texture* tex = Texture::Load(folder + "paper_child_" + name + "_right.png");
	if (!tex) cout << "[PlayerSprite] 그림이 없습니다: " << name << endl;
	return tex;
}

void PlayerSpriteAnimator::Start() {
	sprite = gameObject->GetComponent<SpriteRenderer>();
	controller = gameObject->GetComponent<PlayerController>();
	health = gameObject->GetComponent<PlayerHealth>();
	box = gameObject->GetComponent<BoxCollider>();

	if (!sprite) cout << "[PlayerSprite] " << gameObject->name << " 오브젝트에 SpriteRenderer가 없습니다." << endl;
	if (!controller) cout << "[PlayerSprite] " << gameObject->name << " 오브젝트에 PlayerController가 없습니다." << endl;

	// 시점 전환 컴포넌트: 전환 중에는 PlayerController가 멈추므로 그림도 그대로 둔다
	for (auto& obj : gameObject->ctx.Hierarchy) {
		view = obj->GetComponent<ViewTransition>();
		if (view) break;
	}

	// 9장 모두 같은 384×512 캔버스, 같은 발 위치(pivot), 모두 오른쪽을 본다
	idleTex = LoadFrame("idle");
	walkTex[0] = LoadFrame("walk_01");
	walkTex[1] = LoadFrame("walk_02");
	walkTex[2] = LoadFrame("walk_03");
	walkTex[3] = LoadFrame("walk_04");
	jumpRiseTex = LoadFrame("jump_rise");
	jumpFallTex = LoadFrame("jump_fall");
	hurtTex = LoadFrame("hurt");	// 책벌레에 맞아 넉백 중일 때
	clearTex = LoadFrame("clear");	// [연결 지점] 클리어 상태가 생기면 SetOverrideState(Clear)로 보여 준다

	FitToManifest();
	ApplyFrame();
}

// manifest.json의 공통 pivot을 발 기준점으로 쓰고, 그림 크기를 충돌 상자 높이에 맞춘다.
//  - pivot_output_top_left: 384×512 그림 안 발 위치 (왼쪽 위 기준 픽셀)
//  - content_rectangle의 윗변: 원본 그림 캔버스의 맨 위
//  발(pivot)부터 캔버스 맨 위까지가 충돌 상자 높이(1.2칸)가 되게 한다. 머리 꼭대기는 그보다 조금 낮다.
void PlayerSpriteAnimator::FitToManifest() {
	if (!sprite) return;

	float canvasW = idleTex ? static_cast<float>(idleTex->width) : 384.0f;
	float canvasH = idleTex ? static_cast<float>(idleTex->height) : 512.0f;
	float pivotX = canvasW * 0.5f;	// manifest가 없을 때: 아래 가운데
	float pivotY = canvasH;
	float contentTop = 0.0f;

	ifstream in(folder + "manifest.json");
	if (in) {
		try {
			json manifest;
			in >> manifest;
			canvasW = manifest["canvas"][0];
			canvasH = manifest["canvas"][1];
			pivotX = manifest["pivot_output_top_left"][0];
			pivotY = manifest["pivot_output_top_left"][1];
			if (manifest.contains("content_rectangle")) contentTop = manifest["content_rectangle"][1];
		}
		catch (const exception& e) {
			cout << "[PlayerSprite] manifest.json을 읽지 못해 기본 pivot을 씁니다: " << e.what() << endl;
		}
	}
	else cout << "[PlayerSprite] manifest.json이 없어 기본 pivot(아래 가운데)을 씁니다." << endl;

	// 픽셀 좌표(왼쪽 위 원점, 아래로 +y) → UV(왼쪽 아래 원점, 위로 +v). 그림은 뒤집지 않고 좌표만 바꾼다.
	sprite->pivot = vec2(pivotX / canvasW, 1.0f - pivotY / canvasH);

	float colliderHeight = 1.2f;
	if (box) {
		box->RecalculateCollision();
		colliderHeight = box->maxPos.y - box->minPos.y;
	}
	float pixelsToTop = std::max(1.0f, pivotY - contentTop);
	float worldPerPixel = colliderHeight / pixelsToTop;
	sprite->worldSize = vec2(canvasW, canvasH) * worldPerPixel;

	cout << "[PlayerSprite] pivot (" << sprite->pivot.x << ", " << sprite->pivot.y << "), 그림 크기 "
		<< sprite->worldSize.x << " x " << sprite->worldSize.y << "칸 (충돌 상자 높이 " << colliderHeight << "칸)" << endl;
}

void PlayerSpriteAnimator::Update(float deltaTime) {
	if (!sprite) return;

	// 시점 전환 중에는 플레이어가 멈춰 있다. 전환 직전 그림을 그대로 둔다.
	if (view && view->IsTransitioning()) return;

	// 무적 중 깜빡임 (기획서 4.4 "1초 동안 무적"이 눈에 보이게): blinkInterval(0.1초)마다 반투명 ↔ 보통
	//  그림 색에 곱하는 tint의 알파만 바꾼다. 무적이 끝나면 바로 보통(알파 1)으로 돌아온다.
	bool invincible = health && health->IsInvincible();
	blinkTimer = invincible ? blinkTimer + deltaTime : 0.0f;
	bool dim = invincible && static_cast<int>(blinkTimer / blinkInterval) % 2 == 0;
	sprite->tint.a = dim ? blinkAlpha : 1.0f;

	if (hasOverride) {
		state = overrideState;
		ApplyFrame();
		return;
	}

	// PlayerController가 이번 프레임에 움직인 속도 (벽·바닥에 부딪힌 축은 0)
	vec3 v = controller ? controller->GetVelocity() : vec3(0.0f);
	bool grounded = controller ? controller->IsGrounded() : true;
	airTimer = grounded ? 0.0f : airTimer + deltaTime;

	// 넉백 중(조작 불가 0.25초)이면 hurt 그림. 뒤로 밀려나는 것이므로 보던 방향(책벌레 쪽)은 바꾸지 않는다.
	bool knockedBack = controller && controller->IsControlLocked();

	// 방향: 왼쪽(-x)으로 가면 뒤집고, 오른쪽으로 가면 원래대로. 멈추면 그대로.
	if (!knockedBack && v.x < -walkSpeedThreshold) facingLeft = true;
	else if (!knockedBack && v.x > walkSpeedThreshold) facingLeft = false;

	// 점프는 바로(위로 속도가 생기는 순간), 발판에서 떨어질 때는 airborneDelay 뒤에 공중으로 본다
	bool airborne = !grounded && (v.y > 0.0f || airTimer >= airborneDelay);
	float horizontalSpeed = std::sqrt(v.x * v.x + v.z * v.z);	// 3D의 깊이 이동(W/S)도 걷기로 본다

	PlayerSpriteState next;
	if (knockedBack) next = PlayerSpriteState::Hurt;
	else if (airborne) next = (v.y > 0.0f) ? PlayerSpriteState::JumpRise : PlayerSpriteState::JumpFall;
	else if (horizontalSpeed > walkSpeedThreshold) next = PlayerSpriteState::Walk;
	else next = PlayerSpriteState::Idle;

	if (next == PlayerSpriteState::Walk) {
		// 걷기를 새로 시작하면 walk_01부터
		if (state != PlayerSpriteState::Walk) walkTimer = 0.0f;
		else walkTimer = std::fmod(walkTimer + deltaTime, 4.0f / walkFps);
		walkFrame = static_cast<int>(walkTimer * walkFps) % 4;
	}

	state = next;
	ApplyFrame();
}

void PlayerSpriteAnimator::ApplyFrame() {
	if (!sprite) return;

	Texture* tex = idleTex;
	switch (state) {
	case PlayerSpriteState::Idle:		tex = idleTex;				frameName = "idle"; break;
	case PlayerSpriteState::Walk:		tex = walkTex[walkFrame];	frameName = "walk_0" + to_string(walkFrame + 1); break;
	case PlayerSpriteState::JumpRise:	tex = jumpRiseTex;			frameName = "jump_rise"; break;
	case PlayerSpriteState::JumpFall:	tex = jumpFallTex;			frameName = "jump_fall"; break;
	case PlayerSpriteState::Hurt:		tex = hurtTex;				frameName = "hurt"; break;
	case PlayerSpriteState::Clear:		tex = clearTex;				frameName = "clear"; break;
	}
	if (!tex) tex = idleTex;	// 빠진 그림은 기본 그림으로

	sprite->texture = tex;
	sprite->flipX = facingLeft;
}

void PlayerSpriteAnimator::SetOverrideState(PlayerSpriteState s) {
	hasOverride = true;
	overrideState = s;
}

void PlayerSpriteAnimator::ClearOverrideState() {
	hasOverride = false;
}

Component* PlayerSpriteAnimator::CreateFromJson(Object* obj, const json& props, const map<string, Object*>& spawnedObjects) {
	PlayerSpriteAnimator* anim = obj->AddComponent<PlayerSpriteAnimator>();

	if (props.contains("folder")) anim->folder = props["folder"].get<string>();
	if (props.contains("walkFps")) anim->walkFps = props["walkFps"];
	if (props.contains("walkSpeedThreshold")) anim->walkSpeedThreshold = props["walkSpeedThreshold"];
	if (props.contains("airborneDelay")) anim->airborneDelay = props["airborneDelay"];

	return anim;
}

REGISTER_COMPONENT(PlayerSpriteAnimator)
