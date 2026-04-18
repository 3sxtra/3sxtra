#version 300 es
precision mediump float;

in vec4 vColor;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
