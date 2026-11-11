#pragma once
#include <string>
#include <gl/glm/glm.hpp>
using namespace glm;

// 시점 모드: 2D(직교투영, 기본) / 3D(원근투영)
enum class ViewMode { View2D, View3D };

// 축 정렬 상자(AABB). 레벨의 충돌·트리거 판정은 모두 이 상자로 한다.
struct Box {
	vec3 min{ 0.0f };
	vec3 max{ 0.0f };

	static Box FromCenterSize(const vec3& center, const vec3& size) {
		return Box{ center - size * 0.5f, center + size * 0.5f };
	}
	vec3 Center() const { return (min + max) * 0.5f; }
	vec3 Size() const { return max - min; }
};

// u8"..." 문자열을 std::string으로 바꾼다. GLFW 창 제목은 UTF-8 문자열을 받는다.
inline std::string U8(const char8_t* s) { return std::string(reinterpret_cast<const char*>(s)); }
