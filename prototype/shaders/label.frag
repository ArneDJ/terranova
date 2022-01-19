#version 430 core

in vec2 texcoords;
in vec3 color;

out vec4 fcolor;

void main()
{    
	fcolor = vec4(color, 0.5);
} 
