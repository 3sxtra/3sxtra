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
#include "port/config/cli_parser.h"
#include "port/config/paths.h"
#include "port/rendering/renderer.h"

#include <SDL3/SDL.h>
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
static int s_sprite_group[SPRITE_CACHE_SIZE]; /* texture group recorded at miss time */

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

    /* Also clear sprite miss flags on stage transition so newly added assets are retried */
    SpriteOverride_ClearMissFlags();
}

void SpriteOverride_ClearMissFlags(void) {
    for (int i = 0; i < SPRITE_CACHE_SIZE; i++) {
        s_sprite_miss[i] = false;
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
        SDL_Log("[SpriteOverride] LOADED group=%d cg=%d path=%s", group_index, cg_number, path);
        s_sprite_cache[cg_number] = tex;
    } else {
        s_sprite_miss[cg_number] = true;
        s_sprite_group[cg_number] = group_index;
    }

    return tex;
}

void* LoadBGTileOverride(int type, int stage, int gbix) {
    /* Use a composite key for both filenames and cache indexing to avoid
     * collisions between stages/texture types that share the same gbix. */
    int composite_key = type * 100000 + (stage + 1) * 1000 + gbix;
    int slot = ((unsigned int)composite_key) % BG_TILE_CACHE_SIZE;

    if (s_bg_tile_cache[slot] != NULL) {
        return s_bg_tile_cache[slot];
    }
    if (s_bg_tile_miss[slot]) {
        return NULL;
    }

    char path[1024];
    snprintf(path, sizeof(path), "assets/sprites/bg_%d.png", composite_key);

    void* tex = TextureUtil_Load(path);

    if (tex != NULL) {
        s_bg_tile_cache[slot] = tex;
    } else {
        s_bg_tile_miss[slot] = true;
    }

    return tex;
}

/* ── HD UI texture page override ── */

#define UI_PAGE_CACHE_SIZE 64

typedef struct {
    unsigned int key; /* texHandle | (palHandle << 16), 0 = empty slot */
    void* texture;    /* TextureUtil handle, or NULL if miss */
    bool checked;
} UIPageCacheEntry;

static UIPageCacheEntry s_ui_page_cache[UI_PAGE_CACHE_SIZE];

void* LoadUIPageOverride(unsigned int tex_handle, unsigned int pal_handle) {
    unsigned int key = tex_handle | (pal_handle << 16);
    unsigned int slot = key % UI_PAGE_CACHE_SIZE;

    /* Linear probe (open addressing) */
    for (unsigned int p = 0; p < UI_PAGE_CACHE_SIZE; p++) {
        unsigned int idx = (slot + p) % UI_PAGE_CACHE_SIZE;
        UIPageCacheEntry* e = &s_ui_page_cache[idx];

        if (e->key == key && e->checked) {
            return e->texture; /* hit (may be NULL for miss) */
        }
        if (!e->checked && e->key == 0) {
            /* Empty slot — this key hasn't been probed yet */
            break;
        }
    }

    /* Cache miss — probe filesystem */
    void* tex = NULL;
    char path[256];

    /* Try page-index-based naming first: assets/ui/page_0.png, page_1.png, etc.
     * This matches the output of extract_ui_pages.py (offline extraction). */
    int ppg_idx = Renderer_GetCurrentPPGPageIndex();
    if (ppg_idx >= 0) {
        snprintf(path, sizeof(path), "assets/ui/page_%d.png", ppg_idx);
        tex = TextureUtil_Load(path);
    }

    /* Fall back to handle-based naming: page_{tex}_{pal}.png */
    if (tex == NULL) {
        snprintf(path, sizeof(path), "assets/ui/page_%u_%u.png", tex_handle, pal_handle);
        tex = TextureUtil_Load(path);
    }

    /* Store result */
    for (unsigned int p = 0; p < UI_PAGE_CACHE_SIZE; p++) {
        unsigned int idx = (slot + p) % UI_PAGE_CACHE_SIZE;
        UIPageCacheEntry* e = &s_ui_page_cache[idx];
        if (!e->checked || e->key == 0) {
            e->key = key;
            e->texture = tex;
            e->checked = true;
            break;
        }
    }

    if (tex != NULL) {
        SDL_Log(
            "SpriteOverride: loaded HD UI page: %s (ppg_idx=%d, th=%u, ph=%u)", path, ppg_idx, tex_handle, pal_handle);
    }

    return tex;
}

void SpriteOverride_DumpMissing(void) {
    if (!g_dump_missing_sprites) {
        return;
    }

    char csv_path[1024];
    const char* base = Paths_GetBasePath();
    snprintf(csv_path, sizeof(csv_path), "%smissing_sprites.csv", base ? base : "");

    FILE* f = fopen(csv_path, "w");
    if (!f) {
        SDL_Log("SpriteOverride: failed to open %s for writing", csv_path);
        return;
    }

    fprintf(f, "type,group,id,expected_path\n");

    int sprite_count = 0;
    for (int i = 0; i < SPRITE_CACHE_SIZE; i++) {
        if (s_sprite_miss[i]) {
            fprintf(f, "sprite,%d,%d,assets/sprites/sprite_%d.png\n", s_sprite_group[i], i, i);
            sprite_count++;
        }
    }

    int bg_count = 0;
    for (int i = 0; i < BG_TILE_CACHE_SIZE; i++) {
        if (s_bg_tile_miss[i]) {
            fprintf(f, "bg,0,%d,assets/sprites/bg_%d.png\n", i, i);
            bg_count++;
        }
    }

    fclose(f);
    SDL_Log("SpriteOverride: wrote %s (%d sprites, %d bg tiles missing)", csv_path, sprite_count, bg_count);
}

void SpriteOverride_Shutdown(void) {
    SpriteOverride_DumpMissing();

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

    /* Clean up UI page override cache */
    for (int i = 0; i < UI_PAGE_CACHE_SIZE; i++) {
        if (s_ui_page_cache[i].texture != NULL) {
            TextureUtil_Free(s_ui_page_cache[i].texture);
        }
        s_ui_page_cache[i].texture = NULL;
        s_ui_page_cache[i].key = 0;
        s_ui_page_cache[i].checked = false;
    }
}
