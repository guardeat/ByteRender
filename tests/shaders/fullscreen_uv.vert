#version 460
// Full-screen triangle with texture coordinates: uv (0, 0) at the top-left of the target, (1, 1) at the bottom-right.
layout(location = 0) out vec2 out_uv;

void main() {
    const vec2 position = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    gl_Position = vec4(position, 0.0, 1.0);
    out_uv = vec2(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5); // clip space is Y-up, uv is Y-down
}
