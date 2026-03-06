#version 450
layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec4 FgColor;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) flat in float TexLayer;
layout (location = 3) flat in float PaletteIdx;

// Single RGBA8 texture array (indexed + direct color)
layout(set = 2, binding = 0) uniform sampler2DArray TexArray;

// Palette atlas: 256 wide × 1088 tall, each row is a 256-color palette
layout(set = 2, binding = 1) uniform sampler2D PaletteTex;

void main()
{
    if (TexLayer < 0.0) {
        // Untextured solid color quad
        FragColor = FgColor;
    } else if (PaletteIdx >= 0.0) {
        // Indexed texture: R channel holds the raw palette index (0.0..1.0 = 0..255)
        float idx = texture(TexArray, vec3(TexCoord, TexLayer)).r;
        // Map index → palette atlas UV
        //   U: (idx * 255 + 0.5) / 256  — texel center for exact palette color
        //   V: (paletteRow + 0.5) / 1089 — texel center for the palette row
        vec2 palUV = vec2((idx * 255.0 + 0.5) / 256.0,
                          (PaletteIdx + 0.5) / 1089.0);
        FragColor = texture(PaletteTex, palUV) * FgColor;
    } else {
        // Direct color (PSMCT16, 32-bit): sample from same RGBA8 array
        FragColor = texture(TexArray, vec3(TexCoord, TexLayer)) * FgColor;
    }
}
