#version 460
#extension GL_GOOGLE_include_directive : require
#include "bytee/bindless.glsl"

layout(push_constant) uniform PushConstants {
    uint texture_index;
    uint sampler_index;
    float lod;
    uint use_lod;
} pc;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = pc.use_lod != 0u ? bytee_sample_2d_lod(pc.texture_index, pc.sampler_index, in_uv, pc.lod)
                                 : bytee_sample_2d(pc.texture_index, pc.sampler_index, in_uv);
}
