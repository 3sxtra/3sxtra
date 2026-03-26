#ifndef PALETTE_REMIX_H
#define PALETTE_REMIX_H

#include "types.h"

#ifdef __cplusplus

#include <vector>

namespace PaletteRemix {
    
    // Apply a global mood to a full set of colors (e.g. 64 colors for a character)
    // colors: array of u16 (CPS3 format)
    void apply_global_mood(const char* mood_name, float intensity, u16* colors, size_t count);
    
    // Apply a ramp effect to a specific set of indices (usually representing a single ramp)
    void apply_ramp_effect(const char* effect_name, float intensity, u16* colors, size_t count, const std::vector<int>& ramp_indices);
    
    // Apply a layer look to a specific set of indices
    void apply_layer_look(const char* look_name, float intensity, u16* colors, size_t count, const std::vector<int>& layer_indices);
    
    // Heuristic to detect if a set of colors is a gradient ramp
    bool is_likely_ramp(const std::vector<u16>& ramp_colors);
    
    // Extract which ColorRAM rows (0-511) are currently mapped to by active PPG chunks for L0, L1, L2
    void get_active_stage_palettes(bool l0, bool l1, bool l2, bool* active_rows);
}

#endif /* __cplusplus */

#endif /* PALETTE_REMIX_H */
