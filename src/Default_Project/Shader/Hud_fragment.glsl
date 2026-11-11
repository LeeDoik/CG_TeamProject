#version 330 core
uniform vec4 color;
uniform vec2 screen;
uniform int mode;      // 1 = vignette (darker toward the screen edges)

out vec4 FragColor;

void main()
{
	FragColor = color;
	if (mode == 1) {
		vec2 uv = gl_FragCoord.xy / screen - 0.5;
		uv.x *= screen.x / screen.y;
		float d = length(uv);
		FragColor.a *= smoothstep(0.45, 1.05, d);
	}
}
