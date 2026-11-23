#pragma once
#include <string>
#include <map>
#include <gl/glew.h>
using namespace std;

// PNG 한 장을 읽어 OpenGL RGBA 텍스처로 올린 것
//  - 이미지 위/아래 뒤집기는 Load 안에서 한 번만 한다 (stbi_set_flip_vertically_on_load).
//    그래서 UV (0,0)은 그림의 왼쪽 아래, (1,1)은 오른쪽 위다. 셰이더·쿼드에서는 더 뒤집지 않는다.
//  - 감싸기: GL_CLAMP_TO_EDGE (가장자리 밖 반복 없음), 필터: GL_LINEAR
//  - straight alpha 그대로 올린다. 블렌딩은 GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA (SpriteRenderer)
//  - 같은 경로는 한 번만 읽고 캐시한 것을 돌려준다 (프로그램 끝까지 유지)
class Texture {
public:
	GLuint id = 0;
	int width = 0;
	int height = 0;
	string path;

	// 실패하면 nullptr (콘솔에 이유를 찍는다). GL 컨텍스트가 만들어진 뒤에 불러야 한다.
	static Texture* Load(const string& path);

	void Bind(int unit = 0) const;

private:
	static map<string, Texture*>& Cache();
};
