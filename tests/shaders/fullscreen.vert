#version 460
// One triangle covering the whole target, at a depth from push constants.
layout(push_constant) uniform PushConstants {
    vec4 color;
    vec2 offset;
    float depth;
    uint reverse_winding;
} pc;

void main() {
    const vec2 position = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    gl_Position = vec4(position, pc.depth, 1.0);
}
