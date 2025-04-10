#version 430
layout(binding = 0) uniform sampler2D backface_tex; 
layout(location = 1) uniform float time;
layout(location = 2) uniform int pass;
layout(location = 3) uniform int mode = 1;
layout(location = 4) uniform vec4 slider = vec4(0.0);
layout(location = 6) uniform float animSpeed;
layout(std140, binding = 0) uniform SceneUniforms
{
	mat4 PV;	//camera projection * view matrix
	mat4 PVinv;
	vec4 eye_w;	//world-space eye position
};

layout(std140, binding = 1) uniform LightUniforms
{
   vec4 La;	//ambient light color
   vec4 Ld;	//diffuse light color
   vec4 Ls;	//specular light color
   vec4 light_w; //world-space light position
};

layout(std140, binding = 2) uniform MaterialUniforms
{
   vec4 ka;	//ambient material color
   vec4 kd;	//diffuse material color
   vec4 ks;	//specular material color
   float shininess; //specular exponent
};

in VertexData
{
   vec3 pw;       //world-space vertex position
} inData;   //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

/////////////////////////////////////////////////////////
//Function forward declarations
// 
//the raymarching loop
vec4 raycast_sdf_scene(vec3 rayStart, vec3 rayStop);

//determines the color when ray misses the scene
vec4 sky_color(vec3 dir);

//Scene representation as a signed distance function
float dist_to_scene(vec3 pos);

//Computes the normal vector as gradient of scene sdf
vec3 normal(vec3 pos);

//Computes Phong lighting
vec4 lighting(vec3 pos, vec3 rayDir);

//Shadow
float softshadow( vec3 ro, vec3 rd, float mint, float maxt, float k );

//shape function declarations
float sdSphere( vec3 p, float s );
float sdBox( vec3 p, vec3 b );
float sdTorus(vec3 p, vec2 t);
float sdBoxFrame(vec3 p, vec3 b, float e);
float sdCone(vec3 p, vec2 c, float h);
float sdDeathStar(vec3 p2, float ra, float rb, float d);

//shape modifiers
float opUnion( float d1, float d2 ) { return min(d1,d2); }
float opSubtraction(float d1, float d2) {
    return max(d1, -d2);
}
float opIntersection(float d1, float d2) {
    return max(d1, d2);
}

// For more distance functions see
// http://iquilezles.org/www/articles/distfunctions/distfunctions.htm

// Soft shadows
// http://www.iquilezles.org/www/articles/rmshadows/rmshadows.htm

// WebGL example and a simple ambient occlusion approximation
// https://www.shadertoy.com/view/Xds3zN


void main(void)
{   
   if(pass == 0)
	{
		fragcolor = vec4(inData.pw, 1.0); //write cube back face positions to texture
	}
   else if(pass == 1)
   {
		//DEBUG: uncomment to see front faces
		//fragcolor = vec4(inData.pw, 1.0); //draw cube front faces
		//return;

        vec3 rayStart = inData.pw;
		vec3 rayStop = texelFetch(backface_tex, ivec2(gl_FragCoord.xy), 0).xyz;
        fragcolor = raycast_sdf_scene(rayStart, rayStop);

		//gamma
		fragcolor = pow(fragcolor, vec4(0.45, 0.45, 0.45, 1.0));
   }
}

//You shouldn't need to change this function for Lab 3
vec4 raycast_sdf_scene(vec3 rayStart, vec3 rayStop)
{
	const int MaxSamples = 10000; //max number of steps along ray

	vec3 rayDir = normalize(rayStop-rayStart);	//ray direction unit vector
	float travel = distance(rayStop, rayStart);	
	float stepSize = travel/MaxSamples;	//initial raymarch step size
	vec3 pos = rayStart;				      //position along the ray
	vec3 step = rayDir*stepSize;		   //displacement vector along ray
	
	for (int i=0; i < MaxSamples && travel > 0.0; ++i, pos += step, travel -= stepSize)
	{
		float dist = dist_to_scene(pos); //How far are we from the shape we are raycasting?

		//Distance tells us how far we can safely step along ray without intersecting surface
		stepSize = dist;

		step = rayDir*stepSize;
		
		//Check distance, and if we are close then perform lighting
		const float eps = 1e-6;
		if(dist <= eps)
		{	
			pos += step;
			return lighting(pos, rayDir);
		}	
	}
	//If the ray never intersects the scene then output clear color
	return sky_color(rayDir);
}

// Define a flag to enable or disable sin(time) usage
bool useSinTime = true;  // Set this flag to true or false to control the behavior

// Function to compute sin(time) if enabled
float sinTimeEffect(float value)
{
    return useSinTime ? sin(time*animSpeed) : 0.0;
}

float dist_to_scene(vec3 pos)
{
    // Define the centers of the objects
    vec3 sphere_cen = vec3(0.2, -0.7, 0.0);
    vec3 torus_cen = vec3(0.5 * sinTimeEffect(0.0), 0.5, -0.5 * sinTimeEffect(0.0));
    vec3 deathStarOffset = vec3(0.0, 0.3, -0.3 * sinTimeEffect(0.0));

    // Define independent offsets for each behind shape
    vec3 behindOffset1 = vec3(0.4, 0.1, -0.3 * sinTimeEffect(0.0)); // For BoxFrame
    vec3 behindOffset2 = vec3(0.0, 0.0, -0.3 * sinTimeEffect(0.0)); // For Cone
    vec3 behindOffset3 = vec3(-0.5, -0.5, 0.0); 

    // Calculate distances for each shape
    float d1 = sdSphere(pos - sphere_cen, 0.2);
    float d2 = sdBox(pos - behindOffset3, vec3(0.5, 0.1, 0.1));
    float d3 = sdTorus(pos - torus_cen, vec2(0.1));
    float d4 = sdBoxFrame(pos - behindOffset1, vec3(0.5, 0.5, 0.1), 0.05);
    float d5 = sdCone(pos - behindOffset2, vec2(0.5, 1.75), 1.9);
    float d6 = sdDeathStar(pos - deathStarOffset, 0.5, 0.2, 0.3);

	float coneSphereSub = opSubtraction(d5, d1);
	float boxSpehereInt = opIntersection(d6, d3);

    
    // Return the final union of all distances
    return opUnion(d2, opUnion(boxSpehereInt, opUnion(d4, coneSphereSub)));
}



//compute lighting on the intersected surface
vec4 lighting(vec3 pos, vec3 rayDir)
{
   vec4 ambient_term = ka*La;

   vec3 nw = normal(pos);			//world-space unit normal vector
   vec3 lw = normalize(light_w.xyz - pos);	//world-space unit light vector
   
   float shadowFactor = softshadow(pos + nw * 0.02, lw, 0.02, 2.0, 16.0); // Offset along normal
   
   vec4 diffuse_term = kd*Ld*max(0.0, dot(nw, lw))* shadowFactor ;

   vec3 vw = -rayDir;	//world-space unit view vector
   vec3 rw = reflect(-lw, nw);	//world-space unit reflection vector

   vec4 specular_term = ks*Ls*pow(max(0.0, dot(rw, vw)), shininess) * shadowFactor;

   return ambient_term + diffuse_term + specular_term;
}


float softshadow( vec3 ro, vec3 rd, float mint, float maxt, float k )
{
    float res = 1.0;
    float t = mint;
    for( int i=0; i<256 && t<maxt; i++ )
    {
        float h = dist_to_scene(ro + rd*t);
        if( h<0.001 )
            return 0.0;
        res = min( res, k*h/t );
        t += h;
    }
    return res;
}

//normal vector of the shape we are drawing.
//DO not change this function for Lab 3
vec3 normal(vec3 pos)
{
	const float h = 0.001;
	const vec3 Xh = vec3(h, 0.0, 0.0);	
	const vec3 Yh = vec3(0.0, h, 0.0);	
	const vec3 Zh = vec3(0.0, 0.0, h);	

	//compute gradient using central differences
	return normalize(vec3(dist_to_scene(pos+Xh)-dist_to_scene(pos-Xh), dist_to_scene(pos+Yh)-dist_to_scene(pos-Yh), dist_to_scene(pos+Zh)-dist_to_scene(pos-Zh)));
}

vec4 sky_color(vec3 dir)
{
	//vec3 topColor = vec3(0.2, 0.0, 0.5);		//purple
    //vec3 bottomColor = vec3(1.0, 0.4, 0.1);		//orange

	vec3 topColor = vec3(0.2, 0.0, 0.5);		//purple
    vec3 bottomColor = vec3(1.0, 0.4, 0.1);		//orange

	float t = pow(0.5 * (dir.y + 1.0), 0.5); 
	vec3 sky = bottomColor * (1.0 - t) + topColor * t;
	

	// Starfield generation
    float starDensity = 10.0;  // Adjust for more/less stars
    vec2 uv = fract(dir.xz * starDensity);
    float stars = step(0.98, fract(sin(dot(uv, vec2(13, 79))) * 43758.5453));

    // Sinusoidal twinkle effect
    float flicker = 0.5 + 0.5 * sin(time * 5.0 + dot(uv, vec2(10.0, 10.0)));
    stars *= flicker;

    // Add stars to sky
    sky += vec3(stars);

    return vec4(sky, 1.0); // Return color with full opacity
   
   
   //return vec4(0.0);
   
}

float sdSphere( vec3 p, float s )
{
	return length(p)-s;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdTorus(vec3 p, vec2 t)
{
	vec2 q = vec2(length(p.xz) - t.x, p.y);
	return length(q) - t.y;
}

float sdBoxFrame(vec3 p, vec3 b, float e)
{
	p = abs(p) - b;
	vec3 q = abs(p + e) - e;
	return min(min(
		length(max(vec3(p.x, q.y, q.z), 0.0)) + min(max(p.x, max(q.y, q.z)), 0.0),
		length(max(vec3(q.x, p.y, q.z), 0.0)) + min(max(q.x, max(p.y, q.z)), 0.0)),
		length(max(vec3(q.x, q.y, p.z), 0.0)) + min(max(q.x, max(q.y, p.z)), 0.0));
}

float sdCone(vec3 p, vec2 c, float h)
{
	vec2 q = h * vec2(c.x / c.y, -1.0);

	vec2 w = vec2(length(p.xz), p.y);
	vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
	vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
	float k = sign(q.y);
	float d = min(dot(a, a), dot(b, b));
	float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
	return sqrt(d) * sign(s);
}

float sdDeathStar(vec3 p2, float ra, float rb, float d)
{
	// sampling independent computations (only depend on shape)
	float a = (ra * ra - rb * rb + d * d) / (2.0 * d);
	float b = sqrt(max(ra * ra - a * a, 0.0));

	// sampling dependant computations
	vec2 p = vec2(p2.x, length(p2.yz));
	if (p.x * b - p.y * a > d * max(b - p.y, 0.0))
		return length(p - vec2(a, b));
	else
		return max((length(p) - ra),
			-(length(p - vec2(d, 0.0)) - rb));
}
