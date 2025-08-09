#version 410 core

in vec2 vTexCoord;

out vec4 oFragColor;

uniform sampler2D uColor;
uniform sampler2D uDepth;

uniform float uGamma;
uniform float uNear;
uniform float uFar;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;

float linearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0; 
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float bayerDither(vec2 pos)
{
    int x = int(mod(pos.x, 4.0));
    int y = int(mod(pos.y, 4.0));
    const int dither[16] = int[16](
        0,  8,  2, 10,
        12, 4, 14,  6,
        3, 11,  1,  9,
        15, 7, 13,  5
    );
    return float(dither[y * 4 + x]) / 16.0;
}

void main() {
    vec3 color = texture(uColor, vTexCoord).rgb;
    vec3 mapped = color / (color + vec3(1.0));
    vec3 gammaCorrected = pow(mapped, vec3(1.0 / uGamma));

    float depthSample = texture(uDepth, vTexCoord).r;

    if (depthSample == 1.0) {
        float noise = bayerDither(gl_FragCoord.xy);
        gammaCorrected += (noise - 0.5) / 255.0;
        oFragColor = vec4(gammaCorrected, 1.0);
        return;
    }

    float depth = linearizeDepth(depthSample, uNear, uFar);
    float fogFactor = clamp((uFogFar - depth) / (uFogFar - uFogNear), 0.0, 1.0);

    vec3 finalColor = mix(uFogColor, gammaCorrected, fogFactor);

    float noise = bayerDither(gl_FragCoord.xy);
    finalColor += (noise - 0.5) / 255.0;

    oFragColor = vec4(finalColor, 1.0);
}
