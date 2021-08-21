#version 430 core

layout (location = 0) in vec2 vposition;
layout (location = 1) in vec3 vcolor;

out vec3 color;

void main(void)
{
	color = vcolor;

	gl_Position = vec4(vposition.x, 0.0, vposition.y, 1.0);
}
