#version 430 core

layout(vertices = 3) out;

in vec3 color[];

out vec3 tesscolor[];

void main(void)
{
	float detail = 2;
	if (gl_InvocationID == 0) {
		gl_TessLevelOuter[0] = detail;
		gl_TessLevelOuter[1] = detail;
		gl_TessLevelOuter[2] = detail;
		gl_TessLevelInner[0] = detail;
	}

	tesscolor[gl_InvocationID] = color[gl_InvocationID];

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
