#version 330 core
out vec4 FragColor;

in vec4 FgColor;
in vec2 TexCoord;
flat in float TexLayer;
flat in float PaletteIndex;

uniform usampler2DArray Source; // Raw indices (R8UI)
uniform samplerBuffer PaletteBuffer; // RGBA float colors
uniform sampler2DArray SourceRGBA; // Direct-color RGBA8

void main()
{
    if (TexLayer >= 0.0) {
        // Indexed path — palette lookup
        uint index = texture(Source, vec3(TexCoord, TexLayer)).r;
        int offset = int(PaletteIndex) * 256 + int(index);
        FragColor = texelFetch(PaletteBuffer, offset) * FgColor;
    } else {
        // Direct-color path — RGBA texture array
        float layer = -TexLayer - 2.0;
        FragColor = texture(SourceRGBA, vec3(TexCoord, layer)) * FgColor;
    }
}
