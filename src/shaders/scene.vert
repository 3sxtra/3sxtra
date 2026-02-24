#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aTexLayer;
layout (location = 4) in float aPaletteIndex;

out vec4 FgColor;
out vec2 TexCoord;
flat out float TexLayer;
flat out float PaletteIndex;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
    FgColor = aColor;
    TexLayer = aTexLayer;
    PaletteIndex = aPaletteIndex;
}
