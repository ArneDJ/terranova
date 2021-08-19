#version 430 core

layout(quads, fractional_even_spacing, ccw) in;

out TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} tesseval;

uniform sampler2D DISPLACEMENT;

uniform mat4 CAMERA_VP;
uniform vec3 MAP_SCALE;

void main(void)
{
	vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.y);
	vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.y);
	vec4 position = mix(p1, p2, gl_TessCoord.x);

	tesseval.texcoord = position.xz / MAP_SCALE.xz;

	position.y = MAP_SCALE.y * texture(DISPLACEMENT, tesseval.texcoord).r;

	tesseval.position = position.xyz;

	gl_Position = CAMERA_VP * position;

	tesseval.zclipspace = gl_Position.z;
}
