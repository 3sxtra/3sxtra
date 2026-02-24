#version 450
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aTexLayer;

layout (location = 0) out vec4 FgColor;
layout (location = 1) out vec2 TexCoord;
layout (location = 2) flat out float TexLayer;

layout(set = 1, binding = 0) uniform UBO {
    mat4 projection;
};

void main()
{
    gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
    FgColor = aColor;
    TexLayer = aTexLayer;
}
