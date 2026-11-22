#version 460
#extension GL_GOOGLE_include_directive : require
#include "bytee/bindless.glsl"

layout(push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
    uvec2 vertices; // buffer device address (used by the vertex shader)
    uint texture_index;
    uint sampler_index;
} pc;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = in_color * bytee_sample_2d(pc.texture_index, pc.sampler_index, in_uv);
}
