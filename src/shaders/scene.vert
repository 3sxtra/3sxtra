#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aTexLayer;
layout (location = 4) in float aPaletteIndex;
layout (location = 5) in float aZ;

out vec4 FgColor;
out vec2 TexCoord;
flat out float TexLayer;
flat out float PaletteIndex;

uniform mat4 projection;

void main()
{
    // ⚡ Bolt: Native Z comes in as [0.0, 65535.0], where 0=BACK and 65535=FRONT.
    // OpenGL NDC depth expects [-1.0, 1.0], mapping to gl_FragCoord.z [0.0, 1.0].
    // By doing this, gl_FragCoord.z becomes precisely (aZ / 65535.0).
    float ndc_z = (aZ / 65535.0) * 2.0 - 1.0;
    
    gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);
    gl_Position.z = ndc_z;
    
    TexCoord = aTexCoord;
    FgColor = aColor;
    TexLayer = aTexLayer;
    PaletteIndex = aPaletteIndex;
}
