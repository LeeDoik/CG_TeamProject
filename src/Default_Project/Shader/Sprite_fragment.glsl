#version 330 core
//종이 스프라이트(텍스처 사각형)용 프래그먼트 셰이더
//straight alpha 그림을 GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA로 섞는다 (블렌딩 설정은 SpriteRenderer)

in vec2 uv;

uniform sampler2D spriteTex;	//텍스처 유닛 0
uniform vec4 tint;				//그림 색에 곱하는 색 (기본 흰색 = 그림 그대로)
uniform float alphaCutoff;		//이보다 투명한 픽셀은 버린다

out vec4 FragColor;

void main()
{
	vec4 color = texture(spriteTex, uv) * tint;

	//알파 테스트: 거의 투명한 픽셀은 색도 깊이도 쓰지 않는다.
	//버리지 않으면 투명한 사각형 부분이 깊이 버퍼에 남아 뒤에 그려지는 물체를 가린다.
	if (color.a < alphaCutoff) discard;

	FragColor = color;
}
