#include "palette_remix.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <string>



// --- CPS3 Format Helpers ---
// CPS3 format: 1BBBBBGGGGGRRRRR (MSB always set, 5 bits per channel)
static inline int cps3_r(u16 c) { return (c >> 0) & 0x1F; }
static inline int cps3_g(u16 c) { return (c >> 5) & 0x1F; }
static inline int cps3_b(u16 c) { return (c >> 10) & 0x1F; }
static inline u16 cps3_pack(int r, int g, int b) {
    return (u16)(0x8000 | ((b & 0x1F) << 10) | ((g & 0x1F) << 5) | (r & 0x1F));
}

// Convert 5-bit CPS3 channel to 8-bit for color math
static inline int to8bit(int v5) { return (v5 << 3) | (v5 >> 2); }
// Convert 8-bit back to 5-bit
static inline int to5bit(int v8) { return v8 >> 3; }

namespace PaletteRemix {

// --- HLS Math (Ported from Python colorsys) ---
static void rgb_to_hls(float r, float g, float b, float& h, float& l, float& s) {
    float maxc = std::max({r, g, b});
    float minc = std::min({r, g, b});
    l = (minc + maxc) / 2.0f;
    if (minc == maxc) {
        h = 0.0f;
        s = 0.0f;
        return;
    }
    if (l <= 0.5f) {
        s = (maxc - minc) / (maxc + minc);
    } else {
        s = (maxc - minc) / (2.0f - maxc - minc);
    }
    float rc = (maxc - r) / (maxc - minc);
    float gc = (maxc - g) / (maxc - minc);
    float bc = (maxc - b) / (maxc - minc);
    if (r == maxc) {
        h = bc - gc;
    } else if (g == maxc) {
        h = 2.0f + rc - bc;
    } else {
        h = 4.0f + gc - rc;
    }
    h = fmod((h / 6.0f), 1.0f);
    if (h < 0.0f) h += 1.0f;
}

static float v(float m1, float m2, float hue) {
    hue = fmod(hue, 1.0f);
    if (hue < 0.0f) hue += 1.0f;
    if (hue < 1.0f/6.0f) return m1 + (m2 - m1) * hue * 6.0f;
    if (hue < 0.5f) return m2;
    if (hue < 2.0f/3.0f) return m1 + (m2 - m1) * (2.0f/3.0f - hue) * 6.0f;
    return m1;
}

static void hls_to_rgb(float h, float l, float s, float& r, float& g, float& b) {
    if (s == 0.0f) {
        r = g = b = l;
        return;
    }
    float m2;
    if (l <= 0.5f) {
        m2 = l * (1.0f + s);
    } else {
        m2 = l + s - (l * s);
    }
    float m1 = 2.0f * l - m2;
    r = v(m1, m2, h + 1.0f/3.0f);
    g = v(m1, m2, h);
    b = v(m1, m2, h - 1.0f/3.0f);
}

static void get_mood_hls(float& h, float& lum, float& s, const std::string& mood_name) {
    if (mood_name == "Noir") {
        s *= 0.1f;
        lum = std::pow(lum, 1.2f);
    } else if (mood_name == "Miami") {
        if (lum < 0.5f) {
            float target_h = 0.5f, blend = (0.5f - lum) * 0.5f;
            h = h * (1.0f - blend) + target_h * blend;
        } else {
            float target_h = 0.9f, blend = (lum - 0.5f) * 0.5f;
            h = h * (1.0f - blend) + target_h * blend;
        }
        s *= 1.2f;
    } else if (mood_name == "Grindhouse") {
        float target_h = 0.14f, blend = 0.3f;
        h = h * (1.0f - blend) + target_h * blend;
        lum = (lum - 0.5f) * 1.2f + 0.5f;
        s *= 0.8f;
    } else if (mood_name == "Sunrise") {
        float target_h = 0.08f, blend = 0.2f;
        h = h * (1.0f - blend) + target_h * blend;
        lum = lum * 1.1f + 0.05f;
        s *= 1.1f;
    } else if (mood_name == "Noon") {
        lum = (lum - 0.5f) * 1.3f + 0.5f;
        s *= 1.1f;
        if (!(h > 0.5f && h < 0.7f)) {
            h = h * 0.9f + 0.14f * 0.1f;
        }
    } else if (mood_name == "Dusk") {
        float target_h = 0.75f, blend = 0.3f;
        h = h * (1.0f - blend) + target_h * blend;
        lum *= 0.8f;
        s *= 1.2f;
    } else if (mood_name == "Midnight") {
        float target_h = 0.66f, blend = 0.4f;
        h = h * (1.0f - blend) + target_h * blend;
        lum *= 0.4f;
        lum = (lum - 0.2f) * 1.5f + 0.2f;
    } else if (mood_name == "Stormy") {
        s *= 0.2f;
        float target_h = 0.6f, blend = 0.2f;
        h = h * (1.0f - blend) + target_h * blend;
        lum *= 0.9f;
    } else if (mood_name == "Neon-night") {
        if (lum > 0.6f) {
            s = std::min(1.0f, s * 1.5f);
        } else {
            s *= 0.5f;
            lum *= 0.7f;
        }
    } else if (mood_name == "Cold Steel") {
        float target_h = 0.55f, blend = 0.4f;
        h = h * (1.0f - blend) + target_h * blend;
        s *= 0.8f;
    } else if (mood_name == "Heatwave") {
        float target_h = 0.05f, blend = 0.4f;
        h = h * (1.0f - blend) + target_h * blend;
        lum = lum * 1.1f + 0.1f;
        s *= 1.2f;
    }
}

void apply_global_mood(const char* mood_name, float intensity, u16* colors, size_t count) {
    if (intensity <= 0.0f) return;
    std::string mood(mood_name);

    for (size_t i = 0; i < count; i++) {
        if (i % 16 == 0) continue; // Skip transparency index
        if (colors[i] == 0) continue; // Pure black skipping heuristic
        
        int r8 = to8bit(cps3_r(colors[i]));
        int g8 = to8bit(cps3_g(colors[i]));
        int b8 = to8bit(cps3_b(colors[i]));
        
        float h, lum, s;
        rgb_to_hls(r8 / 255.0f, g8 / 255.0f, b8 / 255.0f, h, lum, s);
        
        get_mood_hls(h, lum, s, mood);
        
        lum = std::max(0.0f, std::min(1.0f, lum));
        s = std::max(0.0f, std::min(1.0f, s));
        h = fmod(h, 1.0f);
        if (h < 0.0f) h += 1.0f;
        
        float nr, ng, nb;
        hls_to_rgb(h, lum, s, nr, ng, nb);
        
        int new_r8 = (int)std::round(nr * 255.0f);
        int new_g8 = (int)std::round(ng * 255.0f);
        int new_b8 = (int)std::round(nb * 255.0f);
        
        if (intensity < 1.0f) {
            new_r8 = (int)(r8 * (1.0f - intensity) + new_r8 * intensity);
            new_g8 = (int)(g8 * (1.0f - intensity) + new_g8 * intensity);
            new_b8 = (int)(b8 * (1.0f - intensity) + new_b8 * intensity);
        }
        
        colors[i] = cps3_pack(to5bit(new_r8), to5bit(new_g8), to5bit(new_b8));
    }
}

void apply_ramp_effect(const char* effect_name, float intensity, u16* colors, size_t count, const std::vector<int>& ramp_indices) {
    if (intensity <= 0.0f || ramp_indices.empty()) return;
    std::string effect(effect_name);
    
    // Some mood effects can be applied via this function for specific ramps
    if (effect == "Noir" || effect == "Miami" || effect == "Grindhouse" || 
        effect == "Sunrise" || effect == "Noon" || effect == "Dusk" || 
        effect == "Midnight" || effect == "Stormy" || effect == "Neon-night" || 
        effect == "Cold Steel" || effect == "Heatwave") {
        for (int idx : ramp_indices) {
            if (idx % 16 == 0 || colors[idx] == 0) continue;
            int r8 = to8bit(cps3_r(colors[idx]));
            int g8 = to8bit(cps3_g(colors[idx]));
            int b8 = to8bit(cps3_b(colors[idx]));
            float h, lum, s;
            rgb_to_hls(r8 / 255.0f, g8 / 255.0f, b8 / 255.0f, h, lum, s);
            get_mood_hls(h, lum, s, effect);
            lum = std::max(0.0f, std::min(1.0f, lum));
            s = std::max(0.0f, std::min(1.0f, s));
            h = fmod(h, 1.0f);
            if (h < 0.0f) h += 1.0f;
            float nr, ng, nb;
            hls_to_rgb(h, lum, s, nr, ng, nb);
            int new_r8 = (int)std::round(nr * 255.0f);
            int new_g8 = (int)std::round(ng * 255.0f);
            int new_b8 = (int)std::round(nb * 255.0f);
            if (intensity < 1.0f) {
                new_r8 = (int)(r8 * (1.0f - intensity) + new_r8 * intensity);
                new_g8 = (int)(g8 * (1.0f - intensity) + new_g8 * intensity);
                new_b8 = (int)(b8 * (1.0f - intensity) + new_b8 * intensity);
            }
            colors[idx] = cps3_pack(to5bit(new_r8), to5bit(new_g8), to5bit(new_b8));
        }
        return;
    }

    std::vector<int> valid_indices;
    for (int idx : ramp_indices) {
        if (idx % 16 != 0 && colors[idx] != 0) {
            valid_indices.push_back(idx);
        }
    }
    if (valid_indices.empty()) return;

    if (effect == "Gradient Curve") {
        float power = 0.1f + (intensity * 100.0f) / 50.0f * 0.9f;
        if ((intensity * 100.0f) >= 50.0f) {
            power = 1.0f + ((intensity * 100.0f) - 50.0f) / 50.0f * 2.0f;
        }
        
        float min_lum = 1.0f, max_lum = 0.0f;
        for (int idx : valid_indices) {
            float h, l, s;
            rgb_to_hls(to8bit(cps3_r(colors[idx]))/255.0f, to8bit(cps3_g(colors[idx]))/255.0f, to8bit(cps3_b(colors[idx]))/255.0f, h, l, s);
            if (l < min_lum) min_lum = l;
            if (l > max_lum) max_lum = l;
        }
        float range_lum = max_lum - min_lum;
        if (range_lum > 0.01f) {
             for (int idx : valid_indices) {
                 float h, l, s;
                 rgb_to_hls(to8bit(cps3_r(colors[idx]))/255.0f, to8bit(cps3_g(colors[idx]))/255.0f, to8bit(cps3_b(colors[idx]))/255.0f, h, l, s);
                 float norm_lum = (l - min_lum) / range_lum;
                 float new_norm_lum = std::pow(norm_lum, power);
                 float new_lum = min_lum + new_norm_lum * range_lum;
                 float nr, ng, nb;
                 hls_to_rgb(h, new_lum, s, nr, ng, nb);
                 colors[idx] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
             }
        }
    } else if (effect == "Posterize") {
        int steps = 2 + (int)(intensity * 14.0f);
        for (int idx : valid_indices) {
             float h, l, s;
             rgb_to_hls(to8bit(cps3_r(colors[idx]))/255.0f, to8bit(cps3_g(colors[idx]))/255.0f, to8bit(cps3_b(colors[idx]))/255.0f, h, l, s);
             float new_lum = std::round(l * steps) / steps;
             float nr, ng, nb;
             hls_to_rgb(h, new_lum, s, nr, ng, nb);
             colors[idx] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
        }
    } else if (effect == "Dither-Friendly") {
        std::vector<int> sorted = valid_indices;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
            float ha, la, sa, hb, lb, sb;
            rgb_to_hls(to8bit(cps3_r(colors[a]))/255.0f, to8bit(cps3_g(colors[a]))/255.0f, to8bit(cps3_b(colors[a]))/255.0f, ha, la, sa);
            rgb_to_hls(to8bit(cps3_r(colors[b]))/255.0f, to8bit(cps3_g(colors[b]))/255.0f, to8bit(cps3_b(colors[b]))/255.0f, hb, lb, sb);
            return la < lb;
        });
        
        float start_h, start_l, start_s;
        rgb_to_hls(to8bit(cps3_r(colors[sorted.front()]))/255.0f, to8bit(cps3_g(colors[sorted.front()]))/255.0f, to8bit(cps3_b(colors[sorted.front()]))/255.0f, start_h, start_l, start_s);
        
        float end_h, end_l, end_s;
        rgb_to_hls(to8bit(cps3_r(colors[sorted.back()]))/255.0f, to8bit(cps3_g(colors[sorted.back()]))/255.0f, to8bit(cps3_b(colors[sorted.back()]))/255.0f, end_h, end_l, end_s);
        
        int n = sorted.size();
        for (int i = 0; i < n; i++) {
            float t = (n > 1) ? (float)i / (n - 1) : 0.0f;
            float new_lum = start_l + (end_l - start_l) * t;
            float new_h = start_h + (end_h - start_h) * t;
            float new_s = start_s + (end_s - start_s) * t;
            float nr, ng, nb;
            hls_to_rgb(new_h, new_lum, new_s, nr, ng, nb);
            colors[sorted[i]] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
        }
    } else if (effect == "Edge Accent") {
        float strength = intensity; // 0.0 to 1.0 passed in
        std::vector<int> sorted = valid_indices;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
            float ha, la, sa, hb, lb, sb;
            rgb_to_hls(to8bit(cps3_r(colors[a]))/255.0f, to8bit(cps3_g(colors[a]))/255.0f, to8bit(cps3_b(colors[a]))/255.0f, ha, la, sa);
            rgb_to_hls(to8bit(cps3_r(colors[b]))/255.0f, to8bit(cps3_g(colors[b]))/255.0f, to8bit(cps3_b(colors[b]))/255.0f, hb, lb, sb);
            return la < lb;
        });
        
        int dark_idx = sorted.front();
        float h, lum, s;
        rgb_to_hls(to8bit(cps3_r(colors[dark_idx]))/255.0f, to8bit(cps3_g(colors[dark_idx]))/255.0f, to8bit(cps3_b(colors[dark_idx]))/255.0f, h, lum, s);
        lum = std::max(0.0f, lum - strength);
        float nr, ng, nb;
        hls_to_rgb(h, lum, s, nr, ng, nb);
        colors[dark_idx] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
        
        int bright_idx = sorted.back();
        rgb_to_hls(to8bit(cps3_r(colors[bright_idx]))/255.0f, to8bit(cps3_g(colors[bright_idx]))/255.0f, to8bit(cps3_b(colors[bright_idx]))/255.0f, h, lum, s);
        lum = std::min(1.0f, lum + strength);
        hls_to_rgb(h, lum, s, nr, ng, nb);
        colors[bright_idx] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
        
    } else if (effect == "Neonizer") {
        std::vector<int> sorted = valid_indices;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
            float ha, la, sa, hb, lb, sb;
            rgb_to_hls(to8bit(cps3_r(colors[a]))/255.0f, to8bit(cps3_g(colors[a]))/255.0f, to8bit(cps3_b(colors[a]))/255.0f, ha, la, sa);
            rgb_to_hls(to8bit(cps3_r(colors[b]))/255.0f, to8bit(cps3_g(colors[b]))/255.0f, to8bit(cps3_b(colors[b]))/255.0f, hb, lb, sb);
            return la < lb;
        });
        int threshold = (int)(sorted.size() * 0.7f);
        for (int i = 0; i < (int)sorted.size(); i++) {
            float h, lum, s;
            int idx = sorted[i];
            rgb_to_hls(to8bit(cps3_r(colors[idx]))/255.0f, to8bit(cps3_g(colors[idx]))/255.0f, to8bit(cps3_b(colors[idx]))/255.0f, h, lum, s);
            if (i < threshold) {
                lum *= 0.5f;
                s *= 0.5f;
            } else {
                s = std::min(1.0f, s * 1.5f);
                lum = std::min(1.0f, lum * 1.2f);
            }
            float nr, ng, nb;
            hls_to_rgb(h, lum, s, nr, ng, nb);
            colors[idx] = cps3_pack(to5bit(std::round(nr*255.0f)), to5bit(std::round(ng*255.0f)), to5bit(std::round(nb*255.0f)));
        }
    }
}

void apply_layer_look(const char* look_name, float intensity, u16* colors, size_t count, const std::vector<int>& layer_indices) {
    if (intensity <= 0.0f) return;
    std::string look(look_name);
    
    for (int idx : layer_indices) {
        if (idx % 16 == 0 || colors[idx] == 0) continue;
        
        int orig_r8 = to8bit(cps3_r(colors[idx]));
        int orig_g8 = to8bit(cps3_g(colors[idx]));
        int orig_b8 = to8bit(cps3_b(colors[idx]));
        
        float h, lum, s;
        rgb_to_hls(orig_r8/255.0f, orig_g8/255.0f, orig_b8/255.0f, h, lum, s);
        
        float new_lum = lum, new_s = s;
        
        if (look == "Foreground Pop") {
            new_lum = (lum - 0.5f) * 1.2f + 0.5f;
            new_s = std::min(1.0f, s * 1.3f);
        } else if (look == "Background Wash") {
            new_s *= 0.5f;
            new_lum = lum * 0.8f + 0.2f;
        } else if (look == "CRT / Arcade") {
            if (lum > 0.7f) {
                new_lum = std::min(1.0f, lum * 1.1f);
            }
        } else if (look == "Comic / Cel-shade" || look == "Comic / Cel") {
            new_lum = (lum - 0.5f) * 1.5f + 0.5f;
            float steps = 4.0f;
            new_lum = std::round(new_lum * steps) / steps;
        }
        
        new_lum = std::max(0.0f, std::min(1.0f, new_lum));
        new_s = std::max(0.0f, std::min(1.0f, new_s));
        
        float nr, ng, nb;
        hls_to_rgb(h, new_lum, new_s, nr, ng, nb);
        
        int new_r8 = (int)std::round(nr * 255.0f);
        int new_g8 = (int)std::round(ng * 255.0f);
        int new_b8 = (int)std::round(nb * 255.0f);
        
        if (intensity < 1.0f) {
            new_r8 = (int)(orig_r8 * (1.0f - intensity) + new_r8 * intensity);
            new_g8 = (int)(orig_g8 * (1.0f - intensity) + new_g8 * intensity);
            new_b8 = (int)(orig_b8 * (1.0f - intensity) + new_b8 * intensity);
        }
        
        colors[idx] = cps3_pack(to5bit(new_r8), to5bit(new_g8), to5bit(new_b8));
    }
}

bool is_likely_ramp(const std::vector<u16>& ramp_colors) {
    std::vector<float> lums;
    for (size_t i = 0; i < ramp_colors.size(); i++) {
        if (i == 0 || ramp_colors[i] == 0) continue; // Skip trans/black
        float h, l, s;
        rgb_to_hls(to8bit(cps3_r(ramp_colors[i]))/255.0f, to8bit(cps3_g(ramp_colors[i]))/255.0f, to8bit(cps3_b(ramp_colors[i]))/255.0f, h, l, s);
        lums.push_back(l);
    }
    if (lums.size() < 3) return false;
    
    bool is_increasing = true;
    for (size_t i = 0; i < lums.size() - 1; i++) {
        if (lums[i] > lums[i+1]) { is_increasing = false; break; }
    }
    
    bool is_decreasing = true;
    for (size_t i = 0; i < lums.size() - 1; i++) {
        if (lums[i] < lums[i+1]) { is_decreasing = false; break; }
    }
    
    if (!is_increasing && !is_decreasing) return false;
    
    float min_l = lums[0], max_l = lums[0];
    for (float l : lums) {
        if (l < min_l) min_l = l;
        if (l > max_l) max_l = l;
    }
    return (max_l - min_l) >= 0.1f;
}



} // namespace PaletteRemix

