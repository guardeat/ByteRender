// Bytee bindless resources (architecture §7). Include with:
//   #extension GL_GOOGLE_include_directive : require
//   #include "bytee/bindless.glsl"
// Indices come from rhi::bindless_index(texture / sampler), usually through push constants.
#ifndef BYTEE_BINDLESS_GLSL
#define BYTEE_BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : require

// One descriptor set per resource kind, each with a single array at binding 0:
//   set 0: sampled textures, set 1: storage textures (declared by compute shaders, roadmap step 13), set 2: samplers.
// Sampled textures have one declaration per view type: use the one matching the texture's dimension (2D, 2D array
// for textures with several layers, 3D).
layout(set = 0, binding = 0) uniform texture2D bytee_textures_2d[];
layout(set = 0, binding = 0) uniform texture2DArray bytee_textures_2d_array[];
layout(set = 0, binding = 0) uniform texture3D bytee_textures_3d[];
layout(set = 2, binding = 0) uniform sampler bytee_samplers[];
// The same samplers declared for depth comparison: use with samplers created with SamplerDesc::compare (shadow maps).
layout(set = 2, binding = 0) uniform samplerShadow bytee_samplers_shadow[];

vec4 bytee_sample_2d(uint texture_index, uint sampler_index, vec2 uv) {
    return texture(sampler2D(bytee_textures_2d[nonuniformEXT(texture_index)], bytee_samplers[nonuniformEXT(sampler_index)]),
                   uv);
}

vec4 bytee_sample_2d_lod(uint texture_index, uint sampler_index, vec2 uv, float lod) {
    return textureLod(
        sampler2D(bytee_textures_2d[nonuniformEXT(texture_index)], bytee_samplers[nonuniformEXT(sampler_index)]), uv,
        lod);
}

vec4 bytee_sample_2d_array(uint texture_index, uint sampler_index, vec3 uv_layer) {
    return texture(
        sampler2DArray(bytee_textures_2d_array[nonuniformEXT(texture_index)], bytee_samplers[nonuniformEXT(sampler_index)]),
        uv_layer);
}

// Depth comparison (shadow maps): returns the filtered comparison result in [0, 1]. `uv_reference.z` is the
// reference depth. Needs a sampler created with SamplerDesc::compare.
float bytee_sample_2d_shadow(uint texture_index, uint sampler_index, vec3 uv_reference) {
    return texture(sampler2DShadow(bytee_textures_2d[nonuniformEXT(texture_index)],
                                   bytee_samplers_shadow[nonuniformEXT(sampler_index)]),
                   uv_reference);
}

vec4 bytee_sample_3d(uint texture_index, uint sampler_index, vec3 uvw) {
    return texture(sampler3D(bytee_textures_3d[nonuniformEXT(texture_index)], bytee_samplers[nonuniformEXT(sampler_index)]),
                   uvw);
}

#endif
