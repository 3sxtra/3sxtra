#version 450
layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec4 Color;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2D textSampler;

void main()
{
    float alpha = texture(textSampler, TexCoords).r;
    FragColor = vec4(Color.rgb, Color.a * alpha);
}
