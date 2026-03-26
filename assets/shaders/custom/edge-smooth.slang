#version 450

// edge-smooth.slang — Sharp Pixel Art Upscaler
// Clean sharp-bilinear: snaps to nearest texel with a thin
// anti-aliased blend zone at pixel boundaries. No edge detection,
// no directional logic, no artifacts. Just crisp pixels with
// smooth transitions.

#pragma name EdgeSmooth
#pragma parameter SMOOTH_BLEND "Pixel Edge Softness" 0.4 0.0 0.5 0.05

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float SMOOTH_BLEND;
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

void main() {
    vec2 ps = params.SourceSize.zw;

    // Sub-pixel position within the texel grid
    vec2 texel = vTexCoord * params.SourceSize.xy - 0.5;
    vec2 texel_floor = floor(texel);
    vec2 f = texel - texel_floor;
    vec2 base = (texel_floor + 0.5) * ps;

    // Sample the 2x2 quad
    vec3 E = texture(Source, base).rgb;
    vec3 F = texture(Source, base + vec2(ps.x, 0.0)).rgb;
    vec3 H = texture(Source, base + vec2(0.0, ps.y)).rgb;
    vec3 I = texture(Source, base + vec2(ps.x, ps.y)).rgb;

    // Sharp blend: snap toward nearest texel, with a thin
    // anti-aliased transition controlled by SMOOTH_BLEND.
    // At 0.0 = pure nearest neighbor
    // At 0.5 = pure bilinear
    float b = max(params.SMOOTH_BLEND, 0.001);
    vec2 s;
    s.x = smoothstep(0.5 - b, 0.5 + b, f.x);
    s.y = smoothstep(0.5 - b, 0.5 + b, f.y);

    vec3 result = mix(
        mix(E, F, s.x),
        mix(H, I, s.x),
        s.y
    );

    FragColor = vec4(result, 1.0);
}
