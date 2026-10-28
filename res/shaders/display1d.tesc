#version 450

// specify number of control points per patch output
// this value controls the size of the input and output arrays
layout (vertices=2) out;

// varying input from vertex shader
layout(location = 0) in VertexData {
    float prev_fluid_value;
    float next_fluid_value;
    int   fluid_index;
} tesc_in[];

// varying output to evaluation shader
layout(location = 0) out VertexData {
    float prev_fluid_value;
    float next_fluid_value;
    int   fluid_index;
} tesc_out[];

void main()
{
    // Pass attributes through
    tesc_out[gl_InvocationID].prev_fluid_value = tesc_in[gl_InvocationID].prev_fluid_value;
    tesc_out[gl_InvocationID].next_fluid_value = tesc_in[gl_InvocationID].next_fluid_value;
    tesc_out[gl_InvocationID].fluid_index      = tesc_in[gl_InvocationID].fluid_index;

    // Set tess levels:

    // The number of divisions along outer edges. No
    // divisions are needed so we set everything to 1
    gl_TessLevelOuter[0] = 1;
    gl_TessLevelOuter[1] = 1;
    gl_TessLevelOuter[2] = 1;
    gl_TessLevelOuter[3] = 1;

    // The number of internal divisions. No divisions
    // are needed so we set everything to 1
    gl_TessLevelInner[0] = 1;
    gl_TessLevelInner[1] = 1;

    // gl_TessLevelInner is not used for isolines
}
