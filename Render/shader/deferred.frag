#version 410 core

layout (location = 0) out vec3 oNormal;
layout (location = 1) out vec3 oAlbedo;
layout (location = 2) out vec4 oMaterial;

in vec3 vNormal;
in vec2 vTexCoord;

uniform vec4 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;
uniform float uEmission;

void main()
{    
    oNormal = normalize(vNormal);

    oAlbedo = uAlbedo.rgb;
    oMaterial = vec4(uMetallic, uRoughness, uAO, uEmission);
}