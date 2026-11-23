#version 330 core
//종이 스프라이트(텍스처 사각형)용 버텍스 셰이더
//Position: attribute index 0
//UV: attribute index 1  ((0,0) = 그림 왼쪽 아래. 위/아래 뒤집기는 텍스처를 읽을 때 이미 했다)

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec2 vUV;

out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

//true면 그림을 좌우로 뒤집는다 (왼쪽을 볼 때). 위/아래는 건드리지 않는다.
uniform bool flipX;

void main()
{
	gl_Position = proj * view * model * vec4(vPos, 1.0);
	uv = vec2(flipX ? 1.0 - vUV.x : vUV.x, vUV.y);
}
