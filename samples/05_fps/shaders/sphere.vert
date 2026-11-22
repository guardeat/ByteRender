#version 460
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"

// One instance per sphere: the unit sphere mesh scaled and moved by the instance data.
layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_color;

void main() {
    const vec3 unit = pc.mesh.positions[gl_VertexIndex];
    const SphereInstance sphere = pc.instances.spheres[gl_InstanceIndex];
    const vec3 world_position = sphere.position + unit * sphere.radius;
    out_world_position = world_position;
    out_normal = unit;
    out_color = sphere.color.rgb;
    gl_Position = pc.view_projection * vec4(world_position, 1.0);
}
