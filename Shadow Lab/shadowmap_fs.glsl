#version 430
layout(binding = 0) uniform sampler2DShadow shadow_map;
layout(location = 1) uniform float time;
layout(location = 2) uniform int pass;
layout(location = 3) uniform int u_pcf_mode; // 0 for 2x2, 1 for 4x4
layout(location = 4) uniform float u_w_numerator;
layout(location = 5) uniform int u_debug_lit4;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   mat4 V;
   mat4 Shadow;
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
   vec2 tex_coord;
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
   vec4 shadow_coord;
} inData;   //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

vec4 lit4;

void main(void)
{   
   //Compute per-fragment Phong lighting

   //float z = texture(shadow_map, inData.shadow_coord.xy/inData.shadow_coord.w).r; // light-space depth in the shadowmap
	//float r = inData.shadow_coord.z / inData.shadow_coord.w; // light-space depth of this fragment
	//float lit = float(r <= z); // if ref is closer than z then the fragment is lit
	float lit = textureProj(shadow_map, inData.shadow_coord);
	float w = u_w_numerator / textureSize(shadow_map, 0).x;

	if (u_pcf_mode == 0) {
		float lit = textureProj(shadow_map, inData.shadow_coord);
	}
	else {

		lit4[0] = textureProj(shadow_map, inData.shadow_coord + vec4(-w, -w, 0.0, 0.0));
		lit4[1] = textureProj(shadow_map, inData.shadow_coord + vec4(+w, -w, 0.0, 0.0));
		lit4[2] = textureProj(shadow_map, inData.shadow_coord + vec4(-w, +w, 0.0, 0.0));
		lit4[3] = textureProj(shadow_map, inData.shadow_coord + vec4(+w, +w, 0.0, 0.0));
		/*vec3 coord = inData.shadow_coord.xyz / inData.shadow_coord.w;
		vec4 lit4 = textureGather(shadow_map, coord.xy, coord.z);*/

		lit = dot(lit4, vec4(0.25));
	}

   vec4 ambient_term = ka*La;

   const float eps = 1e-8; //small value to avoid division by 0
   float d = distance(light_w.xyz, inData.pw.xyz);
   float atten = 1.0/(0.5 + 0.4*d + 0.1*d*d); //d-squared attenuation

   vec3 nw = normalize(inData.nw);			//world-space unit normal vector
   vec3 lw = normalize(light_w.xyz - inData.pw.xyz);	//world-space unit light vector
   vec4 diffuse_term = atten*kd*Ld*max(0.0, dot(nw, lw));

   vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	//world-space unit view vector
   vec3 rw = reflect(-lw, nw);	//world-space unit reflection vector

   vec4 specular_term = atten*ks*Ls*pow(max(0.0, dot(rw, vw)), shininess);

   fragcolor = ambient_term + (lit+0.05)*(diffuse_term + specular_term);
   //fragcolor = lit4;
   fragcolor = pow(fragcolor, vec4(1.0/2.2));

   if (u_debug_lit4 == 1) {
	   fragcolor = lit4;
   }

}

