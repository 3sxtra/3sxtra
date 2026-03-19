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
#include <string.h>

/* ================================================================
 * Plugin state
 * ================================================================ */

static const renderer_import_t* g_import = NULL;
static char g_sprites_path[512] = { 0 };

/* ================================================================
 * Full-sprite override cache
 * ================================================================ */

#define FULL_SPRITE_CACHE_MAX 4096
#define FULL_SPRITE_CACHE_MASK (FULL_SPRITE_CACHE_MAX - 1)

typedef struct {
    uint32_t key;
    void* texture;
    bool checked;
} FullSpriteCacheEntry;

static FullSpriteCacheEntry full_sprite_cache[FULL_SPRITE_CACHE_MAX] = { { 0 } };
static int full_sprite_cache_count = 0;

static void hd_ClearSpriteCache(void) {
    for (int i = 0; i < FULL_SPRITE_CACHE_MAX; i++) {
        if (full_sprite_cache[i].checked && full_sprite_cache[i].texture != NULL) {
            g_import->TextureFree(full_sprite_cache[i].texture);
        }
        full_sprite_cache[i].checked = false;
        full_sprite_cache[i].texture = NULL;
    }
    full_sprite_cache_count = 0;
}

static void* hd_LoadFullSpriteOverride(int group_index, int cg_number) {
    if (g_sprites_path[0] == '\0')
        return NULL;
    const uint32_t key = (uint32_t)(group_index << 16) | (uint32_t)(cg_number & 0xFFFF);
    uint32_t slot = key & FULL_SPRITE_CACHE_MASK;

    for (int i = 0; i < 32; i++) {
        FullSpriteCacheEntry* entry = &full_sprite_cache[slot];

        if (!entry->checked) {
            break;
        }

        if (entry->key == key) {
            return entry->texture;
        }

        slot = (slot + 1) & FULL_SPRITE_CACHE_MASK;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/sprite_%d_%d.png", g_sprites_path, group_index, cg_number);
    void* tex = g_import->TextureLoad(path);

    if (tex == NULL) {
        snprintf(path, sizeof(path), "%s/sprite_%d.png", g_sprites_path, cg_number);
        tex = g_import->TextureLoad(path);
    }

    /* Cache the result (even NULL = negative cache) */
    if (full_sprite_cache_count < FULL_SPRITE_CACHE_MAX / 2) {
        slot = key & FULL_SPRITE_CACHE_MASK;

        for (int i = 0; i < 32; i++) {
            FullSpriteCacheEntry* entry = &full_sprite_cache[slot];

            if (!entry->checked) {
                entry->key = key;
                entry->texture = tex;
                entry->checked = true;
                full_sprite_cache_count++;
                return tex;
            }

            slot = (slot + 1) & FULL_SPRITE_CACHE_MASK;
        }
    }

    /* Cache is full and no free slot found — free the loaded texture to
     * prevent a leak (it would be re-loaded next frame, but at least
     * we don't leak the handle). */
    if (tex != NULL) {
        g_import->TextureFree(tex);
    }
    return NULL;
}

/* ================================================================
 * TryRenderSprite — load override + push to render queue in one call
 *
 * Coordinates are in CPS3 screen space (384×224).
 * TextureUtil_DrawQuadEx handles backend dispatch and any
 * necessary upscaling internally.
 * ================================================================ */

static bool hd_TryRenderSprite(int group_index, int cg_number, float screen_x, float screen_y, float z, int flip_x,
                               unsigned int color) {
    (void)color; /* TODO: tint support */
    void* texture = hd_LoadFullSpriteOverride(group_index, cg_number);
    if (texture == NULL)
        return false;

    int tex_w = 0, tex_h = 0;
    g_import->TextureGetSize(texture, &tex_w, &tex_h);

    /* Draw at the transformed screen-space position.
     * The texture's native size serves as the quad dimensions —
     * the backend will render it at pixel resolution. */
    g_import->TextureDrawQuadEx(
        texture, screen_x, screen_y,
        (float)tex_w, (float)tex_h,
        g_import->ConvScreenFZ(z),
        flip_x, 0);

    return true;
}

/* ================================================================
 * Background tile cache
 * ================================================================ */

#define BG_TILE_CACHE_MAX 1024
#define BG_TILE_CACHE_MASK (BG_TILE_CACHE_MAX - 1)

typedef struct {
    int gbix;
    void* texture;
    bool checked;
} BGTileCacheEntry;

static BGTileCacheEntry bg_tile_cache[BG_TILE_CACHE_MAX] = { { 0 } };

static void hd_ClearBGTileCache(void) {
    for (int i = 0; i < BG_TILE_CACHE_MAX; i++) {
        if (bg_tile_cache[i].checked && bg_tile_cache[i].texture != NULL) {
            g_import->TextureFree(bg_tile_cache[i].texture);
        }
        bg_tile_cache[i].checked = false;
        bg_tile_cache[i].texture = NULL;
    }
}

static void* hd_LoadBGTileOverride(int gbix) {
    uint32_t slot = (uint32_t)gbix & BG_TILE_CACHE_MASK;

    for (int i = 0; i < 16; i++) {
        BGTileCacheEntry* entry = &bg_tile_cache[slot];

        if (!entry->checked)
            break;
        if (entry->gbix == gbix)
            return entry->texture;
        slot = (slot + 1) & BG_TILE_CACHE_MASK;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/bg_%d.png", g_sprites_path, gbix);
    void* tex = g_import->TextureLoad(path);

    slot = (uint32_t)gbix & BG_TILE_CACHE_MASK;

    for (int i = 0; i < 16; i++) {
        BGTileCacheEntry* entry = &bg_tile_cache[slot];

        if (!entry->checked) {
            entry->gbix = gbix;
            entry->texture = tex;
            entry->checked = true;
            break;
        }

        slot = (slot + 1) & BG_TILE_CACHE_MASK;
    }

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
 * Lifecycle
 * ================================================================ */

static bool hd_Init(int argc, const char** argv) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--sprites-path") == 0) {
            snprintf(g_sprites_path, sizeof(g_sprites_path), "%s", argv[i + 1]);
            break;
        }
    }

    if (g_sprites_path[0] == '\0') {
        g_import->Log("No --sprites-path specified");
        return false;
    }

    g_import->Log("Renderer HD plugin initialized with path: %s", g_sprites_path);
    return true;
}

static void hd_Shutdown(void) {
    hd_ClearBGTileCache();
    hd_ClearSpriteCache();
    g_sprites_path[0] = '\0';
}

/* ================================================================
 * Export table
 * ================================================================ */

static renderer_export_t g_exports = {
    .api_version = RENDERER_PLUGIN_API_VERSION,
    .Init = hd_Init,
    .Shutdown = hd_Shutdown,
    .render_scale = 4,
    .TryRenderSprite = hd_TryRenderSprite,
    .LoadBGTileOverride = hd_LoadBGTileOverride,
    .DrawBGTile = hd_DrawBGTile,
    .ClearBGTileCache = hd_ClearBGTileCache,
    .ClearSpriteCache = hd_ClearSpriteCache,
};

/* ================================================================
 * DLL entry point
 * ================================================================ */

EXPORT renderer_export_t* GetRendererAPI(const renderer_import_t* import) {
    g_import = import;
    return &g_exports;
}
