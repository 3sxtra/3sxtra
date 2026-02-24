#version 330 core
out vec4 FragColor;

in vec4 FgColor;
in vec2 TexCoord;
flat in float TexLayer;
flat in float PaletteIndex;

uniform usampler2DArray Source; // Raw indices (R8UI)
uniform samplerBuffer PaletteBuffer; // RGBA float colors

void main()
{
    // Read raw index (0-255)
    uint index = texture(Source, vec3(TexCoord, TexLayer)).r;

    // Calculate global palette offset
    // Each palette is 256 colors. PaletteIndex is the slot index.
    int offset = int(PaletteIndex) * 256 + int(index);

    // Fetch color
    vec4 palColor = texelFetch(PaletteBuffer, offset);

    FragColor = palColor * FgColor;
}
