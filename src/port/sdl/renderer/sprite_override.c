/**
 * @file sprite_override.c
 * @brief HD sprite override loader and cache.
 *
 * Loads high-resolution PNG images from assets/sprites/ and caches them
 * by CG number.  When a cached override exists, the normal CPS3 tiled
 * rendering is skipped and the single HD texture is drawn instead.
 */
#include "port/sdl/renderer/sprite_override.h"
#include "port/sdl/renderer/sdl_texture_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define SPRITE_CACHE_SIZE 65536
#define BG_TILE_CACHE_SIZE 4096

/* Cache arrays are global statics. This relies on the assumption that
 * all texture loading and rendering occurs strictly on the main thread
 * (which is standard for SDL rendering). No mutexes are required. */
static void* s_sprite_cache[SPRITE_CACHE_SIZE];
static bool s_sprite_miss[SPRITE_CACHE_SIZE];

static void* s_bg_tile_cache[BG_TILE_CACHE_SIZE];
static bool s_bg_tile_miss[BG_TILE_CACHE_SIZE];

void ClearBGTileCache(void) {
    /* Reset checked flags and explicitly free textures.
     * Since the stage has ended, it is safe to free previous stage textures.
     * This prevents memory leaks.
     */
    for (int i = 0; i < BG_TILE_CACHE_SIZE; i++) {
        if (s_bg_tile_cache[i] != NULL) {
            TextureUtil_Free(s_bg_tile_cache[i]);
            s_bg_tile_cache[i] = NULL;
        }
        s_bg_tile_miss[i] = false;
    }
}

void* LoadFullSpriteOverride(int group_index, int cg_number) {
    if (cg_number < 0 || cg_number >= SPRITE_CACHE_SIZE) {
        return NULL;
    }

    if (s_sprite_cache[cg_number] != NULL) {
        return s_sprite_cache[cg_number];
    }
    if (s_sprite_miss[cg_number]) {
        return NULL;
    }

    char path[1024];

    /* Try "{sprites}/sprite{group}_{cg}.png" first */
    snprintf(path, sizeof(path), "assets/sprites/sprite_%d_%d.png", group_index, cg_number);
    void* tex = TextureUtil_Load(path);

    if (tex == NULL) {
        /* Fall back to "{sprites}/sprite{cg}.png" */
        snprintf(path, sizeof(path), "assets/sprites/sprite_%d.png", cg_number);
        tex = TextureUtil_Load(path);
    }

    if (tex != NULL) {
        s_sprite_cache[cg_number] = tex;
    } else {
        s_sprite_miss[cg_number] = true;
    }

    return tex;
}

void* LoadBGTileOverride(int type, int stage, int gbix) {
    if (gbix < 0 || gbix >= BG_TILE_CACHE_SIZE) {
        return NULL;
    }

    if (s_bg_tile_cache[gbix] != NULL) {
        return s_bg_tile_cache[gbix];
    }
    if (s_bg_tile_miss[gbix]) {
        return NULL;
    }

    char path[1024];
    /* Format using the separated parameters to construct the unique composite key for the filename */
    int composite_key = type * 100000 + stage * 1000 + gbix;
    snprintf(path, sizeof(path), "assets/sprites/bg_%d.png", composite_key);

    void* tex = TextureUtil_Load(path);

    if (tex != NULL) {
        s_bg_tile_cache[gbix] = tex;
    } else {
        s_bg_tile_miss[gbix] = true;
    }

    return tex;
}

void SpriteOverride_Shutdown(void) {
    for (int i = 0; i < SPRITE_CACHE_SIZE; i++) {
        if (s_sprite_cache[i] != NULL) {
            TextureUtil_Free(s_sprite_cache[i]);
            s_sprite_cache[i] = NULL;
        }
        s_sprite_miss[i] = false;
    }
    for (int i = 0; i < BG_TILE_CACHE_SIZE; i++) {
        if (s_bg_tile_cache[i] != NULL) {
            TextureUtil_Free(s_bg_tile_cache[i]);
            s_bg_tile_cache[i] = NULL;
        }
        s_bg_tile_miss[i] = false;
    }
}
