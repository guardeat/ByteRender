#version 460
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"

// A large quad at y = 0 around the camera (two triangles from gl_VertexIndex, no vertex buffer). Following the camera
// in 1 m steps keeps the checkerboard fixed in the world while the ground never ends.
const float k_half_size = 400.0;
const vec2 k_corners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));

layout(location = 0) out vec3 out_world_position;

void main() {
    const vec2 center = floor(pc.camera_position.xz);
    const vec2 xz = center + k_corners[gl_VertexIndex] * k_half_size;
    out_world_position = vec3(xz.x, 0.0, xz.y);
    gl_Position = pc.view_projection * vec4(out_world_position, 1.0);
}
