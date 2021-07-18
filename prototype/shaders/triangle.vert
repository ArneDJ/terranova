#version 460 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

layout(std430, binding = 5) readonly buffer transforms
{
	float transform_SSBO[];
};

//layout(binding = 1) readonly buffer SBO    { ImDrawVert data[]; } sbo;
//layout(binding = 2) readonly buffer IBO    { uint   data[]; } ibo;
//layout(binding = 3) readonly buffer DrawBO { DrawData data[]; } drawDataBuffer;

uniform mat4 VP;
uniform bool WIRED_MODE;

out vec3 color;

const vec3 col[8] = vec3[8](
	vec3( 1.0, 0.0, 0.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 1.0, 1.0, 0.0),
	vec3( 1.0, 1.0, 0.0),
	vec3( 0.0, 0.0, 1.0),
	vec3( 0.0, 1.0, 0.0),
	vec3( 1.0, 0.0, 0.0)
);
const int indices[36] = int[36](
	// front
	0, 1, 2, 2, 3, 0,
	// right
	1, 5, 6, 6, 2, 1,
	// back
	7, 6, 5, 5, 4, 7,
	// left
	4, 0, 3, 3, 7, 4,
	// bottom
	4, 5, 1, 1, 0, 4,
	// top
	3, 2, 6, 6, 7, 3
);
void main()
{
	mat4 model = mat4(1.0);
	model[3][0] = 3.0 * gl_BaseInstance;
	/*
	model[3][0] = transform_SSBO[3 * gl_BaseInstance];
	model[3][1] = transform_SSBO[3 * gl_BaseInstance+1];
	model[3][2] = transform_SSBO[3 * gl_BaseInstance+2];
	*/

	int idx = indices[gl_VertexID];
	//gl_Position = MVP * vec4(vposition, 1.0);
	gl_Position = VP * model * vec4(vposition, 1.0);
	color = WIRED_MODE == true ? vec3(0.0) : col[idx];
}
