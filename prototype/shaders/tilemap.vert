#version 460 core

layout (location = 0) in vec2 vposition;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec3 vbarycentric;

uniform mat4 CAMERA_VP;

out vec3 position;
out vec3 color;
out vec3 barycentric;

void main()
{
	position = vec3(vposition.x, 0.0, vposition.y);
	gl_Position = CAMERA_VP * vec4(position, 1.0);

	color = vnormal;
	barycentric = vbarycentric;
}
