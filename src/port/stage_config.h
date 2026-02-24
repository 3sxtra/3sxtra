#ifndef STAGE_CONFIG_H
#define STAGE_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STAGE_LAYERS 4

typedef enum {
    SCALE_MODE_FIT_HEIGHT, // Scale to match 512px height (default)
    SCALE_MODE_STRETCH,    // Stretch to viewport
    SCALE_MODE_NATIVE,     // 1:1 pixel mapping
    SCALE_MODE_MANUAL      // Use manual scale factor
} LayerScaleMode;

typedef struct {
    char filename[64];
    bool enabled;
    LayerScaleMode scale_mode;
    float scale_factor_x;
    float scale_factor_y;
    float parallax_x; // 1.0 = moves with camera, 0.0 = static
    float parallax_y;
    float offset_x;
    float offset_y;
    int original_bg_index; // Original game layer index to copy speed from (-1 = default/none)
    int z_index;           // Draw order (lower = back)
    bool loop_x;
    bool loop_y;
} StageLayerConfig;

typedef struct {
    bool is_custom; // True if loaded from config
    StageLayerConfig layers[MAX_STAGE_LAYERS];
} StageConfig;

// Global config instance for the current stage
extern StageConfig g_stage_config;

/**
 * @brief Initialize stage configuration system.
 */
void StageConfig_Init(void);

/**
 * @brief Load configuration for a specific stage index.
 * If no config exists, sets defaults based on native behavior.
 */
void StageConfig_Load(int stage_index);

/**
 * @brief Save current configuration to stage_config.ini in the stage folder.
 */
void StageConfig_Save(int stage_index);

/**
 * @brief Get the default configuration for a specific layer index.
 */
void StageConfig_SetDefaultLayer(int layer_idx);

#ifdef __cplusplus
}
#endif

#endif // STAGE_CONFIG_H
