#version 460
// The classic RGB triangle, counter-clockwise in ByteRender's Y-up clip space. No vertex buffers: positions and colors
// are computed from the vertex index (vertex pulling).
layout(location = 0) out vec3 out_color;

const vec2 k_positions[3] = vec2[](vec2(0.0, 0.7), vec2(-0.7, -0.6), vec2(0.7, -0.6));
const vec3 k_colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

void main() {
    gl_Position = vec4(k_positions[gl_VertexIndex], 0.0, 1.0);
    out_color = k_colors[gl_VertexIndex];
}
