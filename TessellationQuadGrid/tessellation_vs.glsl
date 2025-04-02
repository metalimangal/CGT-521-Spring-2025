#version 430

layout(location=0) in vec3 pos_attrib;

int ngrid = 10;

void main(void)
{
	vec3 inst_offset = 2.0*vec3(gl_InstanceID/ngrid, gl_InstanceID%ngrid, 0.0);
	vec3 global_offset = vec3(-9.0/10.0, -9.0/10.0, 0.0);
	float scale = 1.0/ngrid;
	gl_Position = vec4((pos_attrib+inst_offset)*scale + global_offset, 1.0);
}