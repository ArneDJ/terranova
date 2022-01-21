#version 430 core

in TESSEVAL {
	vec3 position;
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
uniform sampler2D POLITICAL;
uniform sampler2D BOUNDARIES;

// surface color textures
uniform sampler2D ROCK;
uniform sampler2D BUMP;

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
	
	vec3 bump = texture(BUMP, 50.0 * fragment.texcoord).rbg;
	bump = (bump * 2.0) - 1.0;

	vec3 tangent = normalize(cross(normal, vec3(0.0, 0.0, 1.0)));
	vec3 bitangent = normalize(cross(tangent, normal));
	mat3 orthobasis = mat3(tangent, normal, bitangent);
	normal = normalize(orthobasis * bump);

	vec3 rock_color = texture(ROCK, 50.0 * fragment.texcoord).rbg;

	vec4 political = texture(POLITICAL, fragment.texcoord);

	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	
	vec3 final_color = vec3(height);

	final_color = mix(final_color, rock_color, 0.5);

	float border = texture(BORDERS, fragment.texcoord).r;

	float political_mix = texture(BOUNDARIES, fragment.texcoord).r;
	political_mix = smoothstep(0.1, 0.2, political_mix);
	float negative_border = 1.0 - smoothstep(0.1, 0.2, border);
	final_color = mix(final_color, political.rgb, 0.5 * negative_border * political_mix * political.a);

	vec3 border_color = mix(final_color, vec3(1.0, 1.0, 1.0), BORDER_MIX);
	final_color = mix(final_color, border_color, smoothstep(0.1, 0.2, border));

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
