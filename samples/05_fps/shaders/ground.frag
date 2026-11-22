#version 460
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 0) out vec4 out_color;

// 1 m checkerboard.
void main() {
    const ivec2 cell = ivec2(floor(in_world_position.xz));
    const bool dark = ((cell.x + cell.y) & 1) != 0;
    const vec3 albedo = dark ? vec3(0.18, 0.2, 0.22) : vec3(0.42, 0.45, 0.48);
    out_color = vec4(shade(albedo, vec3(0.0, 1.0, 0.0), in_world_position), 1.0);
}
