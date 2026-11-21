#version 460
// Vertex pulling: positions are read from a buffer through its device address (bindless buffer access).
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430, buffer_reference_align = 8) readonly buffer Positions {
    vec2 values[];
};

layout(push_constant) uniform PushConstants {
    vec4 color;
    Positions positions;
} pc;

void main() {
    gl_Position = vec4(pc.positions.values[gl_VertexIndex], 0.0, 1.0);
}
