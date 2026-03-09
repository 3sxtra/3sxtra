/**
 * @file sdl_app_scale.c
 * @brief Scale-mode enum, letterbox geometry, and viewport rectangle utilities.
 *
 * Pure geometry helpers extracted from sdl_app.c.  Manages the global
 * scale_mode setting and provides cached letterbox rectangle computation
 * for the various output scaling modes (nearest, linear, integer, etc.).
 */
#include "port/sdl/app/sdl_app_scale.h"

#include "port/config/config.h"
#include "port/sdl/app/sdl_app_internal.h"

#include <SDL3/SDL.h>
#include <string.h>

/* ── Global state ──────────────────────────────────────────────────────── */

ScaleMode scale_mode = SCALEMODE_NEAREST;

static const float display_target_ratio = 4.0f / 3.0f;

/* ⚡ Letterbox rect cache — avoid recomputing every frame when geometry is stable. */
static SDL_FRect cached_letterbox_rect = { 0 };
static int cached_lb_win_w = 0;
static int cached_lb_win_h = 0;
static int cached_lb_scale_mode = -1;

/* ── Conversion helpers ────────────────────────────────────────────────── */

/** @brief Return the display name for the current scale mode. */
const char* scale_mode_name(void) {
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
        return "Nearest";
    case SCALEMODE_LINEAR:
        return "Linear";
    case SCALEMODE_SOFT_LINEAR:
        return "Soft Linear";
    case SCALEMODE_SQUARE_PIXELS:
        return "Square Pixels";
    case SCALEMODE_INTEGER:
        return "Integer";
    case SCALEMODE_PIXEL_ART:
        return "Pixel Art";
    case SCALEMODE_COUNT:
        return "Unknown";
    }
    return "Unknown";
}

/** @brief Convert a ScaleMode enum to its config-file string key. */
const char* scale_mode_to_config_string(ScaleMode mode) {
    switch (mode) {
    case SCALEMODE_NEAREST:
        return "nearest";
    case SCALEMODE_LINEAR:
        return "linear";
    case SCALEMODE_SOFT_LINEAR:
        return "soft-linear";
    case SCALEMODE_SQUARE_PIXELS:
        return "square-pixels";
    case SCALEMODE_INTEGER:
        return "integer";
    case SCALEMODE_PIXEL_ART:
        return "pixel-art";
    default:
        return "nearest";
    }
}

/** @brief Parse a config-file string into a ScaleMode enum. */
ScaleMode config_string_to_scale_mode(const char* string) {
    if (SDL_strcmp(string, "nearest") == 0) {
        return SCALEMODE_NEAREST;
    }
    if (SDL_strcmp(string, "linear") == 0) {
        return SCALEMODE_LINEAR;
    }
    if (SDL_strcmp(string, "soft-linear") == 0) {
        return SCALEMODE_SOFT_LINEAR;
    }
    if (SDL_strcmp(string, "square-pixels") == 0) {
        return SCALEMODE_SQUARE_PIXELS;
    }
    if (SDL_strcmp(string, "integer") == 0) {
        return SCALEMODE_INTEGER;
    }
    if (SDL_strcmp(string, "pixel-art") == 0) {
        return SCALEMODE_PIXEL_ART;
    }
    return SCALEMODE_NEAREST;
}

/** @brief Advance to the next scale mode (wrapping) and persist to config. */
void cycle_scale_mode(void) {
    scale_mode = (scale_mode + 1) % SCALEMODE_COUNT;
    Config_SetString(CFG_KEY_SCALEMODE, scale_mode_to_config_string(scale_mode));
    SDLApp_MarkBezelDirty(); /* Viewport changed, recalculate bezel positions */
    SDL_Log("Scale mode: %s", scale_mode_name());
}

/* ── Geometry utilities ────────────────────────────────────────────────── */

/** @brief Center an SDL_FRect within the window. */
static void center_rect(SDL_FRect* rect, int win_w, int win_h) {
    rect->x = (win_w - rect->w) / 2;
    rect->y = (win_h - rect->h) / 2;
}

/** @brief Compute the largest 4:3 rectangle that fits the window (letterboxed). */
static SDL_FRect fit_4_by_3_rect(int win_w, int win_h) {
    SDL_FRect rect;
    rect.w = win_w;
    rect.h = win_w / display_target_ratio;

    if (rect.h > win_h) {
        rect.h = win_h;
        rect.w = win_h * display_target_ratio;
    }

    center_rect(&rect, win_w, win_h);
    return rect;
}

/** @brief Compute the largest integer-scaled rectangle for pixel-perfect display. */
static SDL_FRect fit_integer_rect(int win_w, int win_h, int pixel_w, int pixel_h) {
    SDL_FRect rect;

    /* Try pixel-ratio-aware integer scale first (true pixel-perfect at target ratio) */
    int scale_w = win_w / (384 * pixel_w);
    int scale_h = win_h / (224 * pixel_h);
    int scale = (scale_h < scale_w) ? scale_h : scale_w;

    if (scale >= 1) {
        /* Pixel-perfect at the target pixel ratio */
        rect.w = (float)(scale * 384 * pixel_w);
        rect.h = (float)(scale * 224 * pixel_h);
    } else {
        /* Window too small for full pixel-ratio scaling.
         * Fall back to integer-scaling the base 384x224 resolution,
         * then apply the pixel aspect ratio via the output rect dimensions. */
        scale_w = win_w / 384;
        scale_h = win_h / 224;
        scale = (scale_h < scale_w) ? scale_h : scale_w;
        if (scale < 1)
            scale = 1;

        int base_w = scale * 384;
        int base_h = scale * 224;

        /* Apply pixel aspect ratio and fit within window */
        float aspect = (float)(base_w * pixel_w) / (float)(base_h * pixel_h);
        if ((float)base_w / (float)win_w > (float)base_h / (float)win_h) {
            /* Width-constrained */
            rect.w = (float)base_w;
            rect.h = (float)base_w / aspect;
        } else {
            /* Height-constrained */
            rect.h = (float)base_h;
            rect.w = (float)base_h * aspect;
        }

        /* Clamp to window bounds */
        if (rect.w > win_w) {
            float s = (float)win_w / rect.w;
            rect.w = (float)win_w;
            rect.h *= s;
        }
        if (rect.h > win_h) {
            float s = (float)win_h / rect.h;
            rect.h = (float)win_h;
            rect.w *= s;
        }
    }

    center_rect(&rect, win_w, win_h);
    return rect;
}

/** @brief Get the current letterbox/viewport rectangle based on scale mode. */
SDL_FRect get_letterbox_rect(int win_w, int win_h) {
    /* ⚡ Return cached result if inputs haven't changed */
    if (win_w == cached_lb_win_w && win_h == cached_lb_win_h && scale_mode == cached_lb_scale_mode) {
        return cached_letterbox_rect;
    }

    SDL_FRect result;
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
    case SCALEMODE_LINEAR:
    case SCALEMODE_SOFT_LINEAR:
        result = fit_4_by_3_rect(win_w, win_h);
        break;

    case SCALEMODE_INTEGER:
        /* In order to scale a 384x224 buffer to 4:3 we need to stretch the image vertically by 9 / 7 */
        result = fit_integer_rect(win_w, win_h, 7, 9);
        break;

    case SCALEMODE_PIXEL_ART:
        result = fit_4_by_3_rect(win_w, win_h);
        break;

    case SCALEMODE_SQUARE_PIXELS:
        result = fit_integer_rect(win_w, win_h, 1, 1);
        break;

    case SCALEMODE_COUNT:
    default:
        result = fit_4_by_3_rect(win_w, win_h);
        break;
    }

    /* Update cache */
    cached_lb_win_w = win_w;
    cached_lb_win_h = win_h;
    cached_lb_scale_mode = scale_mode;
    cached_letterbox_rect = result;
    return result;
}
