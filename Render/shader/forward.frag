#version 410 core

uniform vec4 uColor;

uniform struct {
    vec3 direction;
    vec3 color;
    float intensity;
} uDLight;

in vec3 vNormal;
in vec2 vTexCoord;

out vec4 vFragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDLight.direction);
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = uDLight.color * uDLight.intensity * NdotL;
    vec3 ambient = uDLight.color * uDLight.intensity * 0.3;
    vec3 finalColor = uColor.rgb * (diffuse + ambient);
    vFragColor = vec4(finalColor, uColor.a);
}