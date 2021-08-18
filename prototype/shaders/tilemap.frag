#version 460 core

in vec3 position;
in vec3 color;
in vec3 barycentric;

void main()
{
	vec3 final_color = color;

	if (barycentric.b < 0.1) {
		float strength = 1.0 - barycentric.b;
		final_color = mix(final_color, vec3(1.0, 1.0, 1.0), 0.1 * pow(strength, 6));
	}

	gl_FragColor = vec4(final_color, 1.0);
};
