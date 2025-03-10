#version 440            
layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;
layout(location = 2) uniform float size = 50.0;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

layout(location = 0) in vec3 pos_attrib;
layout(location = 1) in vec3 vel_attrib;
layout(location = 2) in float age_attrib;

//write into the first bound tfo
layout(xfb_buffer = 0) out;

//interleave attrib order was pos, vel, age
//3 floats + 3 floats + 1 float = 7 floats -> stride = 7*sizeof(float) = 28 bytes

//pos offset = 0 bytes
layout(xfb_offset = 0, xfb_stride = 28) out vec3 pos_out; 

//vel offset = 3*sizeof(float) = 12 bytes
layout(xfb_offset = 12, xfb_stride = 28) out vec3 vel_out; 

//age offset = 6*sizeof(float) = 24 bytes
layout(xfb_offset = 24, xfb_stride = 28) out float age_out;

//Basic velocity field
vec3 velocity(vec3 p);

//pseudorandom number
vec3 hash( vec3 f );

vec3 gravity = vec3(0.0, -0.5, 0.0);

void main(void)
{
	//Draw current particles from vbo
	gl_Position = PV*M*vec4(pos_attrib, 1.0);
	gl_PointSize = size * abs(sin(time));

	//Compute particle attributes for next frame
	vel_out = velocity(pos_attrib);
	pos_out = pos_attrib + 0.003*vel_out.xyz;
	age_out = age_attrib - 1.0; //decrement age each frame

	//Reinitialize particles as needed with pseudorandom position and age
	if(age_out <= 0.0 || length(pos_out) > 2.5f)
	{
		vec3 seed = vec3(float(gl_VertexID), time/10.0, gl_Position.x); //seed for the random number generator
		vec3 rand = hash(seed);
		pos_out = vec3(2.0*rand.xy-vec2(1.0), -2.0).xzy;
		
		age_out = 500.0 + 1000.0*dot(rand,vec3(0.333));
	}
}


vec3 v0(vec3 p)
{
	return vec3(sin(p.y*5.0), -sin(p.x*7.0), +cos(1.2*p.z));
}

vec3 v(vec3 p)
{
	vec3 t = vec3(0.0, 0.5*time, 0.0);
	return v0(p.xyz-t) + v0(p.yxz+t);
} 

vec3 magnetic_field(vec3 p)
{
	float r = length(p);  // Distance from the center
	vec3 m = vec3(0.0, 1.0, 0.0); // Magnetic dipole moment along Y-axis

	// Dipole field equation: B = (3(m?r)r - m r^2) / r^5
	vec3 B = (3.0 * dot(m, p) * p - m * (r * r)) / pow(r, 5.0);

	return B; // Velocity follows field lines
}

vec3 velocity(vec3 p) //fractal sum of basic vector field
{
	const int n = 5;
	vec3 octaves = vec3(0.0);
	float scale = 1.5;
	for(int i=0; i<n; i++)
	{
		octaves = octaves + v(scale*p)/scale;
		scale*= 1.5;
	}
	//return 0.5*octaves.xzy;

	return 0.7 * magnetic_field(p);
}


////////////////////////////////////////////////////////////
//Pseudorandom number generation

const uint k_hash = 1103515245U;  // GLIB C

vec3 uhash3( uvec3 x )  
{
    x = ((x>>8U)^x.yzx)*k_hash;
    x = ((x>>8U)^x.yzx)*k_hash;
    x = ((x>>8U)^x.yzx)*k_hash;
    
    return vec3(x)/float(0xffffffffU);
}

vec3 hash( vec3 f )
{ 
    return uhash3( uvec3( floatBitsToUint(f.x),
                          floatBitsToUint(f.y),
                          floatBitsToUint(f.z) ) );
}

