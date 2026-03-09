/**
 * @file sdl_app_scale.h
 * @brief Scale-mode enum, letterbox geometry, and viewport rectangle utilities.
 *
 * Extracted from sdl_app.c to isolate pure geometry helpers from the main
 * application lifecycle.  All functions are stateless except for the cached
 * letterbox computation and the global scale_mode setting.
 */
#ifndef SDL_APP_SCALE_H
#define SDL_APP_SCALE_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Available output scale / filtering modes. */
typedef enum ScaleMode {
    SCALEMODE_NEAREST,
    SCALEMODE_LINEAR,
    SCALEMODE_SOFT_LINEAR,
    SCALEMODE_SQUARE_PIXELS,
    SCALEMODE_INTEGER,
    SCALEMODE_PIXEL_ART,
    SCALEMODE_COUNT
} ScaleMode;

/** @brief Current scale mode (global, persisted to config). */
extern ScaleMode scale_mode;

/** @brief Return the display name for the current scale mode. */
const char* scale_mode_name(void);

/** @brief Convert a ScaleMode enum to its config-file string key. */
const char* scale_mode_to_config_string(ScaleMode mode);

/** @brief Parse a config-file string into a ScaleMode enum. */
ScaleMode config_string_to_scale_mode(const char* string);

/** @brief Advance to the next scale mode (wrapping) and persist to config. */
void cycle_scale_mode(void);

/** @brief Get the current letterbox/viewport rectangle based on scale mode. */
SDL_FRect get_letterbox_rect(int win_w, int win_h);

#ifdef __cplusplus
}
#endif

#endif /* SDL_APP_SCALE_H */
