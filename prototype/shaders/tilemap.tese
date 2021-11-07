#version 430 core

layout(triangles, equal_spacing, ccw) in;

in vec3 tile_color[];
in vec3 edge_color[];

out TESSEVAL {
	vec3 position;
	vec3 barycentric;
	vec3 tile_color;
	vec3 edge_color;
	vec2 texcoord;
	float zclipspace;
} tesseval;

uniform sampler2D DISPLACEMENT;

uniform mat4 CAMERA_VP;
uniform vec3 MAP_SCALE;

vec3 interpolate(vec3 v0, vec3 v1, vec3 v2)
{
   	return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;
}

void main(void)
{
	tesseval.tile_color = tile_color[0];
	tesseval.edge_color = mix(edge_color[0], edge_color[1], 0.5);

	tesseval.barycentric = gl_TessCoord;

	vec3 interpolated = interpolate(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	vec4 position = vec4(interpolated, 1.0);

	tesseval.texcoord = position.xz / MAP_SCALE.xz;

	position.y = MAP_SCALE.y * texture(DISPLACEMENT, tesseval.texcoord).r;

	tesseval.position = position.xyz;

	gl_Position = CAMERA_VP * position;

	tesseval.zclipspace = gl_Position.z;
}
