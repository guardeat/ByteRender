// Shared by the 05_fps shaders: push constants, sphere data and lighting.
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct SphereInstance {
    vec3 position;
    float radius;
    vec4 color; // linear RGB
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer MeshPositions {
    vec3 positions[]; // unit sphere: positions are also normals
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Spheres {
    SphereInstance spheres[];
};

layout(push_constant) uniform PushConstants {
    mat4 view_projection;
    vec4 camera_position; // xyz
    vec4 fog_color;       // linear RGB: the sky (clear color)
    MeshPositions mesh;
    Spheres instances;
} pc;

const vec3 k_sun_direction = normalize(vec3(0.4, 1.0, 0.3));

// Sun (Lambert) plus sky ambient, faded into the sky with distance.
vec3 shade(vec3 albedo, vec3 normal, vec3 world_position) {
    const float sun = max(dot(normal, k_sun_direction), 0.0);
    const float sky = 0.5 + 0.5 * normal.y;
    const vec3 lit = albedo * (sun * vec3(1.0, 0.95, 0.85) + sky * 0.35 * pc.fog_color.rgb);
    const float distance_to_camera = length(world_position - pc.camera_position.xyz);
    const float fog = 1.0 - exp(-distance_to_camera * 0.015);
    return mix(lit, pc.fog_color.rgb, fog);
}
