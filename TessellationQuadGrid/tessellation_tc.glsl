#version 430

layout(location=6) uniform vec2 mouse_pos;

layout (vertices = 4) out;  //number of output verts of the tess. control shader

const float max_lod = 10.0;

float calc_lod(float dist)
{
	float lod = 1.0/dist;
	return clamp(lod, 1.0, max_lod);
}

void main()
{

	//Pass-through: just copy input vertices to output
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

	float d0 = distance(mouse_pos, mix(gl_in[3].gl_Position.xy, gl_in[0].gl_Position.xy, 0.5));
	float d1 = distance(mouse_pos, mix(gl_in[0].gl_Position.xy, gl_in[1].gl_Position.xy, 0.5));
	float d2 = distance(mouse_pos, mix(gl_in[1].gl_Position.xy, gl_in[2].gl_Position.xy, 0.5));
	float d3 = distance(mouse_pos, mix(gl_in[2].gl_Position.xy, gl_in[3].gl_Position.xy, 0.5));
	float d_avg = 0.25*(d0+d1+d2+d3);

	gl_TessLevelOuter[0] = calc_lod(d0);
	gl_TessLevelOuter[1] = calc_lod(d1);
	gl_TessLevelOuter[2] = calc_lod(d2);
	gl_TessLevelOuter[3] = calc_lod(d3);

	gl_TessLevelInner[0] = calc_lod(d_avg);
	gl_TessLevelInner[1] = calc_lod(d_avg);
}
