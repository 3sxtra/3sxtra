/**
 * @file sdl_app_debug_hud.c
 * @brief Debug HUD implementation: FPS measurement, history, and on-screen overlay.
 *
 * Contains the frame-end-time ring buffer, rolling FPS computation, unbounded
 * FPS history buffer, and text rendering for the debug HUD on all backends.
 */
#include "port/sdl/app/sdl_app_debug_hud.h"

#include "port/config/config.h"
#include "port/rendering/sdl_bezel.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_scale.h"
#include "port/sdl/app/sdl_app_shader_config.h"
#include "port/sdl/renderer/sdl_text_renderer.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Frame-time ring buffer ─────────────────────────────────────────── */

#define FRAME_END_TIMES_MAX 30

static Uint64 frame_end_times[FRAME_END_TIMES_MAX];
static int frame_end_times_index = 0;
static bool frame_end_times_filled = false;
static double fps = 0;

/* ── FPS history (unbounded, grows since game start) ────────────────── */

static float* fps_history = NULL;
static int fps_history_count = 0;
static int fps_history_capacity = 0;

/* ── Visibility flag ────────────────────────────────────────────────── */

bool show_debug_hud = false;

/* ── Public API ─────────────────────────────────────────────────────── */

void SDLAppDebugHud_NoteFrameEnd(void) {
    frame_end_times[frame_end_times_index] = SDL_GetTicksNS();
    frame_end_times_index += 1;
    frame_end_times_index %= FRAME_END_TIMES_MAX;

    if (frame_end_times_index == 0) {
        frame_end_times_filled = true;
    }
}

void SDLAppDebugHud_UpdateFPS(void) {
    if (!frame_end_times_filled) {
        return;
    }

    double total_frame_time_ms = 0;

    for (int i = 0; i < FRAME_END_TIMES_MAX - 1; i++) {
        const int cur = (frame_end_times_index + i) % FRAME_END_TIMES_MAX;
        const int next = (cur + 1) % FRAME_END_TIMES_MAX;
        total_frame_time_ms += (double)(frame_end_times[next] - frame_end_times[cur]) / 1e6;
    }

    double average_frame_time_ms = total_frame_time_ms / (FRAME_END_TIMES_MAX - 1);
    fps = 1000 / average_frame_time_ms;

    // Push into FPS history (grows dynamically)
    if (fps_history_count >= fps_history_capacity) {
        fps_history_capacity = fps_history_capacity ? fps_history_capacity * 2 : 1024;
        fps_history = (float*)realloc(fps_history, fps_history_capacity * sizeof(float));
    }
    fps_history[fps_history_count++] = (float)fps;
}

void SDLAppDebugHud_Render(int win_w, int win_h, const SDL_FRect* viewport) {
    char debug_text[512];
    char fps_text[64];
    char mode_text[128];
    char shader_text[128];

    snprintf(fps_text, sizeof(fps_text), "FPS: %.2f%s", fps, SDLApp_IsFrameRateUncapped() ? " UNCAPPED [F5]" : "");

    if (SDLAppShader_IsLibretroMode()) {
        if (SDLAppShader_GetAvailableCount() > 0) {
            snprintf(mode_text,
                     sizeof(mode_text),
                     "Preset: %s [F9]",
                     SDLAppShader_GetPresetName(SDLAppShader_GetCurrentIndex()));
        } else {
            snprintf(mode_text, sizeof(mode_text), "Preset: None found");
        }
    } else {
        snprintf(mode_text,
                 sizeof(mode_text),
                 "Scale: %s [F8]%s",
                 scale_mode_name(),
                 BezelSystem_IsVisible() ? " (Bezels On)" : "");
    }

    snprintf(shader_text,
             sizeof(shader_text),
             "Shader Mode: %s [F4]",
             SDLAppShader_IsLibretroMode() ? "Libretro" : "Internal");

    snprintf(debug_text, sizeof(debug_text), "%s | %s | %s", fps_text, shader_text, mode_text);

    float overlay_scale = ((float)win_h / 480.0f) * 0.8f;
    float base_x = viewport->x + (10.0f * overlay_scale);
    float base_y = 0.0f;

    SDLTextRenderer_SetBackgroundEnabled(1);
    SDLTextRenderer_SetBackgroundColor(0.0f, 0.0f, 0.0f, 0.5f);

    SDLTextRenderer_DrawText(debug_text, base_x + 1, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, win_w, win_h);
    SDLTextRenderer_DrawText(debug_text, base_x, base_y, overlay_scale, 1.0f, 1.0f, 1.0f, win_w, win_h);

    SDLTextRenderer_SetBackgroundEnabled(0);
}

void SDLAppDebugHud_RenderSDL2D(int win_w, int win_h, const SDL_FRect* dst_rect) {
    char debug_text[64];
    snprintf(debug_text, sizeof(debug_text), "FPS: %.2f", fps);
    float overlay_scale = (float)win_h / 480.0f;
    float base_x = dst_rect->x + (10.0f * overlay_scale);
    float base_y = dst_rect->y + (2.0f * overlay_scale);

    // ⚡ 4-direction shadow (cardinal only) — reduced from 8 to save draw calls
    SDLTextRenderer_DrawText(
        debug_text, base_x, base_y - 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
    SDLTextRenderer_DrawText(
        debug_text, base_x - 1, base_y, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
    SDLTextRenderer_DrawText(
        debug_text, base_x + 1, base_y, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
    SDLTextRenderer_DrawText(
        debug_text, base_x, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);

    SDLTextRenderer_DrawText(debug_text, base_x, base_y, overlay_scale, 1.0f, 1.0f, 1.0f, (float)win_w, (float)win_h);
    SDLTextRenderer_Flush();
}

double SDLAppDebugHud_GetFPS(void) {
    return fps;
}

const float* SDLAppDebugHud_GetFPSHistory(int* out_count) {
    if (out_count) {
        *out_count = fps_history_count;
    }
    return fps_history;
}

bool SDLAppDebugHud_IsVisible(void) {
    return show_debug_hud;
}

void SDLAppDebugHud_Toggle(void) {
    show_debug_hud = !show_debug_hud;
    Config_SetBool(CFG_KEY_DEBUG_HUD, show_debug_hud);
    SDL_Log("Debug HUD %s", show_debug_hud ? "ON" : "OFF");
}
