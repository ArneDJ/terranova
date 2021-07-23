#version 460 core

layout (location = 0) in vec3 vposition;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec2 vtexcoords;

layout (std430, binding = 2) readonly buffer ModelMatricesBlock
{
	mat4 model_matrices[];
};

layout (binding = 4) uniform CameraBlock {
	mat4 view;
	mat4 projection;
	mat4 view_projection;
	vec4 position;
	vec4[6] frustum_planes;
} camera;

uniform mat4 MODEL;
uniform bool WIRED_MODE;
uniform bool INDIRECT_DRAW;

out vec3 color;

void main()
{
	mat4 translation = mat4(1.0);
	if (INDIRECT_DRAW == true) {
		translation = model_matrices[gl_BaseInstance];
	} else {
		translation = MODEL;
	}
		
	gl_Position = camera.view_projection * translation * MODEL * vec4(vposition, 1.0);

	color = WIRED_MODE == true ? vec3(0.0) : vnormal;
}
