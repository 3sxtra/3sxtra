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
    void apply_ramp_effect(const char* effect_name, float intensity, u16* colors, size_t count,
                           const std::vector<int>& ramp_indices);

    // Apply a layer look to a specific set of indices
    void apply_layer_look(const char* look_name, float intensity, u16* colors, size_t count,
                          const std::vector<int>& layer_indices);

    // Heuristic to detect if a set of colors is a gradient ramp
    bool is_likely_ramp(const std::vector<u16>& ramp_colors);

} // namespace PaletteRemix

#endif /* __cplusplus */

#endif /* PALETTE_REMIX_H */
