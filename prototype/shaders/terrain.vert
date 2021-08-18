#version 430 core

layout(location = 0) in vec3 vposition;

void main(void)
{
	gl_Position = vec4(vposition, 1.0);
}

