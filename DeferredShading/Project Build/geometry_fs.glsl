#version 430

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;

layout(binding = 0) uniform sampler2D diffuse_tex;

in VertexData
{
    vec2 tex_coord;
    vec3 pw;
    vec3 nw;
} inData;

void main()
{
    gPosition = vec4(inData.pw, 1.0);
    gNormal = vec4(normalize(inData.nw), 1.0);

    vec4 ktex = texture(diffuse_tex, inData.tex_coord);

    gAlbedoSpec.rgb = ktex.rgb;    // Store diffuse texture color
    gAlbedoSpec.a = 1.0;            // Later you can store specular strength here if needed
}
