#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 16: aggregation
// Original: Copyright (C) 2017-2021 igv (LGPL v3.0+)

layout(push_constant) uniform Push
{
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 SUBCONV1Size;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D SUBCONV1;
layout(set = 0, binding = 3) uniform sampler2D Original;


// BT.601 luma coefficients (must match feature extraction passes)
const vec3 Y_coeff = vec3(0.299, 0.587, 0.114);

void main()
{
    // Pixel shuffle: extract CNN-upscaled luma from sub-pixel conv output
    vec2 fcoord = fract(vTexCoord * global.SUBCONV1Size.xy);
    vec2 base = vTexCoord + (vec2(0.5) - fcoord) * global.SUBCONV1Size.zw;
    ivec2 index = ivec2(fcoord * vec2(2));
    vec4 res = texture(SUBCONV1, base);
    float new_luma = res[index.x * 2 + index.y];

    // Get bilinearly upscaled original RGB and its luma
    vec3 orig_rgb = texture(Original, vTexCoord).rgb;
    float old_luma = dot(orig_rgb, Y_coeff);

    // Additive detail: apply CNN edge enhancement
    float detail = new_luma - old_luma;
    vec3 final_rgb = orig_rgb + vec3(detail);

    FragColor = vec4(clamp(final_rgb, 0.0, 1.0), 1.0);
}