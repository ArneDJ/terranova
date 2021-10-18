#version 430 core

in vec2 texcoords;
in vec3 color;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D text;

void main()
{    
	vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texcoords).r);

	if (color.r < 0.1) {
		sampled.a = 1.0;
	}

	if (sampled.a < 0.5) { discard; }

	fcolor = vec4(color, 1.0) * sampled;
} 
