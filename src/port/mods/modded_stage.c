/**
 * @file modded_stage.c
 * @brief HD multi-layer parallax stage background replacement.
 *
 * Loads PNG layers from assets/stages/stage_XX/ and renders them at native
 * screen resolution into the default framebuffer (backbuffer). The game's
 * 384×224 canvas FBO is then composited on top with blending, so sprites
 * appear over the HD background without any downscaling.
 *
 * All scroll/positioning data is read directly from the live g_state.bg_w engine
 * struct — this system owns zero gameplay state and is purely cosmetic.
 */
#include "port/mods/modded_stage.h"
#include "game_state.h"
#include "port/config/config.h"
#include "port/config/paths.h"
#include "port/mods/stage_config.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Total number of stages the engine supports */
#define MODDED_STAGE_COUNT 22

/* Number of BGW entries in the engine's BG struct (bgw[7]) */
#define BGW_ARRAY_SIZE 7

typedef struct {
    void* texture; /* GL texture g_state.ID */
    int width;
    int height;
} ModdedLayerResources;

/* Module state */
static bool s_enabled = false;
static bool s_rendering_disabled = false;
static bool s_animations_disabled = false;
static int s_loaded_stage = -1;
static ModdedLayerResources s_layer_res[MAX_STAGE_LAYERS];
static int s_layer_res_count = 0;

/* ---------- Lifecycle ---------- */

void ModdedStage_Init(void) {
    s_enabled = Config_HasKey(CFG_KEY_HD_STAGES) ? Config_GetBool(CFG_KEY_HD_STAGES) : true;
    s_rendering_disabled = false;
    s_animations_disabled = false;
    s_loaded_stage = -1;
    s_layer_res_count = 0;
    memset(s_layer_res, 0, sizeof(s_layer_res));
    StageConfig_Init();
}

void ModdedStage_Shutdown(void) {
    ModdedStage_Unload();
}

void ModdedStage_SetEnabled(bool enabled) {
    s_enabled = enabled;
}

bool ModdedStage_IsEnabled(void) {
    return s_enabled;
}

void ModdedStage_SetDisableRendering(bool disabled) {
    s_rendering_disabled = disabled;
}

bool ModdedStage_IsRenderingDisabled(void) {
    return s_rendering_disabled;
}

void ModdedStage_SetAnimationsDisabled(bool disabled) {
    s_animations_disabled = disabled;
}

bool ModdedStage_IsAnimationsDisabled(void) {
    return s_animations_disabled;
}

/* ---------- Asset Loading ---------- */

void ModdedStage_LoadForStage(int stage_index) {
    /* Don't reload if already loaded for this stage */
    if (s_loaded_stage == stage_index && s_layer_res_count > 0) {
        return;
    }

    ModdedStage_Unload();

    if (stage_index < 0 || stage_index >= MODDED_STAGE_COUNT) {
        return;
    }

    /* Load Configuration */
    StageConfig_Load(stage_index);

    const char* base = Paths_GetBasePath();
    if (!base)
        base = "";

    char path[512];
    int loaded = 0;

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        StageLayerConfig* cfg = &g_stage_config.layers[i];
        if (!cfg->enabled)
            continue;

        snprintf(path, sizeof(path), "%sassets/stages/stage_%02d/%s", base, stage_index, cfg->filename);

        void* tex = TextureUtil_Load(path);
        if (tex == NULL) {
            /* If it's a critical layer (0), maybe warn? But config might specify empty layers. */
            SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "ModdedStage: Failed to load %s", path);
            continue;
        }

        s_layer_res[i].texture = tex;
        TextureUtil_GetSize(tex, &s_layer_res[i].width, &s_layer_res[i].height);
        loaded++;

        if (i >= s_layer_res_count)
            s_layer_res_count = i + 1;
    }

    if (loaded > 0) {
        s_loaded_stage = stage_index;
        SDL_LogDebug(
            SDL_LOG_CATEGORY_RENDER, "ModdedStage: Stage %d loaded with %d active layers", stage_index, loaded);
    }
}

void ModdedStage_Unload(void) {
    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        if (s_layer_res[i].texture) {
            TextureUtil_Free(s_layer_res[i].texture);
            s_layer_res[i].texture = NULL;
        }
    }
    s_layer_res_count = 0;
    s_loaded_stage = -1;
}

/* ---------- Query ---------- */

bool ModdedStage_IsActiveForCurrentStage(void) {
    return s_enabled && s_layer_res_count > 0 && s_loaded_stage == g_state.bg_w.stage;
}

int ModdedStage_GetLayerCount(void) {
    return s_layer_res_count; // Approximation
}

int ModdedStage_GetLoadedStageIndex(void) {
    return s_loaded_stage;
}

static void draw_layer(int layer_index, const BackgroundParameters* local_bg_prm, const BG* bg) {
    if (layer_index < 0 || layer_index >= MAX_STAGE_LAYERS)
        return;

    StageLayerConfig* cfg = &g_stage_config.layers[layer_index];
    if (!cfg->enabled)
        return;

    ModdedLayerResources* res = &s_layer_res[layer_index];
    if (!res->texture)
        return;

    float tex_w = (float)res->width;
    float tex_h = (float)res->height;

    /* Calculate scale */
    float effective_w, effective_h;

    if (cfg->scale_mode == SCALE_MODE_MANUAL) {
        effective_w = tex_w * cfg->scale_factor_x;
        effective_h = tex_h * cfg->scale_factor_y;
    } else if (cfg->scale_mode == SCALE_MODE_NATIVE) {
        effective_w = tex_w;
        effective_h = tex_h;
    } else if (cfg->scale_mode == SCALE_MODE_STRETCH) {
        // Stretch to fill viewport (simplistic default)
        effective_w = 384.0f;
        effective_h = 224.0f;
    } else {
        // FIT_HEIGHT (Default)
        float scale = tex_h / 512.0f;
        if (scale < 0.001f)
            scale = 1.0f;
        effective_w = tex_w / scale;
        effective_h = 512.0f;
    }

    /* Viewport size in game pixels */
    const float vp_w = 384.0f;
    const float vp_h = 224.0f;

    /* Normalize scroll position using the engine's live scroll limits.
     * bg_h_shift = wxy.pos - pos_offset (set in bg_pos_hosei).
     * wxy.pos is clamped to [l_limit2, r_limit2], so bg_h_shift ranges
     * from (l_limit2 - pos_offset) to (r_limit2 - pos_offset).
     * We remap this range to [0 .. effective_w - viewport_w].
     */
    int bg_idx = cfg->original_bg_index;
    float scroll_x, scroll_y;

    if (bg_idx >= 0 && bg_idx < BGW_ARRAY_SIZE && bg != NULL) {
        const BGW* bgw = &bg->bgw[bg_idx];
        float raw_x = (float)(s16)local_bg_prm[bg_idx].bg_h_shift;
        float raw_y = (float)(s16)local_bg_prm[bg_idx].bg_v_shift;

        /* bg_h_shift = wxy.pos - pos_offset  (see bg_pos_hosei / Irl_Scrn).
         * wxy.pos is clamped to [l_limit2, r_limit2] by bg_base_x_move_check.
         * So the runtime range of bg_h_shift is:
         *   min = l_limit2 - pos_offset
         *   max = r_limit2 - pos_offset
         * We map this range to [0 .. effective_w - vp_w] in HD-image space. */
        float native_range_x = (float)(bgw->r_limit2 - bgw->l_limit2);
        float y_lim = (float)bgw->y_limit2;

        float min_shift = (float)bgw->l_limit2 - (float)bg->pos_offset;
        float max_scroll_x = effective_w - vp_w;
        if (native_range_x > 0.001f && max_scroll_x > 0.0f) {
            float t = (raw_x - min_shift) / native_range_x;
            scroll_x = t * max_scroll_x;
        } else {
            /* No horizontal scrolling — center the image */
            scroll_x = max_scroll_x * 0.5f;
        }

        /* Vertical: map [0 .. y_limit2] -> [bottom .. top] of HD image.
         * At ground level (v_shift=0) we show the BOTTOM of the image.
         * As fighters jump (v_shift increases), the camera pans up showing more sky. */
        float max_scroll_y = effective_h - vp_h;
        if (y_lim > 0.001f) {
            float t = raw_y / y_lim;              /* 0.0 at ground, 1.0 at max jump height */
            scroll_y = max_scroll_y * (1.0f - t); /* invert: ground=bottom, sky=top */
        } else {
            scroll_y = max_scroll_y; /* no vertical scroll — show bottom */
        }

        /* Apply config multiplier and offset */
        scroll_x = scroll_x * cfg->parallax_x + cfg->offset_x;
        scroll_y = scroll_y * cfg->parallax_y + cfg->offset_y;
    } else {
        /* No BG reference — static with offset only */
        scroll_x = cfg->offset_x;
        scroll_y = cfg->offset_y;
    }

    float u0 = scroll_x / effective_w;
    float v0 = scroll_y / effective_h;
    float u1 = (scroll_x + vp_w) / effective_w;
    float v1 = (scroll_y + vp_h) / effective_h;

    /* Base depth for the modded stage - drawn way behind the character sprites */
    float z_base = 0.05f + (layer_index * 0.001f);

    TextureUtil_DrawSubQuadEx(res->texture, 0.0f, 0.0f, vp_w, vp_h, u0, v0, u1, v1, z_base);
}
void ModdedStage_Render(const BG* bg) {
    if (!bg || s_layer_res_count == 0)
        return;

    /* Sort layers by Z-Index from Config */
    // We can create a lightweight index array
    int order[MAX_STAGE_LAYERS];
    int count = 0;

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        if (g_stage_config.layers[i].enabled && s_layer_res[i].texture) {
            order[count++] = i;
        }
    }

    // Create a simple mapping to help insertion sort
    struct SortNode {
        int index;
        int z;
    } sort_buf[MAX_STAGE_LAYERS];

    for (int i = 0; i < count; i++) {
        sort_buf[i].index = order[i];
        sort_buf[i].z = g_stage_config.layers[order[i]].z_index;
    }

    // Insertion sort by z_index
    for (int i = 1; i < count; i++) {
        struct SortNode key = sort_buf[i];
        int j = i - 1;
        while (j >= 0 && sort_buf[j].z > key.z) {
            sort_buf[j + 1] = sort_buf[j];
            j--;
        }
        sort_buf[j + 1] = key;
    }

    for (int i = 0; i < count; i++) {
        draw_layer(sort_buf[i].index, g_state.bg_prm, bg); // Pass global g_state.bg_prm array + BG struct
    }
}
