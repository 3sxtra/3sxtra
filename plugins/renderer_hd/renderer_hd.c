/**
 * renderer_hd.dll — HD sprite/tile override renderer plugin for 3SX.
 *
 * Implements the renderer_export_t interface. Loads PNG sprite overrides
 * from a configurable directory and renders them at high resolution
 * using the host engine's TextureUtil API for cross-backend compatibility.
 *
 * Compatible with all rendering backends: OpenGL, SDL_GPU, SDL2D, SDL2D Classic.
 */

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

#include "port/renderer_plugin.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Plugin state
 * ================================================================ */

static const renderer_import_t* g_import = NULL;
static char g_sprites_path[512] = { 0 };
static int g_render_scale = 4;
static int g_sprite_scale = 4;
static float g_sprite_ratio = 1.0f; /* render_scale / sprite_scale */
static renderer_export_t g_exports;

/* ================================================================
 * Full-sprite override cache
 * ================================================================ */

#define STB_DS_IMPLEMENTATION
#include "stb/stb_ds.h"

typedef struct {
    uint32_t key;
    void* value;
} TexCacheMap;

static TexCacheMap* full_sprite_cache = NULL;

static void hd_ClearSpriteCache(void) {
    for (ptrdiff_t i = 0; i < hmlen(full_sprite_cache); i++) {
        if (full_sprite_cache[i].value != NULL) {
            g_import->TextureFree(full_sprite_cache[i].value);
        }
    }
    hmfree(full_sprite_cache);
    full_sprite_cache = NULL;
}

static void* hd_LoadFullSpriteOverride(int group_index, int cg_number) {
    if (g_sprites_path[0] == '\0')
        return NULL;
    const uint32_t key = (uint32_t)(group_index << 16) | (uint32_t)(cg_number & 0xFFFF);

    ptrdiff_t idx = hmgeti(full_sprite_cache, key);
    if (idx >= 0) {
        return full_sprite_cache[idx].value;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/sprite_%d_%d.png", g_sprites_path, group_index, cg_number);
    void* tex = g_import->TextureLoadScaled(path, g_sprite_ratio);

    if (tex == NULL) {
        snprintf(path, sizeof(path), "%s/sprite_%d.png", g_sprites_path, cg_number);
        tex = g_import->TextureLoadScaled(path, g_sprite_ratio);
    }

    hmput(full_sprite_cache, key, tex);
    return tex;
}

/* ================================================================
 * TryRenderSprite — load override + push to render queue in one call
 *
 * Coordinates are in CPS3 screen space (384×224).
 * TextureUtil_DrawQuadEx handles backend dispatch and any
 * necessary upscaling internally.
 * ================================================================ */

static bool hd_TryRenderSprite(int group_index, int cg_number, float screen_x, float screen_y, float z, int flip_x,
                               unsigned int color, float screen_w, float screen_h) {
    (void)color; /* TODO: tint support */
    void* texture = hd_LoadFullSpriteOverride(group_index, cg_number);
    if (texture == NULL)
        return false;

    /* Use the caller-provided CPS3-space bounding box dimensions,
     * not the HD texture's pixel dimensions. The backend handles
     * the actual pixel rendering at whatever resolution it uses. */
    g_import->TextureDrawQuadEx(
        texture, screen_x, screen_y,
        screen_w, screen_h,
        g_import->ConvScreenFZ(z),
        flip_x, 0);

    return true;
}

/* ================================================================
 * Background tile cache
 * ================================================================ */

static TexCacheMap* bg_tile_cache = NULL;

static void hd_ClearBGTileCache(void) {
    for (ptrdiff_t i = 0; i < hmlen(bg_tile_cache); i++) {
        if (bg_tile_cache[i].value != NULL) {
            g_import->TextureFree(bg_tile_cache[i].value);
        }
    }
    hmfree(bg_tile_cache);
    bg_tile_cache = NULL;
}

static void* hd_LoadBGTileOverride(int type, int stage, int gbix) {
    uint32_t key = (uint32_t)(type * 100000 + stage * 1000 + gbix);

    ptrdiff_t idx = hmgeti(bg_tile_cache, key);
    if (idx >= 0) {
        return bg_tile_cache[idx].value;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/bg_%u.png", g_sprites_path, key);
    void* tex = g_import->TextureLoadScaled(path, g_sprite_ratio);

    hmput(bg_tile_cache, key, tex);
    return tex;
}

/* DrawBGTile — receives explicit screen-space rectangle + z from host.
 * vtxCol is logged but not applied (TextureDrawQuadEx has no tint param yet). */
static void hd_DrawBGTile(void* texture, float x, float y, float w, float h, float z,
                          unsigned int vtxCol) {
    (void)vtxCol; /* TODO: apply per-tile color tint when TextureUtil supports it */
    g_import->TextureDrawQuadEx(texture, x, y, w, h, z, 0, 0);
}

/* ================================================================
 * Texture override cache (handle-based)
 *
 * Keyed by (texture_handle << 16 | palette_handle).
 * Loaded at native resolution (premultiplied) via TextureLoad.
 * ================================================================ */

static TexCacheMap* tex_override_cache = NULL;

static void hd_ClearTextureOverrideCache(void) {
    for (ptrdiff_t i = 0; i < hmlen(tex_override_cache); i++) {
        if (tex_override_cache[i].value != NULL) {
            g_import->TextureFree(tex_override_cache[i].value);
        }
    }
    hmfree(tex_override_cache);
    tex_override_cache = NULL;
}

static void* hd_TryOverrideTexture(unsigned int texture_handle, unsigned int palette_handle) {
    if (g_sprites_path[0] == '\0')
        return NULL;

    const uint32_t key = (texture_handle << 16) | (palette_handle & 0xFFFF);
    ptrdiff_t idx = hmgeti(tex_override_cache, key);
    if (idx >= 0) {
        return tex_override_cache[idx].value;
    }

    /* Cache miss — probe filesystem.
     * Premultiplied: use TextureLoad (native resolution, no scaling). */
    char path[512];
    void* tex = NULL;

    /* Try texture_{th}_{ph}.png (specific palette variant) */
    snprintf(path, sizeof(path), "%s/texture_%u_%u.png", g_sprites_path, texture_handle, palette_handle);
    tex = g_import->TextureLoad(path);

    /* Fallback: texture_{th}.png (palette-independent) */
    if (tex == NULL) {
        snprintf(path, sizeof(path), "%s/texture_%u.png", g_sprites_path, texture_handle);
        tex = g_import->TextureLoad(path);
    }

    hmput(tex_override_cache, key, tex);

    if (tex != NULL) {
        g_import->Log("Texture override loaded: %s (th=%u, ph=%u)", path, texture_handle, palette_handle);
    }

    return tex;
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

static bool hd_Init(int argc, const char** argv) {
    int render_scale = 4;
    int sprite_scale = 0; /* 0 = auto (match render_scale) */

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--sprites-path") == 0) {
            snprintf(g_sprites_path, sizeof(g_sprites_path), "%s", argv[i + 1]);
        } else if (strcmp(argv[i], "--render-scale") == 0) {
            render_scale = atoi(argv[i + 1]);
            if (render_scale < 1)
                render_scale = 1;
            if (render_scale > 8)
                render_scale = 8;
        } else if (strcmp(argv[i], "--sprite-scale") == 0) {
            sprite_scale = atoi(argv[i + 1]);
            if (sprite_scale < 1)
                sprite_scale = 1;
            if (sprite_scale > 8)
                sprite_scale = 8;
        }
    }

    /* Default to assets/sprites/ next to the executable */
    if (g_sprites_path[0] == '\0') {
        snprintf(g_sprites_path, sizeof(g_sprites_path), "assets/sprites");
        g_import->Log("Using default sprites path: %s", g_sprites_path);
    }

    if (sprite_scale == 0)
        sprite_scale = render_scale;

    g_render_scale = render_scale;
    g_sprite_scale = sprite_scale;
    g_sprite_ratio = (float)render_scale / (float)sprite_scale;
    g_exports.render_scale = render_scale;

    g_import->Log("Renderer HD plugin initialized (render_scale=%d, sprite_scale=%d, path=%s)",
                  render_scale, sprite_scale, g_sprites_path);
    return true;
}

static void hd_Shutdown(void) {
    hd_ClearBGTileCache();
    hd_ClearSpriteCache();
    hd_ClearTextureOverrideCache();
    g_sprites_path[0] = '\0';
}

/* ================================================================
 * DLL entry point
 * ================================================================ */

EXPORT renderer_export_t* GetRendererAPI(const renderer_import_t* import) {
    g_import = import;

    g_exports.api_version = RENDERER_PLUGIN_API_VERSION;
    g_exports.Init = hd_Init;
    g_exports.Shutdown = hd_Shutdown;
    g_exports.render_scale = 4; /* default; overridden in Init after parsing args */
    g_exports.TryRenderSprite = hd_TryRenderSprite;
    g_exports.LoadBGTileOverride = hd_LoadBGTileOverride;
    g_exports.DrawBGTile = hd_DrawBGTile;
    g_exports.ClearBGTileCache = hd_ClearBGTileCache;
    g_exports.ClearSpriteCache = hd_ClearSpriteCache;
    g_exports.TryOverrideTexture = hd_TryOverrideTexture;
    g_exports.ClearTextureOverrideCache = hd_ClearTextureOverrideCache;

    return &g_exports;
}
