#version 430
layout(binding = 0) uniform sampler2D diffuse_tex; 
layout(location = 1) uniform float time;
layout(location = 2) uniform bool enableF;
layout(location = 3) uniform bool enableD;
layout(location = 4) uniform bool enableG;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
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
   float eta; // refractive index
   float roughness; // surface roughness (m in Cook-Torrance)
};



in VertexData
{
   vec2 tex_coord;
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
} inData;   //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

const float PI = 3.14159265359;
const float eps = 1e-8; //small value to avoid division by 0

// Schlick's approximation for Fresnel
vec4 FresnelSchlick(float cosTheta, vec4 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Cook-Torrance Distribution term (Beckmann)
float DistributionBeckmann(float NdotH, float m)
{
    float NdotH2 = NdotH * NdotH;
    float m2 = m * m;
    float tan2Alpha = (1.0 - NdotH2) / max(NdotH2 + eps, 0.001);
    float exponent = exp(-tan2Alpha / m2);
    return exponent / (4.0 * m2 * NdotH2 * NdotH2 + eps);
}

// Cook-Torrance Geometry term
float GeometryCookTorrance(float NdotV, float NdotL, float NdotH, float VdotH)
{
    float G1 = (2.0 * NdotH * NdotV) / VdotH + eps;
    float G2 = (2.0 * NdotH * NdotL) / VdotH + eps;
    return min(1.0, min(G1, G2));
}

void main(void)
{   
   //Compute per-fragment Phong lighting
   //vec4 ktex = texture(diffuse_tex, inData.tex_coord);
	
   vec4 ambient_term = ka*La;


   float d = distance(light_w.xyz, inData.pw.xyz);
   //float atten = 1.0/(d*d+eps); //d-squared attenuation

   vec3 nw = normalize(inData.nw);			//world-space unit normal vector
   vec3 lw = normalize(light_w.xyz - inData.pw.xyz);	//world-space unit light vector
   vec4 diffuse_term = /*atten * */ kd * Ld * max(0.0, dot(nw, lw));

   vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	//world-space unit view vector
   vec3 rw = reflect(-lw, nw);	//world-space unit reflection vector
   vec3 hw = normalize(lw + vw);

   float NdotL = max(dot(nw, lw), 0.0);
   float NdotV = max(dot(nw, vw), 0.0);
   float NdotH = max(dot(nw, hw), 0.0);
   float VdotH = max(dot(vw, hw), 0.001);


   float F0_scalar = pow((1.0 - eta) / (1.0 + eta), 2.0);
   vec4 F0 = vec4(vec3(F0_scalar), 1.0);
   vec4 fresnel = enableF ? FresnelSchlick(VdotH, F0) : vec4(1.0);

   float D = enableD ? DistributionBeckmann(NdotH, roughness):1.0;
   float G = enableG ? GeometryCookTorrance(NdotV, NdotL, NdotH, VdotH) : 1.0;

   vec4 specular_term = ks * Ls * (D * G * fresnel) / max(PI * NdotL * NdotV + eps, 0.001);

   fragcolor = ambient_term + diffuse_term + specular_term;
}

