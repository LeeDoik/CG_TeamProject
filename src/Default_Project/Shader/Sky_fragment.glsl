#version 330 core
// [Game] Storybook sky: vertical gradient (by world y) on paper grain, unlit.
in vec3 out_Color;
in vec3 FragPos;
in vec3 Normal;

uniform vec3 skyTop;
uniform vec3 skyBottom;
uniform float skyY0;
uniform float skyY1;
uniform sampler2D paperTex;

out vec4 FragColor;

void main()
{
	float t = clamp((FragPos.y - skyY0) / (skyY1 - skyY0), 0.0, 1.0);
	t = t * t * (3.0 - 2.0 * t);
	vec3 c = mix(skyBottom, skyTop, t);
	vec3 paper = texture(paperTex, FragPos.xy * 0.06).rgb;
	FragColor = vec4(c * (0.88 + 0.12 * paper), 1.0);
}
