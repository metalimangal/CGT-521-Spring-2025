#version 430
layout(binding = 0) uniform sampler2D diffuse_tex;
layout(location = 3)uniform bool isAsteroid;

in VertexData
{
   vec2 tex_coord;
   vec3 nw;   //world-space normal vector
   vec3 pw;    //world-space position
   vec3 color;
} inData;   

out vec4 fragcolor; //the output color for this fragment  

const vec3 lw = vec3(1.0, 0.0, 0.0f);  

void main(void)
{   
    vec3 nw = normalize(inData.nw);
    vec4 ktex = texture(diffuse_tex, inData.tex_coord);

    if(isAsteroid==false)
    {
        float light = 0.1 + max(0.0, dot(nw, lw));
        fragcolor = light*ktex;
        return;
    }

    float light = 0.1;
    //fake shadow
    if(dot(normalize(inData.pw), lw) > -0.75)
    {
        light += 0.75*max(0.0, dot(nw, lw));
    }
    fragcolor = light*texture(diffuse_tex, inData.tex_coord)*vec4(inData.color,1.0);

}

