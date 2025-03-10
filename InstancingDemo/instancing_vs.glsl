#version 430  

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;
layout(location = 3)uniform bool isAsteroid;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

layout(location = 0) in vec3 pos_attrib; //this variable holds the position of mesh vertices
layout(location = 1) in vec2 tex_coord_attrib;
layout(location = 2) in vec3 normal_attrib;  

out VertexData
{
   vec2 tex_coord;
   vec3 nw;    //world-space normal vector
   vec3 pw;    //world-space position
   vec3 color;
} outData; 

mat4 InstanceMatrix();
float rand(vec2 co);
float rand(float co);

void main(void)
{
    mat4 Mi = InstanceMatrix();

    outData.pw = vec3(Mi*vec4(pos_attrib, 1.0));
    gl_Position = PV*vec4(outData.pw, 1.0); //transform vertices and send result into pipeline

    outData.tex_coord = tex_coord_attrib;           //send tex_coord to fragment shader
    outData.nw = vec3(Mi*vec4(normal_attrib, 0.0));	//world-space normal vector
    outData.color = vec3(0.25+0.75*rand(float(gl_InstanceID)));
}

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float rand(float co)
{
    return fract(sin(dot(co, 78.233)) * 43758.5453);
}

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

vec3 random_axis(float inst)
{
   vec3 a;
   a.x = rand(inst)-0.5;
   a.y = rand(-inst)-0.5;
   a.z = rand(-0.8*inst)-0.5;
   return a;
}

mat4 InstanceMatrix()
{
   float inst = float(gl_InstanceID);
   
   //orbit about planet (revolve about z axis)
   float angle = 0.02*time + 0.08*inst;
   mat4 R0 = mat4(cos(angle), -sin(angle), 0.0, 0.0,
                  sin(angle), cos(angle), 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0, 
                  0.0, 0.0, 0.0, 1.0);

   if(isAsteroid==false) return R0*M;

   mat4 T = mat4(1.0);
   T[3][0] = -0.5+1.0*rand(0.4*inst);   //ring width
   T[3][2] = 0.1*(0.5-rand(0.7*inst)); //ring thickness

   //tumbling
   angle = rand(0.9*inst)*time;
   vec3 axis = random_axis(inst);
   mat4 R1 = rotationMatrix(axis, angle);

   //scale
   float scale = 0.02 + 0.5*rand(inst);
   mat4 S = mat4(scale);
   S[3][3] = 1.0;

   return R0*T*M*R1*S;
}