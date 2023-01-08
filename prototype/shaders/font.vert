#version 460 core

layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>

out vec2 texcoords;

uniform float SCALE;
uniform vec3 ORIGIN;
uniform mat4 PROJECT, VIEW;

void main()
{
	//gl_Position = PROJECT * vec4(vertex.xy, 0.0, 1.0);
	texcoords = vertex.zw;

	// spherical billboards
	mat4 T = mat4(1.0);
	T[3].xyz = ORIGIN;
	mat4 MV = VIEW * T;

	// column 0:
	MV[0][0] = 1;
	MV[0][1] = 0;
	MV[0][2] = 0;
	// column 1:
	MV[1][0] = 0;
	MV[1][1] = 1;
	MV[1][2] = 0;
	// column 2:
	MV[2][0] = 0;
	MV[2][1] = 0;
	MV[2][2] = 1;

	vec3 scaled_pos = SCALE * vec3(vertex.x, vertex.y, 0.0);

	gl_Position = PROJECT * MV * vec4(scaled_pos, 1.0);
}
