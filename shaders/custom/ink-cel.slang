#version 450

// ink-cel.slang — Ink Outline + Cel Shading
// Sobel at native res for outlines + color quantization.
// Designed to enhance large sprite edges (characters, stage elements)
// without overwhelming small details (icons, UI, text).

#pragma name InkCel
#pragma parameter INK_STRENGTH "Ink Outline Strength" 0.45 0.0 3.0 0.05
#pragma parameter INK_THICKNESS "Ink Edge Sensitivity" 0.12 0.05 2.0 0.01
#pragma parameter INK_COLOR_R "Ink Color R" 0.03 0.0 1.0 0.01
#pragma parameter INK_COLOR_G "Ink Color G" 0.03 0.0 1.0 0.01
#pragma parameter INK_COLOR_B "Ink Color B" 0.06 0.0 1.0 0.01
#pragma parameter CEL_LEVELS "Cel Shading Color Levels" 16.0 2.0 32.0 1.0
#pragma parameter CEL_STRENGTH "Cel Shading Strength" 0.25 0.0 1.0 0.05
#pragma parameter SATURATION_BOOST "Saturation Boost" 1.05 0.5 2.0 0.05

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float INK_STRENGTH;
    float INK_THICKNESS;
    float INK_COLOR_R;
    float INK_COLOR_G;
    float INK_COLOR_B;
    float CEL_LEVELS;
    float CEL_STRENGTH;
    float SATURATION_BOOST;
} params;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 saturate_color(vec3 c, float amount) {
    float lum = luminance(c);
    return mix(vec3(lum), c, amount);
}

vec3 cel_shade(vec3 c, float levels) {
    return floor(c * levels + 0.5) / levels;
}

void main() {
    vec2 ps = params.OriginalSize.zw;
    vec3 ink_color = vec3(params.INK_COLOR_R, params.INK_COLOR_G, params.INK_COLOR_B);

    // 3x3 Sobel on luminance at native-res spacing
    float tl = luminance(texture(Source, vTexCoord + vec2(-ps.x,  ps.y)).rgb);
    float tc = luminance(texture(Source, vTexCoord + vec2( 0.0,   ps.y)).rgb);
    float tr = luminance(texture(Source, vTexCoord + vec2( ps.x,  ps.y)).rgb);
    float ml = luminance(texture(Source, vTexCoord + vec2(-ps.x,  0.0 )).rgb);
    float mr = luminance(texture(Source, vTexCoord + vec2( ps.x,  0.0 )).rgb);
    float bl = luminance(texture(Source, vTexCoord + vec2(-ps.x, -ps.y)).rgb);
    float bc = luminance(texture(Source, vTexCoord + vec2( 0.0,  -ps.y)).rgb);
    float br = luminance(texture(Source, vTexCoord + vec2( ps.x, -ps.y)).rgb);

    float edge_h = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    float edge_v = -tl - 2.0*tc - tr + bl + 2.0*bc + br;
    float edge = sqrt(edge_h * edge_h + edge_v * edge_v);

    // Hard threshold — only strong, unambiguous edges get ink.
    // INK_THICKNESS controls the cutoff: lower = only the strongest edges.
    float threshold = 0.25 / params.INK_THICKNESS;
    // Sharp cutoff with narrow blend zone to avoid gradual darkening
    float ink_mask = smoothstep(threshold, threshold * 1.2, edge);
    ink_mask *= params.INK_STRENGTH;
    ink_mask = clamp(ink_mask, 0.0, 1.0);

    vec3 color = texture(Source, vTexCoord).rgb;

    // Subtle saturation boost
    color = saturate_color(color, params.SATURATION_BOOST);

    // Gentle cel-shade (16 levels = almost invisible banding)
    vec3 cel = cel_shade(color, params.CEL_LEVELS);
    color = mix(color, cel, params.CEL_STRENGTH);

    // Composite ink — only on the strongest edges
    color = mix(color, ink_color, ink_mask);

    FragColor = vec4(color, 1.0);
}
