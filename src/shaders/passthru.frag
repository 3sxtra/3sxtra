#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform int u_filter_type; // 0 = standard, 1 = pixel art (sharp bilinear)
uniform vec4 SourceSize; // xy = width, height, zw = 1/width, 1/height
uniform sampler2D Source;

void main()
{
    if (u_filter_type == 1) {
        // Sharp Bilinear (Pixel Art)
        vec2 uv = TexCoord * SourceSize.xy;
        vec2 pixel = uv;
        vec2 seam = floor(uv + 0.5);
        vec2 dudv = fwidth(uv);
        uv = seam + clamp((uv - seam) / dudv, -0.5, 0.5);
        FragColor = texture(Source, uv * SourceSize.zw);
    } else {
        // Standard (Nearest or Linear based on sampler state)
        FragColor = texture(Source, TexCoord);
    }
}
