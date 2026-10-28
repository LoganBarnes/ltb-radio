#version 450

layout(quads, equal_spacing, ccw) in;

layout(location = 0) in VertexData {
    float prev_fluid_value;
    float next_fluid_value;
    int   fluid_index;
} tese_in[];

layout(binding = 0) uniform CameraParams
{
    mat4 clip_from_world;
} camera;

layout(push_constant) uniform SimulationParams
{
    float frame_interpolant;
    float cell_size;
    uint  fluid_cell_count;
    float line_thickness;
} sim;

void main()
{
    float x_cell_center = float(tese_in[0].fluid_index) * sim.cell_size;
    float x_offset      = gl_TessCoord.x * sim.cell_size;

    float y_coord       = gl_TessCoord.y - 0.5F;
    float y_offset      = y_coord * sim.line_thickness;

    float fluid_value = mix(
    tese_in[0].prev_fluid_value,
    tese_in[0].next_fluid_value,
    sim.frame_interpolant
    );

    float x_pos = x_cell_center + x_offset;
    float y_pos = -fluid_value + y_offset;

    gl_Position = camera.clip_from_world * vec4(x_pos, y_pos, 0.0F, 1.0F);
}
