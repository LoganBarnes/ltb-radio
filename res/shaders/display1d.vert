#version 450

layout(location = 0) in float prev_fluid_value_in;
layout(location = 1) in float next_fluid_value_in;

layout(location = 0) out VertexData {
    float prev_fluid_value;
    float next_fluid_value;
    int   fluid_index;
} vs_out;

void main()
{
    vs_out.prev_fluid_value = prev_fluid_value_in;
    vs_out.next_fluid_value = next_fluid_value_in;
    vs_out.fluid_index      = gl_VertexIndex;
}
