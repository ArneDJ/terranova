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

uniform float BORDER_MIX;

uniform sampler2D DISPLACEMENT;
uniform sampler2D NORMALMAP;
uniform sampler2D BORDERS;

vec3 apply_light(vec3 color, vec3 normal)
{
	const vec3 lightdirection = vec3(0.5, 0.93, 0.1);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, lightdirection));
	vec3 scatteredlight = lightcolor * diffuse;

	return mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);
}

void main(void)
{
	vec3 marker_color = MARKER_COLOR;

	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;

	vec3 final_color = fragment.tile_color;

	float border = texture(BORDERS, fragment.texcoord).r;
	if (border > 0.2) {
		final_color = mix(final_color, vec3(1.0, 1.0, 1.0), BORDER_MIX);
	}
	/*
	if (length(fragment.edge_color) == 0) {
	if (fragment.barycentric.b < 0.1) {
		float strength = 1.0 - fragment.barycentric.b;
		final_color = mix(final_color, fragment.edge_color, 0.8 * pow(strength, 6));
	}
	}
	*/

	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	final_color = mix(final_color, vec3(height), 0.5f);

	// terrain lighting
	final_color = apply_light(final_color, normal);

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
