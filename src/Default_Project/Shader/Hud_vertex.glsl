#version 330 core
// HUD(screen-space UI): draws a rectangle given in pixels (origin = top-left).
layout(location = 0) in vec2 vPos;   // unit quad 0..1

uniform vec4 rect;                   // x, y, width, height (pixels)
uniform vec2 screen;                 // screen size (pixels)

void main()
{
	vec2 p = rect.xy + vPos * rect.zw;
	gl_Position = vec4(p.x / screen.x * 2.0 - 1.0, 1.0 - p.y / screen.y * 2.0, 0.0, 1.0);
}
