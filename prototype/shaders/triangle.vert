#version 460 core

layout (location = 0) in vec3 vposition;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec2 vtexcoords;

layout (std430, binding = 1) readonly buffer transformation_SSBO
{
	vec4 transformations[];
};

struct MatrixBufferRows {
	vec4 rows[4];
};

layout (std430, binding = 2) buffer transform_matrices_SSBO
{
	MatrixBufferRows transform_matrices[];
};

uniform mat4 MODEL;
uniform mat4 VP;
uniform bool WIRED_MODE;

out vec3 color;

void main()
{
	MatrixBufferRows rows = transform_matrices[gl_BaseInstance];
	mat4 translation = mat4(rows.rows[0], rows.rows[1], rows.rows[2], rows.rows[3]);

	gl_Position = VP * translation * vec4(vposition, 1.0);
	color = WIRED_MODE == true ? vec3(0.0) : vnormal;
}
