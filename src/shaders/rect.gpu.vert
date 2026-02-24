#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 Color;

layout(set = 1, binding = 0) uniform UBO {
    mat4 projection;
} ubo;

void main()
{
    gl_Position = ubo.projection * vec4(aPos, 0.0, 1.0);
    Color = aColor;
}
