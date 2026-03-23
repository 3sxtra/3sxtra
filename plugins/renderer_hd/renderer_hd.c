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
    void* tex = g_import->TextureLoadScaled(path, g_sprite_ratio);

    if (tex == NULL) {
        snprintf(path, sizeof(path), "%s/sprite_%d.png", g_sprites_path, cg_number);
        tex = g_import->TextureLoadScaled(path, g_sprite_ratio);
    }

    /* Debug: log first few attempts */
    if (full_sprite_cache_count < 5) {
        g_import->Log("SpriteOverride: g=%d cg=%d path='%s' tex=%p ratio=%.2f",
                      group_index, cg_number, path, tex, g_sprite_ratio);
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

static void* hd_LoadBGTileOverride(int type, int stage, int gbix) {
    int composite_key = type * 100000 + stage * 1000 + gbix;
    uint32_t slot = (uint32_t)composite_key & BG_TILE_CACHE_MASK;

    for (int i = 0; i < 16; i++) {
        BGTileCacheEntry* entry = &bg_tile_cache[slot];

        if (!entry->checked)
            break;
        if (entry->gbix == composite_key)
            return entry->texture;
        slot = (slot + 1) & BG_TILE_CACHE_MASK;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/bg_%d.png", g_sprites_path, composite_key);
    void* tex = g_import->TextureLoadScaled(path, g_sprite_ratio);

    slot = (uint32_t)composite_key & BG_TILE_CACHE_MASK;

    for (int i = 0; i < 16; i++) {
        BGTileCacheEntry* entry = &bg_tile_cache[slot];

        if (!entry->checked) {
            entry->gbix = composite_key;
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
 * Texture override cache (handle-based)
 *
 * Keyed by (texture_handle << 16 | palette_handle).
 * Loaded at native resolution (premultiplied) via TextureLoad.
 * ================================================================ */

#define TEX_OVERRIDE_CACHE_MAX 256
#define TEX_OVERRIDE_CACHE_MASK (TEX_OVERRIDE_CACHE_MAX - 1)

typedef struct {
    uint32_t key;
    void* texture;
    bool checked;
} TexOverrideCacheEntry;

static TexOverrideCacheEntry tex_override_cache[TEX_OVERRIDE_CACHE_MAX] = { { 0 } };

static void hd_ClearTextureOverrideCache(void) {
    for (int i = 0; i < TEX_OVERRIDE_CACHE_MAX; i++) {
        if (tex_override_cache[i].checked && tex_override_cache[i].texture != NULL) {
            g_import->TextureFree(tex_override_cache[i].texture);
        }
        tex_override_cache[i].checked = false;
        tex_override_cache[i].texture = NULL;
    }
}

static void* hd_TryOverrideTexture(unsigned int texture_handle, unsigned int palette_handle) {
    if (g_sprites_path[0] == '\0')
        return NULL;

    const uint32_t key = (texture_handle << 16) | (palette_handle & 0xFFFF);
    uint32_t slot = key & TEX_OVERRIDE_CACHE_MASK;

    /* Probe cache */
    for (int i = 0; i < 16; i++) {
        TexOverrideCacheEntry* entry = &tex_override_cache[slot];
        if (!entry->checked)
            break;
        if (entry->key == key)
            return entry->texture; /* hit (may be NULL = negative cache) */
        slot = (slot + 1) & TEX_OVERRIDE_CACHE_MASK;
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

    /* Store result (including NULL for negative cache) */
    slot = key & TEX_OVERRIDE_CACHE_MASK;
    for (int i = 0; i < 16; i++) {
        TexOverrideCacheEntry* entry = &tex_override_cache[slot];
        if (!entry->checked) {
            entry->key = key;
            entry->texture = tex;
            entry->checked = true;
            break;
        }
        slot = (slot + 1) & TEX_OVERRIDE_CACHE_MASK;
    }

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
