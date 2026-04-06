#version 300 es
precision mediump float;

layout (location = 0) out vec4 FragColor;

in vec2 TexCoord;
in vec4 FgColor;

uniform sampler2D Source;

void main()
{
    FragColor = texture(Source, TexCoord) * FgColor;
    if (FragColor.a < 0.01) discard;
}
