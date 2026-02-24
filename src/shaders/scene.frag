#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 FgColor;

uniform sampler2D Source;

void main()
{
    FragColor = texture(Source, TexCoord) * FgColor;
}
