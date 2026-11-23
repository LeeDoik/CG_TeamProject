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

//깊이 테스트에만 쓰는 카메라 거리 배율 (SpriteRenderer::GetDepthScale)
//1이면 실제 깊이(3D). 2D에서는 1보다 작아서, 앞쪽 물체보다 카메라에 가까운 것처럼 깊이를 비교한다.
uniform float depthScale;

void main()
{
	vec4 viewPos = view * model * vec4(vPos, 1.0);
	gl_Position = proj * viewPos;

	//깊이(z)만 카메라 쪽으로 당긴 점에서 가져온다. x, y, w는 그대로라 화면 위치·크기는 바뀌지 않는다.
	//뷰 공간에서 카메라는 -z를 보므로 viewPos.z에 배율을 곱하면 카메라까지 거리가 그만큼 줄어든다.
	if (depthScale < 1.0) {
		vec4 pulled = proj * vec4(viewPos.xy, viewPos.z * depthScale, 1.0);
		gl_Position.z = pulled.z / pulled.w * gl_Position.w;
	}

	uv = vec2(flipX ? 1.0 - vUV.x : vUV.x, vUV.y);
}
