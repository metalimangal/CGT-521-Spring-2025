#version 430
#define MAX_LIGHTS 1000
layout(binding = 0) uniform sampler2D diffuse_tex; 
layout(location = 1) uniform float time;
layout(location = 2) uniform bool invertedNormals;
layout(location = 3) uniform int numLights;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

layout(std140, binding = 1) uniform LightUniforms{

   vec4 ambientColor;
   vec4 diffuseColors[MAX_LIGHTS];
   vec4 positions[MAX_LIGHTS];

   vec4 specularColors[MAX_LIGHTS];
   float radii[MAX_LIGHTS];

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
} inData;   //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

void main(void)
{
    vec4 ktex = texture(diffuse_tex, inData.tex_coord);
    vec3 nw = normalize(inData.nw);

    vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);

    vec4 ambient_term = ka * ktex * ambientColor;
    vec4 diffuse_term = vec4(0.0);
    vec4 specular_term = vec4(0.0);

    const float eps = 1e-8;

    for (int i = 0; i < numLights; ++i)
    {
        vec3 lightVec = positions[i].xyz - inData.pw;

        float d = distance(positions[i].xyz, inData.pw.xyz);

        if (d < radii[i])
        {
            vec3 lw = normalize(positions[i].xyz - inData.pw.xyz);

            float atten = 1.0 / (d * d + eps);
            diffuse_term += atten * kd * ktex * diffuseColors[i] * max(0.0, dot(nw, lw));

            vec3 rw = reflect(-lw, nw);

            specular_term += atten* ks* specularColors[i] * pow(max(0.0, dot(rw, vw)), shininess);
        }
    }


    //fragcolor = diffuseColors[0];
    fragcolor = ambient_term + diffuse_term + specular_term;
}


