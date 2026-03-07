/**
 * @file stage_config.c
 * @brief Stage configuration INI file loader/saver.
 *
 * Reads and writes per-stage configuration files (parallax layers,
 * scroll speeds, visibility flags) from INI-format files in the
 * resources directory. Part of the stage modding system.
 */
#include "port/mods/stage_config.h"
#include "port/config/paths.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#endif

#ifdef _WIN32
#define strdup _strdup
#define strcasecmp _stricmp
#endif

StageConfig g_stage_config;

// Default parallax multiplier applied ON TOP of bg_prm (which already contains
// engine parallax from the msp speed table). 1.0 = pass-through / no extra scaling.
static const float DEFAULT_PARALLAX[] = { 1.0f, 1.0f, 1.0f, 1.0f };

void StageConfig_SetDefaultLayer(int i) {
    if (i < 0 || i >= MAX_STAGE_LAYERS)
        return;

    StageLayerConfig* layer = &g_stage_config.layers[i];
    snprintf(layer->filename, sizeof(layer->filename), "layer_%d.png", i);
    layer->enabled = true;
    layer->scale_mode = SCALE_MODE_FIT_HEIGHT;
    layer->scale_factor_x = 1.0f;
    layer->scale_factor_y = 1.0f;
    layer->parallax_x = (i < 4) ? DEFAULT_PARALLAX[i] : 1.0f;
    layer->parallax_y = 1.0f; // Native usually tracks Y 1:1 or close to it
    layer->offset_x = 0.0f;
    layer->offset_y = 0.0f;
    layer->original_bg_index = -1; // Default to auto/none, will be resolved in Load
    layer->z_index = i * 10;
}

void StageConfig_Init(void) {
    memset(&g_stage_config, 0, sizeof(StageConfig));
    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        StageConfig_SetDefaultLayer(i);
    }
    g_stage_config.is_custom = false;
}

static void trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return; // All spaces
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
}

void StageConfig_Load(int stage_index) {
    // 1. Reset to defaults first
    StageConfig_Init();

    // 2. Intelligent Defaults: determine the foreground BGW index for parallax.
    //    use_real_scr[] tells us how many real scroll layers this stage has.
    //    stage_bgw_number[][0..2] lists which bgw[] slots are active.
    //
    //    Rule:  1 layer  → foreground is stage_bgw_number[stage][0]
    //           2+ layers → foreground is stage_bgw_number[stage][1]  (msp = 1.0x)
    int foreground_bgw = -1;

    if (stage_index >= 0 && stage_index < 22) {
        int real_layers = use_real_scr[stage_index];
        if (real_layers >= 2) {
            foreground_bgw = stage_bgw_number[stage_index][1];
        } else if (real_layers == 1) {
            foreground_bgw = stage_bgw_number[stage_index][0];
        }
    }

    // Primary HD layer (layer 0) tracks the foreground for 1:1 camera scrolling.
    // Additional HD layers default to no reference (-1 = static).
    g_stage_config.layers[0].original_bg_index = foreground_bgw;

    // 3. Construct path
    const char* base = Paths_GetBasePath();
    char path[1024];
    snprintf(path, sizeof(path), "%sassets/stages/stage_%02d/stage_config.ini", base ? base : "", stage_index);

    FILE* f = fopen(path, "r");
    if (!f)
        return; // No config, stick to defaults

    g_stage_config.is_custom = true;

    char line[256];
    int current_layer = -1;

    while (fgets(line, sizeof(line), f)) {
        char* ptr = line;
        while (isspace((unsigned char)*ptr))
            ptr++;
        if (*ptr == ';' || *ptr == '#' || *ptr == '\0')
            continue;

        // Remove trailing newline
        line[strcspn(line, "\r\n")] = 0;

        // Section [layer_N]
        if (*ptr == '[') {
            char* end = strchr(ptr, ']');
            if (end) {
                *end = 0;
                if (strncmp(ptr + 1, "layer_", 6) == 0) {
                    current_layer = atoi(ptr + 7);
                    if (current_layer < 0 || current_layer >= MAX_STAGE_LAYERS)
                        current_layer = -1;
                }
            }
            continue;
        }

        if (current_layer == -1)
            continue;

        StageLayerConfig* layer = &g_stage_config.layers[current_layer];

        char* eq = strchr(ptr, '=');
        if (eq) {
            *eq = 0;
            char* key = ptr;
            char* val = eq + 1;

            trim_whitespace(key);
            trim_whitespace(val);

            if (strcasecmp(key, "filename") == 0) {
                // remove quotes if present
                if (*val == '"') {
                    val++;
                    val[strlen(val) - 1] = 0;
                }
                strncpy(layer->filename, val, sizeof(layer->filename) - 1);
            } else if (strcasecmp(key, "enabled") == 0)
                layer->enabled = (atoi(val) != 0 || strcasecmp(val, "true") == 0);
            else if (strcasecmp(key, "scale_mode") == 0) {
                if (strcasecmp(val, "fit_height") == 0)
                    layer->scale_mode = SCALE_MODE_FIT_HEIGHT;
                else if (strcasecmp(val, "stretch") == 0)
                    layer->scale_mode = SCALE_MODE_STRETCH;
                else if (strcasecmp(val, "native") == 0)
                    layer->scale_mode = SCALE_MODE_NATIVE;
                else if (strcasecmp(val, "manual") == 0)
                    layer->scale_mode = SCALE_MODE_MANUAL;
            } else if (strcasecmp(key, "scale_x") == 0)
                layer->scale_factor_x = (float)atof(val);
            else if (strcasecmp(key, "scale_y") == 0)
                layer->scale_factor_y = (float)atof(val);
            else if (strcasecmp(key, "parallax_x") == 0)
                layer->parallax_x = (float)atof(val);
            else if (strcasecmp(key, "parallax_y") == 0)
                layer->parallax_y = (float)atof(val);
            else if (strcasecmp(key, "offset_x") == 0)
                layer->offset_x = (float)atof(val);
            else if (strcasecmp(key, "offset_y") == 0)
                layer->offset_y = (float)atof(val);
            else if (strcasecmp(key, "original_bg_index") == 0)
                layer->original_bg_index = atoi(val);
            else if (strcasecmp(key, "z_index") == 0)
                layer->z_index = atoi(val);
        }
    }

    fclose(f);
}

void StageConfig_Save(int stage_index) {
    const char* base = Paths_GetBasePath();
    char path[1024];
    snprintf(path, sizeof(path), "%sassets/stages/stage_%02d/stage_config.ini", base ? base : "", stage_index);

    FILE* f = fopen(path, "w");
    if (!f)
        return;

    fprintf(f, "; HD Stage Configuration for Stage %02d\n", stage_index);
    fprintf(f, "; Modes: fit_height, stretch, native, manual\n\n");

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        StageLayerConfig* layer = &g_stage_config.layers[i];
        if (!layer->enabled && i > 0)
            continue; // Skip unused upper layers if disabled

        fprintf(f, "[layer_%d]\n", i);
        fprintf(f, "filename=%s\n", layer->filename);
        fprintf(f, "enabled=%s\n", layer->enabled ? "true" : "false");

        const char* mode_str = "fit_height";
        switch (layer->scale_mode) {
        case SCALE_MODE_STRETCH:
            mode_str = "stretch";
            break;
        case SCALE_MODE_NATIVE:
            mode_str = "native";
            break;
        case SCALE_MODE_MANUAL:
            mode_str = "manual";
            break;
        default:
            break;
        }
        fprintf(f, "scale_mode=%s\n", mode_str);

        fprintf(f, "scale_x=%.3f\n", layer->scale_factor_x);
        fprintf(f, "scale_y=%.3f\n", layer->scale_factor_y);
        fprintf(f, "parallax_x=%.3f\n", layer->parallax_x);
        fprintf(f, "parallax_y=%.3f\n", layer->parallax_y);
        fprintf(f, "offset_x=%.1f\n", layer->offset_x);
        fprintf(f, "offset_y=%.1f\n", layer->offset_y);
        fprintf(f, "original_bg_index=%d\n", layer->original_bg_index);
        fprintf(f, "z_index=%d\n\n", layer->z_index);
    }

    fclose(f);
}
