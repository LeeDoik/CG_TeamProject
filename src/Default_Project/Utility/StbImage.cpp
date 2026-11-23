// stb_image 구현부
//  단일 헤더 라이브러리라 구현 코드는 이 파일 한 곳에서만 만든다.
//  다른 파일은 <stb/stb_image.h>를 그냥 include해서 함수 선언만 쓴다.
//  헤더 위치: dependencies/include/stb (OpenGL.props가 include 경로에 넣어 준다), 라이선스: dependencies/licenses/stb.txt

// SDL 검사(/sdl)가 켜져 있어 라이브러리 안의 CRT 함수 경고(C4996)가 오류가 되지 않도록 이 파일에서만 끈다
#define _CRT_SECURE_NO_WARNINGS

// 쓰는 형식만 켠다: PNG 읽기
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

// 외부 라이브러리 코드의 경고는 이 프로젝트 빌드 로그에 섞이지 않게 숨긴다
#pragma warning(push, 0)
#include <stb/stb_image.h>
#pragma warning(pop)
