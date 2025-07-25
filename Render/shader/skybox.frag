#version 410 core

uniform struct {
    vec3 direction;
    vec3 color;
    float intensity;
} uDLight;

out vec4 vFragColor;

in vec3 vRotatedDir;

void main() {
    vec3 viewDir = normalize(vRotatedDir);
    
    float height = clamp(viewDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 colorBelow = vec3(0.0, 0.0, 0.2);     
    vec3 colorMiddle = vec3(0.3, 0.4, 0.6);   
    vec3 colorAbove = vec3(0.1, 0.4, 0.9);   
    
    vec3 baseColor = mix(colorBelow, colorMiddle, smoothstep(0.0, 0.4, height));
    baseColor = mix(baseColor, colorAbove, smoothstep(0.3, 1.0, height));
    
    vec3 lightDir = normalize(-uDLight.direction);
    float intensity = pow(max(dot(viewDir, lightDir), 0.0), 64.0) * uDLight.intensity;
    
    vec3 sunColor = vec3(1.0, 0.8, 0.4) * uDLight.color;
    vec3 centerColor = vec3(1.0, 0.9, 0.7) * uDLight.color;
    vec3 highlight = mix(sunColor, centerColor, smoothstep(0.9, 0.95, intensity));

    float timeIntensity = clamp(dot(vec3(0, 1, 0), lightDir), 0.2, 1.0);
    baseColor *= uDLight.intensity * timeIntensity;
    
    if (intensity > 0.9) {
        baseColor = highlight;
    } else {
        baseColor = mix(baseColor, sunColor * intensity, intensity);
    }
    
    vFragColor = vec4(baseColor,1.0);
}