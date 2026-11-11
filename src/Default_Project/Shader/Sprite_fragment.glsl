#version 330 core
// [Game] Sprite with alpha test: transparent pixels are discarded (no blending needed).
in vec2 uv;

uniform sampler2D spriteTex;
uniform vec4 tColor;

out vec4 FragColor;

void main()
{
	vec4 t = texture(spriteTex, uv);
	if (t.a < 0.5) discard;
	FragColor = vec4(t.rgb * tColor.rgb, 1.0);
}
