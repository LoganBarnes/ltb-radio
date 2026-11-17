#version 450

layout(location = 0) in vec2 in_position;

layout(push_constant) uniform PlotPushConstants {
    mat4 clip_from_world;
    vec4 color;
} plot;

layout(location = 0) out vec4 frag_color;

void main()
{
    gl_Position = plot.clip_from_world * vec4(in_position, 0.0, 1.0);
    frag_color  = plot.color;
}
