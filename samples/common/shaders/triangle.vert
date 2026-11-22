#version 460
// The RGB triangle, kept undistorted in any window shape: clip space spans [-1, 1] on both axes whatever the window's
// aspect ratio, so x is scaled by height / width (a real application would use a projection matrix instead).
layout(push_constant) uniform PushConstants {
    float x_scale;
} pc;

layout(location = 0) out vec3 out_color;

const vec2 k_positions[3] = vec2[](vec2(0.0, 0.7), vec2(-0.7, -0.6), vec2(0.7, -0.6));
const vec3 k_colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

void main() {
    const vec2 position = k_positions[gl_VertexIndex];
    gl_Position = vec4(position.x * pc.x_scale, position.y, 0.0, 1.0);
    out_color = k_colors[gl_VertexIndex];
}
