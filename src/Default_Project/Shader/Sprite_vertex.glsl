#version 330 core
// [Game] Sprite: textured quad. vColor.xy carries the texture coordinate.
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec3 vNormal;

out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
	gl_Position = proj * view * model * vec4(vPos, 1.0);
	uv = vColor.xy;
}
