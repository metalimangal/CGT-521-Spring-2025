#version 430

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;
layout(location = 2) uniform bool invertedNormals;
layout(location = 3) uniform int numLights;
layout(std140, binding = 0) uniform SceneUniforms
{
    mat4 PV;
    vec4 eye_w;
};

layout(location = 0) in vec3 pos_attrib;
layout(location = 1) in vec2 tex_coord_attrib;
layout(location = 2) in vec3 normal_attrib;

out VertexData
{
    vec2 tex_coord;
    vec3 pw;   // world-space position
    vec3 nw;   // world-space normal
} outData;

void main()
{
    gl_Position = PV * M * vec4(pos_attrib, 1.0);
    outData.tex_coord = tex_coord_attrib;
    outData.pw = vec3(M * vec4(pos_attrib, 1.0));
    outData.nw = mat3(transpose(inverse(M))) * normal_attrib * (invertedNormals ? -1 : 1);
}
