#version 450

// Omni-FX-SABR: Pass 0 - RigidLockUI (formerly Pass 2)
// Scale2x-style detection for HUD text, life bars, and orthogonal UI elements.
// Outputs the original color in RGB, and a "UI Protection Mask" in the Alpha channel.

#pragma parameter RIGIDUI_THRESH "Rigid UI Coverage Thresh" 0.78 0.0 1.0 0.01
#pragma parameter RIGIDUI_CORNER "Rigid UI Corner Bias" 0.85 0.0 1.0 0.01
#pragma parameter RIGIDUI_EXCLUDE "Rigid UI Char Exclude" 1.0 0.0 1.0 1.0

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float RIGIDUI_THRESH;
    float RIGIDUI_CORNER;
    float RIGIDUI_EXCLUDE;
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

// Simple color distance to detect solid color blocks (UI elements)
float color_dist(vec3 c1, vec3 c2) {
    vec3 diff = c1 - c2;
    return dot(diff, diff);
}

void main() {
    vec2 dx = vec2(params.SourceSize.z, 0.0);
    vec2 dy = vec2(0.0, params.SourceSize.w);

    vec3 c  = texture(Source, vTexCoord).rgb;
    vec3 t  = texture(Source, vTexCoord - dy).rgb;
    vec3 b  = texture(Source, vTexCoord + dy).rgb;
    vec3 l  = texture(Source, vTexCoord - dx).rgb;
    vec3 r  = texture(Source, vTexCoord + dx).rgb;

    // Scale2x logic looks for exact matching neighbors.
    // UI elements in CPS-3 (like health bars, text) often have solid flat colors
    // with strict orthogonal edges.
    
    // We compute how "similar" orthogonal neighbors are. 
    // In strict Scale2x: (t == l && t != b && l != r)
    // We use a generous tolerance to account for CPS-3's anti-aliased UI fonts.
    float tol = 0.02; 
    
    bool match_tl = (color_dist(t, l) < tol) && (color_dist(t, b) > tol) && (color_dist(l, r) > tol);
    bool match_tr = (color_dist(t, r) < tol) && (color_dist(t, b) > tol) && (color_dist(r, l) > tol);
    bool match_bl = (color_dist(b, l) < tol) && (color_dist(b, t) > tol) && (color_dist(l, r) > tol);
    bool match_br = (color_dist(b, r) < tol) && (color_dist(b, t) > tol) && (color_dist(r, l) > tol);

    // If it forms a clean corner, it's highly likely to be a UI pixel.
    float ui_mask = 0.0;
    
    if (match_tl || match_tr || match_bl || match_br) {
        ui_mask = params.RIGIDUI_CORNER; // Assign corner bias weight
    }
    
    // Additional check: solid orthogonal lines (1px thick frames common in UI)
    bool line_h = (color_dist(l, c) < tol) && (color_dist(r, c) < tol) && (color_dist(t, c) > tol) && (color_dist(b, c) > tol);
    bool line_v = (color_dist(t, c) < tol) && (color_dist(b, c) < tol) && (color_dist(l, c) > tol) && (color_dist(r, c) > tol);
    
    if (line_h || line_v) {
        ui_mask = max(ui_mask, params.RIGIDUI_THRESH);
    }
    
    // If character exclusion is active, we could theoretically analyze larger blocks 
    // to reject complex shading here, but for now we rely on the rigid constraints.
    if (params.RIGIDUI_EXCLUDE < 0.5) {
        // Fallback or bypass logic if exclusion is off
    }
    
    // Output RGB with UI Mask in the Alpha channel.
    // The next passes (OmniMask and SABR) will read this alpha to know what to protect!
    FragColor = vec4(c, ui_mask);
}
