#version 330 core
// [Game] Sprite: textured quad. vColor.xy carries the texture coordinate.
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec3 vNormal;

out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform int alignBackdropBase;
uniform vec3 backdropFoot;

void main()
{
	vec4 world = model * vec4(vPos, 1.0);
	mat4 vp = proj * view;
	if (alignBackdropBase == 1 && vColor.z > 0.5) {
		// Solve the bottom vertex height so all paper layers end on the same
		// projected line. Works with orthographic, perspective and their blend.
		vec4 foot = vp * vec4(world.x, backdropFoot.y, backdropFoot.z, 1.0);
		vec4 current = vp * world;
		if (foot.w > 0.0001) {
			float targetY = foot.y / foot.w;
			float slope = vp[1][1] - targetY * vp[1][3];
			if (abs(slope) > 0.0001) world.y += (targetY * current.w - current.y) / slope;
		}
	}
	gl_Position = vp * world;
	uv = vColor.xy;
}
