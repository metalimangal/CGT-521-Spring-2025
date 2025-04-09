#version 430

layout(triangles, fractional_odd_spacing, ccw) in;


void main()
{
    // Calculate the tessellated position of the fragment
    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;

    // Barycentric interpolation for the tessellated triangle
    vec3 p = (1.0 - gl_TessCoord.x - gl_TessCoord.y) * p0 + gl_TessCoord.x * p1 + gl_TessCoord.y * p2;

    gl_Position = vec4(p, 1.0);  // Set the final position
}
