#version 410 core

uniform struct {
    vec3 direction;
    vec3 color;
    float intensity;
} uDLight;

uniform vec3 uScatter;

out vec4 vFragColor;

in vec3 vRotatedDir;

void main() {
    vec3 backColor = uScatter * uDLight.color;

    vec3 lightDir = normalize(-uDLight.direction);
    vec3 viewDir = normalize(vRotatedDir);

    vec3 baseColor = backColor;
    
    float intensity = pow(max(dot(viewDir, lightDir), 0.0), 128.0) * uDLight.intensity;

    vec3 outerColor = uDLight.color * (vec3(1.0) - uScatter);
    vec3 sunColor = outerColor / 0.7f;
    vec3 highlight = mix(outerColor, sunColor, smoothstep(0.1, 0.90, intensity));

    float timeIntensity = clamp(pow(dot(vec3(0, 1, 0), lightDir),0.5), 0.01, 1.0);
    baseColor *= uDLight.intensity * timeIntensity;
    
    if (intensity > 0.9) {
        baseColor = highlight;
    } else {
        
        baseColor = mix(baseColor, sunColor * intensity, intensity);
    }
    
    vFragColor = vec4(baseColor, 1.0);
}