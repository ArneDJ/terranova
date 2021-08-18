#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

void main(void)
{
	fcolor = vec4(1.0, 1.0, 1.0,  1.0);
}
