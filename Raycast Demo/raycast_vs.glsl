#version 430            
layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;
layout(location = 2) uniform int pass = 0;
layout(location = 3) uniform int mode = 1;

//Enum for modes
const int OUTSIDE_LOOKING_IN = 0;
const int INSIDE_LOOKING_OUT = 1;

layout(std140, binding = 0) uniform SceneUniforms
{
	mat4 PV;	//camera projection * view matrix
	mat4 PVinv;
	vec4 eye_w;	//world-space eye position
};

out VertexData
{
   vec3 pw;       //world-space vertex position
} outData; 

const vec4 cube[8] = vec4[]( vec4(-1.0, -1.0, -1.0, 1.0),
							vec4(-1.0, +1.0, -1.0, 1.0),
							vec4(+1.0, +1.0, -1.0, 1.0),
							vec4(+1.0, -1.0, -1.0, 1.0),
							vec4(-1.0, -1.0, +1.0, 1.0),
							vec4(-1.0, +1.0, +1.0, 1.0),
							vec4(+1.0, +1.0, +1.0, 1.0),
							vec4(+1.0, -1.0, +1.0, 1.0));

const int index[14] = int[](1, 0, 2, 3, 7, 0, 4, 1, 5, 2, 6, 7, 5, 4);

void main(void)
{
	int ix = index[gl_VertexID];
	vec4 v = cube[ix];

	if(mode==OUTSIDE_LOOKING_IN)
	{
		gl_Position = PV*M*v;
		outData.pw = vec3(M*v);		//world-space vertex position determines world-space ray endpoints
	}
	if(mode==INSIDE_LOOKING_OUT) //inside looking out mode
	{
		v.w = 1.0+1e-6; //nudge vertices to avoid clipping
		gl_Position = v;

		//Convert from clip to world space
		v.z = -v.z;
		vec4 vw = PVinv*v;
		vw = vw/vw.w;
		outData.pw = vw.xyz;		//world-space vertex position
	}
}
