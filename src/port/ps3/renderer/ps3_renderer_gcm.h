#ifndef PS3_RENDERER_GCM_H
#define PS3_RENDERER_GCM_H

#include "rendering/game_renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include <cell/gcm.h>
#include <stdbool.h>
#include <stdint.h>

#define RENDER_TASK_MAX 8192
#define CELL_GCM_MAX_TEXTURES 256
#define CELL_GCM_MAX_PALETTES 1088

typedef struct {
    float x, y, z;
    float u, v;
    unsigned int color; // RGBA
} GcmVertex;

typedef struct {
    uint32_t texture_handle;
    int vertex_offset;
    float z;
    int original_index;
    int index;
} GcmRenderTask;

// The CRS_Renderer_* API is implemented in ps3_renderer_gcm.c
// No longer need PS3_Renderer_* prefixes for the external API.

#endif // PS3_RENDERER_GCM_H
