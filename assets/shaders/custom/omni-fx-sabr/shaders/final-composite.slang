#version 450

// Omni-FX-SABR: Pass 4 - FinalComposite
// Merge protected UI, guarded topology, and softened contours.
// Uses the 'Original' raw framebuffer to ensure zero degradation of protected pixels.

#pragma parameter FINAL_UI_PRIORITY "Final UI Priority" 1.0 0.0 1.0 0.01
#pragma parameter FINAL_GRAD_CLAMP "Final Grad Clamp" 0.70 0.0 1.0 0.01

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float FINAL_UI_PRIORITY;
    float FINAL_GRAD_CLAMP;
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

layout(set = 0, binding = 2) uniform sampler2D Source;    // Output from Pass 3 (SABR)
layout(set = 0, binding = 3) uniform sampler2D Original;  // Raw emulator output

void main() {
    // Processed image (SABR output)
    vec4 processed = texture(Source, vTexCoord);
    vec3 c_proc = processed.rgb;
    float protection = processed.a;

    // Raw emulator image (Forced to Nearest-Neighbor to prevent hardware bilinear fog)
    vec2 pos_1x = vTexCoord * params.OriginalSize.xy;
    vec2 center_1x = (floor(pos_1x) + 0.5) * params.OriginalSize.zw;
    vec3 c_orig = texture(Original, center_1x).rgb;
    
    vec3 final_c = c_proc;
    
    // 1. Tonal Variation Clamp (FINAL_GRAD_CLAMP)
    // 1. Tonal Variation Clamp
    // We only clamp if this is NOT a hard contour. Hard contours need full freedom 
    // to differ from the original blocky pixel (up to 1.0 diff) to draw clean curves!
    if (protection > 0.1) {
        vec3 diff = abs(c_orig - c_proc);
        float max_diff = max(diff.r, max(diff.g, diff.b));
        float allowed_diff = 1.0 - params.FINAL_GRAD_CLAMP;
        if (max_diff > allowed_diff) {
            float pull_factor = allowed_diff / (max_diff + 0.0001);
            final_c = mix(c_orig, c_proc, pull_factor);
        }
    }
    
    // 2. UI & Guard Topology Priority
    // Force the pixel back to the absolute original if it was strongly protected.
    float override_weight = clamp(protection * params.FINAL_UI_PRIORITY, 0.0, 1.0);
    
    // We target masks > 0.8 (Thin lines = 0.82, UI = 0.85+). 
    // Gradients (~0.72) rely mostly on the gradient clamp above instead of hard pixel-swapping.
    if (override_weight > 0.8) {
        float t = smoothstep(0.8, 1.0, override_weight);
        final_c = mix(final_c, c_orig, t);
    }

    // Output
    FragColor = vec4(final_c, 1.0);
}
