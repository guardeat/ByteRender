#version 410 core

layout (location = 0) out vec3 oNormal;
layout (location = 1) out vec3 oAlbedo;
layout (location = 2) out vec4 oMaterial;

in vec3 vNormal;
in vec2 vTexCoord;

uniform sampler2D uAlbedoTexture;
uniform sampler2D uMaterialTexture;

uniform int uMaterialMode;

uniform vec4 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;
uniform float uEmission;

void main()
{    
    oNormal = normalize(vNormal);

    switch(uMaterialMode) {
        case 0:
            oAlbedo = uAlbedo.rgb;
            oMaterial = vec4(uMetallic, uRoughness, uAO, uEmission);
            break;
        case 1:
            oAlbedo = texture(uAlbedoTexture, vTexCoord).rgb * uAlbedo.rgb;
            oMaterial = vec4(uMetallic, uRoughness, uAO, uEmission);
            break;
        case 2:
            oAlbedo = uAlbedo.rgb;
            oMaterial = texture(uMaterialTexture, vTexCoord);
            break;
        case 3:
            oAlbedo = texture(uAlbedoTexture, vTexCoord).rgb * uAlbedo.rgb;
            oMaterial = texture(uMaterialTexture, vTexCoord);
            break;
    }

}