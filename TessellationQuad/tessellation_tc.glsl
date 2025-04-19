#version 430

// Define the uniform array for the tessellation levels (4 values: 2 outer, 2 inner)
layout(location = 0) uniform float slider[4] = float[](1.0, 1.0, 1.0, 1.0);  // 4 values for tessellation levels

// Set the number of output vertices for a triangle patch (3 vertices)
layout(vertices = 3) out;  // 3 vertices for a triangle patch

void main()
{
    // Pass-through: just copy input vertices to output
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    // Set tessellation levels for the outer triangle edges (3 edges)
    gl_TessLevelOuter[0] = slider[0]; // Outer tessellation level 0 (edge 0)
    gl_TessLevelOuter[1] = slider[1]; // Outer tessellation level 1 (edge 1)
    gl_TessLevelOuter[2] = slider[2]; // Outer tessellation level 2 (edge 2)

    // Set tessellation level for the inner triangle
    gl_TessLevelInner[0] = slider[3]; // Inner tessellation level (single value for inner triangle)
}
