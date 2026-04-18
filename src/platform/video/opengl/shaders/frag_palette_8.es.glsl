#version 300 es
precision mediump float;

in vec4 vColor;
in vec2 vTexCoord;
out vec4 FragColor;

/* On GLES, palettes are stored as Nx1 2D textures instead of 1D textures.
 * texelFetch uses ivec2(index, 0) instead of int(index). */
uniform sampler2D uPalette;
uniform highp usampler2D uIndexTex;

void main() {
    uint index = texture(uIndexTex, vTexCoord).r;
    vec4 color = texelFetch(uPalette, ivec2(int(index), 0), 0);
    FragColor = color * vColor;
}
