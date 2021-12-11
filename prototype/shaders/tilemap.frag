#version 430 core

in TESSEVAL {
	vec3 position;
	vec3 barycentric;
	vec3 tile_color;
	vec3 edge_color;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

uniform vec2 MARKER_POS;
uniform vec3 MARKER_COLOR;
uniform float MARKER_RADIUS;
uniform float MARKER_FADE;

void main(void)
{
	vec3 marker_color = MARKER_COLOR;

	vec3 final_color = fragment.tile_color;

	if (length(fragment.edge_color) == 0) {
	if (fragment.barycentric.b < 0.1) {
		float strength = 1.0 - fragment.barycentric.b;
		final_color = mix(final_color, fragment.edge_color, 0.8 * pow(strength, 6));
	}
	}

	if (MARKER_FADE > 0.f) {
		float dist = distance(fragment.position.xz, MARKER_POS);
		if (dist < MARKER_RADIUS) {
			float transparency = dist / MARKER_RADIUS;
			transparency = 1.0 - pow(transparency, 4);
			marker_color = mix(marker_color, final_color, transparency);
			final_color = mix(final_color, marker_color, MARKER_FADE);
		}
	}

	fcolor = vec4(final_color, 1.0);
};
