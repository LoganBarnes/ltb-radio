#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(binding = 0) uniform CameraBufferObject {
    mat4 view_from_world;
    mat4 clip_from_view;
    mat4 clip_from_world;
    mat4 world_from_clip;
} camera;

layout(push_constant) uniform ModelBufferObject {
    mat4 world_from_model;
} model;

layout(location = 0) out vec3 frag_color;

void main()
{
    gl_Position = camera.clip_from_world * model.world_from_model * vec4(in_position, 1.0);
    frag_color  = in_color;
}
