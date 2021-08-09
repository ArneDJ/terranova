#version 460 core

layout (location = 0) in vec3 vposition;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec2 vtexcoords;

uniform mat4 CAMERA_VP;

out vec3 color;

void main()
{
	gl_Position = CAMERA_VP * vec4(vposition, 1.0);

	color = vnormal;
}
