#version 300 es
precision mediump float;

layout (location = 0) out vec4 FragColor;
uniform vec4 rectColor;
void main()
{
    FragColor = rectColor;
}
