#version 300 es
precision highp float;

in vec4 vColor;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    FragColor = vColor;
    if (FragColor.a == 0.0) {
        discard;
    }
}
