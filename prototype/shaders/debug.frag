#version 460 core

in vec3 vcolor;

out vec4 fcolor;

void main()
{
	vec3 color = vcolor + vec3(1.0, 1.0, 1.0);
	fcolor = vec4(0.5 * color, 1.0);
};
