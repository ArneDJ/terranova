#version 460 core

layout (location = 0) in vec3 vposition;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec2 vtexcoords;
layout (location = 3) in ivec4 vjoints;
layout (location = 4) in vec4 vweights;

layout (std430) readonly buffer JointMatricesBlock
{
	mat4 joint_matrices[];
};

uniform mat4 MODEL;
uniform mat4 CAMERA_VP;
uniform bool WIRED_MODE;

out vec3 color;

void main()
{
	mat4 translation = MODEL;

	mat4 skin = vweights.x * joint_matrices[int(vjoints.x)];
	skin += vweights.y * joint_matrices[int(vjoints.y)];
	skin += vweights.z * joint_matrices[int(vjoints.z)];
	skin += vweights.w * joint_matrices[int(vjoints.w)];
		
	gl_Position = CAMERA_VP * translation * skin * vec4(vposition, 1.0);

	color = vnormal;
}
