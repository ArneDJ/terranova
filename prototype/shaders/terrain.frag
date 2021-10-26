#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

uniform sampler2D TILES;

void main(void)
{
	fcolor = texture(TILES, 100.0 * fragment.texcoord);
	//fcolor = vec4(1.0, 1.0, 1.0,  1.0);
}
