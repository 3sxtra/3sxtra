#version 300 es
precision mediump float;

layout (location = 0) out vec4 FragColor;

in vec4 FgColor;
in vec2 TexCoord;
flat in float TexLayer;
flat in float PaletteIndex;

uniform highp usampler2DArray Source;    // Raw indices (R8UI)
uniform sampler2D PaletteBuffer;         // RGBA float colors — 2D texture (256 x FL_PALETTE_MAX)
uniform sampler2DArray SourceRGBA;       // Direct-color RGBA8

void main()
{
    if (TexLayer >= 0.0) {
        // Indexed path — palette lookup via 2D palette texture
        uint index = texture(Source, vec3(TexCoord, TexLayer)).r;
        // PaletteBuffer is a 256-wide, FL_PALETTE_MAX-tall 2D texture.
        // Each row is one palette slot; each column is a color index.
        float u = (float(index) + 0.5) / 256.0;
        float v = (PaletteIndex + 0.5) / float(textureSize(PaletteBuffer, 0).y);
        FragColor = texture(PaletteBuffer, vec2(u, v)) * FgColor;
    } else {
        // Direct-color path — RGBA texture array
        float layer = -TexLayer - 2.0;
        FragColor = texture(SourceRGBA, vec3(TexCoord, layer)) * FgColor;
    }

    if (FragColor.a < 0.01) {
        discard;
    }
}
