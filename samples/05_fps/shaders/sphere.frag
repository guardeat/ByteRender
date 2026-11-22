#version 460
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_color;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(shade(in_color, normalize(in_normal), in_world_position), 1.0);
}
