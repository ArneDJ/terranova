#version 460 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

layout(std430, binding = 0) readonly buffer transforms
{
	float transform_SSBO[];
};

uniform mat4 MODEL;
uniform mat4 VP;
uniform bool WIRED_MODE;

out vec3 color;

void main()
{
	mat4 translation = mat4(1.0);
	//translation[3][0] = 3.0 * gl_BaseInstance;
	translation[3][0] = transform_SSBO[3 * gl_BaseInstance];
	translation[3][1] = transform_SSBO[3 * gl_BaseInstance+1];
	translation[3][2] = transform_SSBO[3 * gl_BaseInstance+2];

	gl_Position = VP * translation * vec4(vposition, 1.0);
	color = WIRED_MODE == true ? vec3(0.0) : vnormal;
}
