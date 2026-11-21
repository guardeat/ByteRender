#version 460
// A counter-clockwise triangle in Bytee's Y-up clip space, apex at the top.
layout(push_constant) uniform PushConstants {
    vec4 color;
    vec2 offset;
    float depth;
    uint reverse_winding; // 1: emit the vertices in clockwise order
} pc;

const vec2 k_positions[3] = vec2[](vec2(0.0, 0.5), vec2(-0.5, -0.5), vec2(0.5, -0.5));

void main() {
    const uint index = pc.reverse_winding != 0u ? 2u - uint(gl_VertexIndex) : uint(gl_VertexIndex);
    gl_Position = vec4(k_positions[index] + pc.offset, pc.depth, 1.0);
}
