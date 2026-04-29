#version 450

// Omni-FX-SABR: Pass 2 - ScaleFXGuard (formerly Pass 4)
// Detect 1-pixel-ish lines, junctions, and fragile sprite details (ropes, fingers, eyes).
// Protects them from being bloated by SABR, optionally casting a 1px protective halo.

#pragma parameter GUARD_THINLINE "Guard Thin Line" 0.82 0.0 1.0 0.01
#pragma parameter GUARD_JUNCTION "Guard Junction" 0.76 0.0 1.0 0.01
#pragma parameter GUARD_RADIUS "Guard Exp Radius" 1.0 0.0 3.0 1.0

layout(push_constant) uniform Push {
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float GUARD_THINLINE;
    float GUARD_JUNCTION;
    float GUARD_RADIUS;
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

float color_dist(vec3 c1, vec3 c2) {
    vec3 d = c1 - c2;
    return dot(d, d);
}

// Returns 0 = none, 1 = thinline, 2 = junction/endpoint
int get_fragility(vec2 pos, vec2 dx, vec2 dy) {
    vec3 c  = texture(Source, pos).rgb;
    vec3 t  = texture(Source, pos - dy).rgb;
    vec3 b  = texture(Source, pos + dy).rgb;
    vec3 l  = texture(Source, pos - dx).rgb;
    vec3 r  = texture(Source, pos + dx).rgb;
    vec3 tl = texture(Source, pos - dx - dy).rgb;
    vec3 tr = texture(Source, pos + dx - dy).rgb;
    vec3 bl = texture(Source, pos - dx + dy).rgb;
    vec3 br = texture(Source, pos + dx + dy).rgb;

    float tol = 0.01; // Color distance tolerance
    
    // Count similar orthogonal neighbors
    int sim_o = 0;
    if (color_dist(c, t) < tol) sim_o++;
    if (color_dist(c, b) < tol) sim_o++;
    if (color_dist(c, l) < tol) sim_o++;
    if (color_dist(c, r) < tol) sim_o++;
    
    // Count similar diagonal neighbors
    int sim_d = 0;
    if (color_dist(c, tl) < tol) sim_d++;
    if (color_dist(c, tr) < tol) sim_d++;
    if (color_dist(c, bl) < tol) sim_d++;
    if (color_dist(c, br) < tol) sim_d++;

    // Junctions (endpoints, isolated pixels, tight dot-like corners)
    if (sim_o <= 1 && sim_d <= 1) return 2;
    
    // Thin diagonal lines (similar to exactly 2 diagonals that are opposite)
    bool diag1 = (color_dist(c, tl) < tol) && (color_dist(c, br) < tol) && (sim_o == 0);
    bool diag2 = (color_dist(c, tr) < tol) && (color_dist(c, bl) < tol) && (sim_o == 0);
    if (diag1 || diag2) return 1;

    // Thin orthogonal lines (similar to exactly 2 opposite orthogonals)
    bool ortho_h = (color_dist(c, l) < tol) && (color_dist(c, r) < tol) && (color_dist(c, t) > tol) && (color_dist(c, b) > tol);
    bool ortho_v = (color_dist(c, t) < tol) && (color_dist(c, b) < tol) && (color_dist(c, l) > tol) && (color_dist(c, r) > tol);
    if (ortho_h || ortho_v) return 1;

    return 0;
}

float get_protection(int type) {
    if (type == 2) return params.GUARD_JUNCTION;
    if (type == 1) return params.GUARD_THINLINE;
    return 0.0;
}

void main() {
    vec2 dx = vec2(params.SourceSize.z, 0.0);
    vec2 dy = vec2(0.0, params.SourceSize.w);

    vec4 center_sample = texture(Source, vTexCoord);
    vec3 c = center_sample.rgb;
    float incoming_mask = center_sample.a;

    // Evaluate fragility of the center pixel
    // BYPASS FOR TESTING: We are forcing p = 0.0 to see if ScaleFXGuard was over-triggering
    // and killing SABR.
    int type_c = get_fragility(vTexCoord, dx, dy);
    float p = 0.0;
    
    // Evaluate fragility of neighbors if an expansion radius is requested
    // This creates the "guard halo" to prevent neighboring SABR pixels from bloating into the detail
    if (params.GUARD_RADIUS > 0.5) {
        // BYPASSED
    }
    
    // Combine with the incoming mask from RigidLockUI and OmniMask
    float final_mask = max(incoming_mask, p);

    FragColor = vec4(c, final_mask);
}
