#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// Dear ImGui vertices (ImDrawVert: position, uv, packed RGBA8 color), pulled through a buffer device address.
struct ImGuiVertex {
    vec2 position; // ImGui display coordinates (points, top-left origin)
    vec2 uv;
    uint color;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices {
    ImGuiVertex vertices[];
};

layout(push_constant) uniform PushConstants {
    vec2 scale;     // display coordinates -> Y-up clip space
    vec2 translate;
    Vertices vertices;
    uint texture_index;
    uint sampler_index;
} pc;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

// ImGui colors are sRGB; the target applies the sRGB encoding on write, so shaders work with linear values.
vec3 srgb_to_linear(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), greaterThan(color, vec3(0.04045)));
}

void main() {
    const ImGuiVertex vertex = pc.vertices.vertices[gl_VertexIndex];
    const vec4 color = unpackUnorm4x8(vertex.color);
    out_color = vec4(srgb_to_linear(color.rgb), color.a);
    out_uv = vertex.uv;
    gl_Position = vec4(vertex.position * pc.scale + pc.translate, 0.0, 1.0);
}
