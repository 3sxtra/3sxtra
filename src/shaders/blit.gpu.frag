#version 450
layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2D blitSampler;

void main()
{
    FragColor = texture(blitSampler, TexCoords);
}
