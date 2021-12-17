#version 430 core

layout(vertices = 3) out;

in vec3 v_cell_color[];
in vec3 v_edge_color[];

out vec3 tile_color[];
out vec3 edge_color[];

uniform vec3 CAMERA_POSITION;

float lod(float dist)
{
	//float tessfactor = smoothstep(100.0, 140.0, dist);

	//return mix(2.0, 4.0, 1.0 - tessfactor);
	return (dist > 100.0) ? 2.0 : 4.0;
}

// add half vector
precise vec2 midpoint(vec2 a, vec2 b)
{
	return 0.5 * (a + b);
}

void main(void)
{
	float detail = 4;
	if (gl_InvocationID == 0) {
		gl_TessLevelOuter[0] = detail;
		gl_TessLevelOuter[1] = detail;
		gl_TessLevelOuter[2] = detail;
		gl_TessLevelInner[0] = detail;
	}

	tile_color[gl_InvocationID] = v_cell_color[gl_InvocationID];
	edge_color[gl_InvocationID] = v_edge_color[gl_InvocationID];

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
