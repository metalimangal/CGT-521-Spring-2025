#version 440

layout(location = 1) uniform float time;

out vec4 fragcolor; //the output color for this fragment    

in vec3 pos_out;

uniform vec4 particle_color_rgb = vec4(0.75, 0.2, 0.1, 0.1);

void main(void)
{   
    vec2 p = gl_PointCoord * 2.0 - 1.0;

    float dist = length(p);

    if (dist < 0.5) {
        discard;
    }

	fragcolor = vec4(particle_color_rgb.r * abs(sin(pos_out.y)), particle_color_rgb.g * abs(cos(pos_out.x)), particle_color_rgb.b * abs(sin(pos_out.z)), 0.1);
}

