#version 430
#define MAX_LIGHTS 1000

layout(location = 3) uniform int numLights;

out vec4 fragcolor;

in VertexData
{
   vec2 tex_coord;
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
} inData;   //block is named 'inData'

layout(binding = 0) uniform sampler2D gPositionTex;
layout(binding = 1) uniform sampler2D gNormalTex;
layout(binding = 2) uniform sampler2D gAlbedoSpecTex;

layout(std140, binding = 0) uniform SceneUniforms
{
    mat4 PV;
    vec4 eye_w;
};

layout(std140, binding = 1) uniform LightUniforms
{
    vec4 ambientColor;
    vec4 diffuseColors[MAX_LIGHTS];
    vec4 positions[MAX_LIGHTS];
    vec4 specularColors[MAX_LIGHTS];
    float radii[MAX_LIGHTS];
};

layout(std140, binding = 2) uniform MaterialUniforms
{
    vec4 ka;
    vec4 kd;
    vec4 ks;
    float shininess;
};



void main()
{
    vec3 FragPos = texture(gPositionTex, inData.tex_coord).rgb;
    vec3 Normal = normalize(texture(gNormalTex, inData.tex_coord).rgb);
    vec3 Albedo = texture(gAlbedoSpecTex, inData.tex_coord).rgb;
    float specularStrength = texture(gAlbedoSpecTex, inData.tex_coord).a;

    vec3 vw = normalize(eye_w.xyz - FragPos);

    vec4 ambient_term = ka * vec4(Albedo, 1.0) * ambientColor;
    vec4 diffuse_term = vec4(0.0);
    vec4 specular_term = vec4(0.0);

    const float eps = 1e-8;

    for (int i = 0; i < numLights; ++i)
    {
        vec3 lightVec = positions[i].xyz - FragPos;
        float d = length(lightVec);

        if (d < radii[i])
        {
            vec3 lw = normalize(lightVec);

            float atten = 1.0 / (d * d + eps);

            diffuse_term += atten * kd * vec4(Albedo, 1.0) * diffuseColors[i] * max(0.0, dot(Normal, lw));

            vec3 rw = reflect(-lw, Normal);

            specular_term += atten * ks * specularColors[i] * pow(max(0.0, dot(rw, vw)), shininess);
        }
    }

    fragcolor = ambient_term + diffuse_term + specular_term;
}
