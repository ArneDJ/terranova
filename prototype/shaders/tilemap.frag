#version 430 core

in TESSEVAL {
	vec3 position;
	vec3 barycentric;
	vec3 color;
	vec2 texcoord;
	float zclipspace;
} fragment;

void main(void)
{
	vec3 final_color = fragment.color;

	if (fragment.barycentric.b < 0.1) {
		float strength = 1.0 - fragment.barycentric.b;
		final_color = mix(final_color, vec3(1.0, 1.0, 1.0), 0.1 * pow(strength, 6));
	}

	gl_FragColor = vec4(final_color, 1.0);
};
