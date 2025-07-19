#version 410 core

uniform vec4 uColor;

out vec4 vFragColor;

in vec3 vNormal;
in vec2 vTexCoord;

void main() {
    vFragColor = uColor;
}