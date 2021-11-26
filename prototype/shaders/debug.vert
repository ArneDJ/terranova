#version 460 core

layout (location = 0) in vec3 vposition;
layout (location = 1) in vec3 vnormal;

uniform mat4 CAMERA_VP;
uniform mat4 MODEL;
uniform bool WIRED_MODE;

out vec3 vcolor;

void main()
{
	mat4 translation = MODEL;
		
	gl_Position = CAMERA_VP * translation * vec4(vposition, 1.0);

	vec3 normal = normalize(mat3(transpose(inverse(translation))) * normalize(vnormal));

	vcolor = WIRED_MODE == true ? vec3(0.0) : normal;
}
