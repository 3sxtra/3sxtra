#version 450

// Omni-FX-SABR: Pass 1 - OmniMask (formerly Pass 3)
// Classify pixels into UI, thin-line detail, diagonal contour, and interior gradient zones.
// We read the UI Alpha mask from Pass 0 and augment it with our gradient/contour classification.

#pragma parameter OMNI_EDGE_SENS "OmniMask Edge Sens" 0.62 0.0 1.0 0.01
#pragma parameter OMNI_GRAD_PRES "OmniMask Grad Pres" 0.72 0.0 1.0 0.01
#pragma parameter OMNI_UI_WEIGHT "OmniMask UI Weight" 1.0 0.0 1.0 0.01

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float OMNI_EDGE_SENS;
    float OMNI_GRAD_PRES;
    float OMNI_UI_WEIGHT;
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

float luma(vec3 col) {
    return dot(col, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 dx = vec2(params.SourceSize.z, 0.0);
    vec2 dy = vec2(0.0, params.SourceSize.w);

    vec4 c = texture(Source, vTexCoord);
    
    // UI Mask is now handled purely by later heuristics or FinalComposite clamps
    float ui_mask = 0.0;
    
    // Sample 3x3 neighborhood for Sobel Edge Detection
    float tl = luma(texture(Source, vTexCoord - dx - dy).rgb);
    float t  = luma(texture(Source, vTexCoord - dy).rgb);
    float tr = luma(texture(Source, vTexCoord + dx - dy).rgb);
    float l  = luma(texture(Source, vTexCoord - dx).rgb);
    float r  = luma(texture(Source, vTexCoord + dx).rgb);
    float bl = luma(texture(Source, vTexCoord - dx + dy).rgb);
    float b  = luma(texture(Source, vTexCoord + dy).rgb);
    float br = luma(texture(Source, vTexCoord + dx + dy).rgb);

    // Sobel operator
    float sx = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
    float sy = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);
    
    // Normalized edge magnitude
    float edge_mag = clamp(sqrt(sx * sx + sy * sy), 0.0, 1.0);

    float classification_mask = 0.0;

    if (edge_mag > params.OMNI_EDGE_SENS) {
        // HARD EDGE / CONTOUR:
        // We actually want SABR to smooth out hard diagonal pixel steps.
        // So we give this a low protection score (0.0) so it gets filtered.
        classification_mask = 0.0;
    } 
    else if (edge_mag > 0.04) {
        // SOFT EDGE / GRADIENT RAMP:
        // This is painterly shading (muscles, cloth folds). SABR would warp this 
        // into blobs if it tries to smooth it like a hard contour.
        // We strictly protect it as paint!
        classification_mask = params.OMNI_GRAD_PRES;
    } 
    else {
        // FLAT COLOR INTERIOR:
        // Mild protection to prevent SABR from bleeding colors internally.
        classification_mask = 0.5;
    }
    
    // The final mask is the strongest protection rule that applies.
    // (UI always wins if it was detected in Pass 0)
    float final_mask = max(ui_mask, classification_mask);

    FragColor = vec4(c.rgb, final_mask);
}
