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

	// I guess this is something like signed distance fields?
	if (sampled.a < 0.4) { discard; }
	sampled.a = 1.0;

	fcolor = vec4(color, 1.0) * sampled;
} 
