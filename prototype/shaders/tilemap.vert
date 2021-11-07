#version 430 core

layout (location = 0) in vec2 vposition;
layout (location = 1) in vec3 vcolor;
layout (location = 2) in vec3 edge_color;

out vec3 v_cell_color;
out vec3 v_edge_color;

void main(void)
{
	v_cell_color = vcolor;
	v_edge_color = edge_color;

	gl_Position = vec4(vposition.x, 0.0, vposition.y, 1.0);
}
