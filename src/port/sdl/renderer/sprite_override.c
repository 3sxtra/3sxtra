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

static void* s_sprite_cache[SPRITE_CACHE_SIZE];
static bool  s_sprite_miss[SPRITE_CACHE_SIZE];

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
    snprintf(path, sizeof(path), "assets/sprites/sprite%d_%d.png", group_index, cg_number);
    void* tex = TextureUtil_Load(path);

    if (tex == NULL) {
        /* Fall back to "{sprites}/sprite{cg}.png" */
        snprintf(path, sizeof(path), "assets/sprites/sprite%d.png", cg_number);
        tex = TextureUtil_Load(path);
    }

    if (tex != NULL) {
        s_sprite_cache[cg_number] = tex;
    } else {
        s_sprite_miss[cg_number] = true;
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
}
