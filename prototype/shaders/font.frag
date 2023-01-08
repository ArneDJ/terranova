#version 460 core

in vec2 texcoords;

out vec4 frag_color;

uniform sampler2D text;
uniform vec3 COLOR;

void main()
{
	//frag_color = vec4(COLOR, 1.0);
	vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texcoords).r);
	frag_color = vec4(COLOR, 1.0) * sampled;
};
