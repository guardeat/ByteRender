#version 410 core

layout (location = 0) in vec3 aPos;

layout (location = 3) in vec3 aPosition;
layout (location = 4) in vec3 aScale;
layout (location = 5) in vec3 aColor;
layout (location = 6) in vec3 aAttenuation;

flat out vec3 vPosition;
flat out vec3 vColor;
flat out vec3 vAttenuation;

uniform mat4 uProjection;
uniform mat4 uView;

vec3 translateVertex(vec3 point, vec3 translation) {
    return point + translation;
}

vec3 scaleVertex(vec3 point, vec3 scaleFactor) {
    return point * scaleFactor;
}

vec3 translate(vec3 aPos, vec3 position, vec3 scale) {
    vec3 translatedPos = scaleVertex(aPos, scale);

    translatedPos = translateVertex(translatedPos, position);

    return translatedPos;
}

void main() {
    vec3 translated = translate(aPos,aPosition,aScale);
	gl_Position = uProjection * uView * vec4(translated.xyz, 1.0);

    vPosition = aPosition;
    vColor = aColor;
    vAttenuation = aAttenuation;
}