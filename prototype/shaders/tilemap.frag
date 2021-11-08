#version 430 core

in TESSEVAL {
	vec3 position;
	vec3 barycentric;
	vec3 tile_color;
	vec3 edge_color;
	vec2 texcoord;
	float zclipspace;
} fragment;

uniform vec2 MARKER_POS;
uniform float MARKER_RADIUS;

void main(void)
{
	vec3 marker_color = vec3(1.0, 1.0, 0.0);

	vec3 final_color = fragment.tile_color;

	if (length(fragment.edge_color) == 0) {
	if (fragment.barycentric.b < 0.1) {
		float strength = 1.0 - fragment.barycentric.b;
		final_color = mix(final_color, fragment.edge_color, 0.8 * pow(strength, 6));
	}
	}

	float dist = distance(fragment.position.xz, MARKER_POS);
	if (dist < MARKER_RADIUS) {
		float transparency = dist / MARKER_RADIUS;
		transparency = 1.0 - pow(transparency, 4);
		final_color = mix(marker_color, final_color, transparency);
	}

	gl_FragColor = vec4(final_color, 1.0);
};
