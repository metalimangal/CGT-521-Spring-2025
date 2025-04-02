#version 430 

//layout (quads, equal_spacing, ccw) in;
//Try some of these other options
//layout (quads, fractional_odd_spacing, ccw) in;
layout (quads, fractional_even_spacing, ccw) in;


void main()
{
	const float u = gl_TessCoord[0];
	const float v = gl_TessCoord[1];

	const vec4 p0 = gl_in[0].gl_Position;
	const vec4 p1 = gl_in[1].gl_Position;
	const vec4 p2 = gl_in[2].gl_Position;
	const vec4 p3 = gl_in[3].gl_Position;

	vec4 a = mix(p0, p1, u);
	vec4 b = mix(p3, p2, u);
	vec4 c = mix(a, b, v);

	gl_Position = c;
}
