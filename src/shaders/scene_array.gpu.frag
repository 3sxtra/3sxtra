#version 450
layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec4 FgColor;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) flat in float TexLayer;

layout(set = 2, binding = 0) uniform sampler2DArray Source;

void main()
{
    FragColor = texture(Source, vec3(TexCoord, TexLayer)) * FgColor;
}
