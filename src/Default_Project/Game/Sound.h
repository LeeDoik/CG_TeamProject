#pragma once

// 효과음·배경음 (Windows 기본 winmm의 MCI 사용, 외부 라이브러리 없음).
// 소리 파일은 Sounds/<이름>.wav. 테스트(POPUP_HEADLESS)나 POPUP_MUTE에서는 소리를 끈다. M 키로 끄고 켠다.
namespace Sound {
	void Init(bool muted);
	void Play(const char* name);     // jump, switch, blocked, hurt, stomp, checkpoint, pickup, clear, select
	void PlayMusic();                // 배경음 반복
	void SetMuted(bool muted);
	bool IsMuted();
	void Shutdown();
}
