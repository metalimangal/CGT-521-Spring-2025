#version 430

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out VertexData
{
    vec2 tex_coord;
    vec3 pw;   // world-space position
    vec3 nw;   // world-space normal
} outData;

void main()
{
    outData.tex_coord = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}

