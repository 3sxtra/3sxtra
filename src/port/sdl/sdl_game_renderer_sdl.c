/**
 * @file sdl_game_renderer_sdl.c
 * @brief SDL2D backend — game rendering via SDL_Renderer (SDL3's 2D API).
 *
 * Ported from the perf_fix_for_low_end_devices branch.
 * Uses SDL_RenderGeometry for batched quad rendering with z-sorting.
 * No shaders, no GL context, no ImGui — bare-bones renderer for
 * maximum compatibility on low-end devices (RPi4, old GPUs).
 */
#include "common.h"
#include "port/tracy_zones.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#define RENDER_TASK_MAX 8192
#define TEXTURES_TO_DESTROY_MAX 1024

typedef struct RenderTask {
    SDL_Texture* texture;
    SDL_Vertex vertices[4];
    float z;
    int index;
    int original_index; // Preserves submission order for stable sorting
} RenderTask;

static SDL_Texture* cps3_canvas = NULL;

static const int cps3_width = 384;
static const int cps3_height = 224;

// ⚡ Multi-palette cache for indexed (paletted) sprites.
// Caches PALETTE_CACHE_SLOTS palette variants per texture, preventing
// expensive SDL_CreateTextureFromSurface calls on every palette switch.
// 16 slots covers typical SF3 usage (base, hit flash, shadow, super,
// alt colors) without risking GPU memory on low-end targets.
#define PALETTE_CACHE_SLOTS 16

static SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };
static SDL_Texture* textures[FL_TEXTURE_MAX * PALETTE_CACHE_SLOTS] = { NULL };
static int texture_count = 0;
static SDL_Texture* texture_cache[FL_TEXTURE_MAX] = { NULL };

static SDL_Texture* idx_tex_cache[FL_TEXTURE_MAX][PALETTE_CACHE_SLOTS];
static int idx_tex_palette[FL_TEXTURE_MAX][PALETTE_CACHE_SLOTS];
static int idx_tex_next_slot[FL_TEXTURE_MAX];

// ⚡ Reverse index: for each palette, which texture indices reference it.
// Eliminates O(FL_TEXTURE_MAX × PALETTE_CACHE_SLOTS) linear scan in invalidation.
#define PALETTE_REVERSE_MAX 32
static struct {
    int texture_indices[PALETTE_REVERSE_MAX];
    int count;
} palette_reverse[FL_PALETTE_MAX];

// Forward declarations for reverse index helpers (used by DestroyTexture before definition)
static void palette_reverse_add(int palette_handle, int texture_index);
static void palette_reverse_remove(int palette_handle, int texture_index);

static SDL_Texture* textures_to_destroy[TEXTURES_TO_DESTROY_MAX] = { NULL };
static int textures_to_destroy_count = 0;

// ⚡ Tracy: palette cache miss counter — emitted as a plot each frame
static int palette_cache_misses_frame = 0;
static RenderTask render_tasks[RENDER_TASK_MAX];
static int render_task_count = 0;
static int render_task_order[RENDER_TASK_MAX]; // ⚡ Sorted indices for indirect sort

// ⚡ Sortedness tracking — count inversions during draw_quad to decide sort strategy
static int sort_inversions = 0;
static float last_submitted_z = -1e30f;

// ⚡ Cached texture binding — skip redundant SetTexture lookups
static unsigned int last_set_texture_th = 0;

// Pre-allocated batch buffers for optimized rendering
static SDL_Vertex batch_vertices[RENDER_TASK_MAX * 4];
static int batch_indices[RENDER_TASK_MAX * 6];
static bool batch_buffers_initialized = false;

// Debugging and statistics
static bool draw_rect_borders = false;
static bool dump_textures = false;
static int debug_texture_index = 0;

// --- PlayStation 2 Graphics Synthesizer CLUT index shuffle ---
// The PS2 GS stores 256-color CLUTs in a non-linear memory order.
// This macro swaps bits 3 and 4 of the index to match the hardware's block-swizzled layout.
#define clut_shuf(x) (((x) & ~0x18) | ((((x) & 0x08) << 1) | (((x) & 0x10) >> 1)))

// --- Color Reading Functions ---

static void read_rgba32_color(Uint32 pixel, SDL_Color* color) {
    color->b = pixel & 0xFF;
    color->g = (pixel >> 8) & 0xFF;
    color->r = (pixel >> 16) & 0xFF;
    color->a = (pixel >> 24) & 0xFF;
}

// ⚡ LUT-based byte-to-float conversion — eliminates 4 float divisions per call.
// Pre-computed at compile time; used by read_rgba32_fcolor in the draw hot path.
static const float rgba8_to_float[256] = {
    0.0f/255, 1.0f/255, 2.0f/255, 3.0f/255, 4.0f/255, 5.0f/255, 6.0f/255, 7.0f/255,
    8.0f/255, 9.0f/255, 10.0f/255, 11.0f/255, 12.0f/255, 13.0f/255, 14.0f/255, 15.0f/255,
    16.0f/255, 17.0f/255, 18.0f/255, 19.0f/255, 20.0f/255, 21.0f/255, 22.0f/255, 23.0f/255,
    24.0f/255, 25.0f/255, 26.0f/255, 27.0f/255, 28.0f/255, 29.0f/255, 30.0f/255, 31.0f/255,
    32.0f/255, 33.0f/255, 34.0f/255, 35.0f/255, 36.0f/255, 37.0f/255, 38.0f/255, 39.0f/255,
    40.0f/255, 41.0f/255, 42.0f/255, 43.0f/255, 44.0f/255, 45.0f/255, 46.0f/255, 47.0f/255,
    48.0f/255, 49.0f/255, 50.0f/255, 51.0f/255, 52.0f/255, 53.0f/255, 54.0f/255, 55.0f/255,
    56.0f/255, 57.0f/255, 58.0f/255, 59.0f/255, 60.0f/255, 61.0f/255, 62.0f/255, 63.0f/255,
    64.0f/255, 65.0f/255, 66.0f/255, 67.0f/255, 68.0f/255, 69.0f/255, 70.0f/255, 71.0f/255,
    72.0f/255, 73.0f/255, 74.0f/255, 75.0f/255, 76.0f/255, 77.0f/255, 78.0f/255, 79.0f/255,
    80.0f/255, 81.0f/255, 82.0f/255, 83.0f/255, 84.0f/255, 85.0f/255, 86.0f/255, 87.0f/255,
    88.0f/255, 89.0f/255, 90.0f/255, 91.0f/255, 92.0f/255, 93.0f/255, 94.0f/255, 95.0f/255,
    96.0f/255, 97.0f/255, 98.0f/255, 99.0f/255, 100.0f/255, 101.0f/255, 102.0f/255, 103.0f/255,
    104.0f/255, 105.0f/255, 106.0f/255, 107.0f/255, 108.0f/255, 109.0f/255, 110.0f/255, 111.0f/255,
    112.0f/255, 113.0f/255, 114.0f/255, 115.0f/255, 116.0f/255, 117.0f/255, 118.0f/255, 119.0f/255,
    120.0f/255, 121.0f/255, 122.0f/255, 123.0f/255, 124.0f/255, 125.0f/255, 126.0f/255, 127.0f/255,
    128.0f/255, 129.0f/255, 130.0f/255, 131.0f/255, 132.0f/255, 133.0f/255, 134.0f/255, 135.0f/255,
    136.0f/255, 137.0f/255, 138.0f/255, 139.0f/255, 140.0f/255, 141.0f/255, 142.0f/255, 143.0f/255,
    144.0f/255, 145.0f/255, 146.0f/255, 147.0f/255, 148.0f/255, 149.0f/255, 150.0f/255, 151.0f/255,
    152.0f/255, 153.0f/255, 154.0f/255, 155.0f/255, 156.0f/255, 157.0f/255, 158.0f/255, 159.0f/255,
    160.0f/255, 161.0f/255, 162.0f/255, 163.0f/255, 164.0f/255, 165.0f/255, 166.0f/255, 167.0f/255,
    168.0f/255, 169.0f/255, 170.0f/255, 171.0f/255, 172.0f/255, 173.0f/255, 174.0f/255, 175.0f/255,
    176.0f/255, 177.0f/255, 178.0f/255, 179.0f/255, 180.0f/255, 181.0f/255, 182.0f/255, 183.0f/255,
    184.0f/255, 185.0f/255, 186.0f/255, 187.0f/255, 188.0f/255, 189.0f/255, 190.0f/255, 191.0f/255,
    192.0f/255, 193.0f/255, 194.0f/255, 195.0f/255, 196.0f/255, 197.0f/255, 198.0f/255, 199.0f/255,
    200.0f/255, 201.0f/255, 202.0f/255, 203.0f/255, 204.0f/255, 205.0f/255, 206.0f/255, 207.0f/255,
    208.0f/255, 209.0f/255, 210.0f/255, 211.0f/255, 212.0f/255, 213.0f/255, 214.0f/255, 215.0f/255,
    216.0f/255, 217.0f/255, 218.0f/255, 219.0f/255, 220.0f/255, 221.0f/255, 222.0f/255, 223.0f/255,
    224.0f/255, 225.0f/255, 226.0f/255, 227.0f/255, 228.0f/255, 229.0f/255, 230.0f/255, 231.0f/255,
    232.0f/255, 233.0f/255, 234.0f/255, 235.0f/255, 236.0f/255, 237.0f/255, 238.0f/255, 239.0f/255,
    240.0f/255, 241.0f/255, 242.0f/255, 243.0f/255, 244.0f/255, 245.0f/255, 246.0f/255, 247.0f/255,
    248.0f/255, 249.0f/255, 250.0f/255, 251.0f/255, 252.0f/255, 253.0f/255, 254.0f/255, 255.0f/255,
};

// ⚡ Single-entry color cache — exploits repeated vertex colors in hot path.
static Uint32 cached_fcolor_pixel = 0xDEADBEEF; // sentinel: impossible initial value
static SDL_FColor cached_fcolor_value;

static void read_rgba32_fcolor(Uint32 pixel, SDL_FColor* fcolor) {
    if (pixel == cached_fcolor_pixel) {
        *fcolor = cached_fcolor_value;
        return;
    }
    fcolor->b = rgba8_to_float[pixel & 0xFF];
    fcolor->g = rgba8_to_float[(pixel >> 8) & 0xFF];
    fcolor->r = rgba8_to_float[(pixel >> 16) & 0xFF];
    fcolor->a = rgba8_to_float[(pixel >> 24) & 0xFF];
    cached_fcolor_pixel = pixel;
    cached_fcolor_value = *fcolor;
}

static void read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = (pixel & 0x1F) * 255 / 31;
    color->g = ((pixel >> 5) & 0x1F) * 255 / 31;
    color->b = ((pixel >> 10) & 0x1F) * 255 / 31;
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void read_color(const void* pixels, int index, size_t color_size, SDL_Color* color) {
    switch (color_size) {
    case 2: {
        const Uint16* rgba16_colors = (const Uint16*)pixels;
        read_rgba16_color(rgba16_colors[index], color);
        break;
    }
    case 4: {
        const Uint32* rgba32_colors = (const Uint32*)pixels;
        read_rgba32_color(rgba32_colors[index], color);
        break;
    }
    }
}

#define LERP_FLOAT(a, b, x) ((a) * (1.0f - (x)) + (b) * (x))

static void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x) {
    dest->r = LERP_FLOAT(a->r, b->r, x);
    dest->g = LERP_FLOAT(a->g, b->g, x);
    dest->b = LERP_FLOAT(a->b, b->b, x);
    dest->a = LERP_FLOAT(a->a, b->a, x);
}

// --- Texture Debugging ---

static void save_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    if (surface == NULL || palette == NULL) {
        SDL_Log("Cannot save texture: NULL surface or palette");
        return;
    }

    char filename[128];
    snprintf(filename, sizeof(filename), "textures/%d.tga", debug_texture_index);

    const Uint8* pixels = (const Uint8*)surface->pixels;
    const int width = surface->w;
    const int height = surface->h;

    FILE* f = fopen(filename, "wb");
    if (!f) {
        SDL_Log("Failed to open file for writing: %s", filename);
        return;
    }

    uint8_t header[18] = { 0 };
    header[2] = 2; // uncompressed RGB
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32;   // bits per pixel
    header[17] = 0x20; // top-left origin

    fwrite(header, 1, 18, f);

    // Write pixels in BGRA format
    const int pixel_count = width * height;
    for (int i = 0; i < pixel_count; ++i) {
        Uint8 index;

        if (palette->ncolors == 16) {
            const Uint8 byte = pixels[i / 2];
            if (i & 1) {
                index = byte >> 4;
            } else {
                index = byte & 0x0F;
            }
        } else {
            index = pixels[i];
        }

        const SDL_Color* color = &palette->colors[index];
        const Uint8 bgr[] = { color->b, color->g, color->r, color->a };
        fwrite(bgr, 1, 4, f);
    }

    fclose(f);
    debug_texture_index = (debug_texture_index + 1) % 10000;
}

void SDLGameRendererSDL_DumpTextures(void) {
    SDL_CreateDirectory("textures");
    debug_texture_index = 0;
    int count = 0;

    // idx_tex_palette[ti][slot] records the palette handle actually paired with each texture slot.
    // Use that as the ground-truth association, same as SetTexture.
    for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
        SDL_Surface* surf = surfaces[ti];
        if (!surf || !SDL_ISPIXELFORMAT_INDEXED(surf->format))
            continue;

        for (int slot = 0; slot < PALETTE_CACHE_SLOTS; slot++) {
            int ph = idx_tex_palette[ti][slot];
            if (ph <= 0 || ph > FL_PALETTE_MAX)
                continue;
            SDL_Palette* pal = palettes[ph - 1];
            if (!pal)
                continue;

            save_texture(surf, pal);
            count++;
        }
    }

    SDL_Log("[TextureDump] Wrote %d texture(s) to textures/", count);
}


// --- Texture Stack Management ---

static void push_texture(SDL_Texture* texture) {
    if (texture_count >= FL_TEXTURE_MAX * PALETTE_CACHE_SLOTS) {
        fatal_error("Texture stack overflow in push_texture");
    }
    textures[texture_count] = texture;
    texture_count += 1;
}

static SDL_Texture* get_texture(void) {
    if (texture_count == 0) {
        fatal_error("No textures to get");
    }
    return textures[texture_count - 1];
}

// --- Deferred Texture Destruction ---

static void push_texture_to_destroy(SDL_Texture* texture) {
    if (textures_to_destroy_count >= TEXTURES_TO_DESTROY_MAX) {
        SDL_Log("Warning: textures_to_destroy buffer full, destroying texture immediately");
        SDL_DestroyTexture(texture);
        return;
    }
    textures_to_destroy[textures_to_destroy_count] = texture;
    textures_to_destroy_count += 1;
}

static void destroy_textures(void) {
    for (int i = 0; i < texture_count; i++) {
        textures[i] = NULL;
    }
    texture_count = 0;

    for (int i = 0; i < textures_to_destroy_count; i++) {
        SDL_DestroyTexture(textures_to_destroy[i]);
        textures_to_destroy[i] = NULL;
    }
    textures_to_destroy_count = 0;
}

// --- Render Task Management ---

static void clear_render_tasks(void) {
    // ⚡ Only reset count — no need to zero ~800KB of static data every frame
    render_task_count = 0;
    // ⚡ Reset sortedness tracking for next frame
    sort_inversions = 0;
    last_submitted_z = -1e30f;
}

// --- Render Task Sorting ---
// Sort by z-depth first, then original submission order for stable layering.

// ⚡ Index-based comparator — sorts 4-byte indices instead of 104-byte structs,
// eliminating O(n log n) large struct swaps during qsort.
static int compare_render_task_indices(const void* a, const void* b) {
    const int idx_a = *(const int*)a;
    const int idx_b = *(const int*)b;
    const RenderTask* task_a = &render_tasks[idx_a];
    const RenderTask* task_b = &render_tasks[idx_b];

    if (task_a->z < task_b->z)
        return -1;
    else if (task_a->z > task_b->z)
        return 1;

    // Reverse order preserves FIFO submission behavior (array index == submission order)
    if (idx_a > idx_b)
        return -1;
    else if (idx_a < idx_b)
        return 1;

    return 0;
}

// ⚡ Insertion sort for near-sorted index arrays — O(n) when already sorted.
// The game submits sprites roughly in z-order, so most frames have few inversions.
#define INSERTION_SORT_THRESHOLD 16
static void insertion_sort_render_task_indices(int* order, int count) {
    for (int i = 1; i < count; i++) {
        const int key = order[i];
        const float key_z = render_tasks[key].z;
        int j = i - 1;
        while (j >= 0) {
            const int oj = order[j];
            const float oj_z = render_tasks[oj].z;
            // Compare: sort by z ascending, then by index descending (reverse submission order)
            if (oj_z < key_z || (oj_z == key_z && oj > key))
                break;
            order[j + 1] = oj;
            j--;
        }
        order[j + 1] = key;
    }
}

// --- Public API ---

SDL_Texture* SDLGameRendererSDL_GetCanvas(void) {
    return cps3_canvas;
}

void SDLGameRendererSDL_Init(void) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    cps3_canvas =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, cps3_width, cps3_height);
    if (!cps3_canvas) {
        fatal_error("Failed to create cps3_canvas texture: %s", SDL_GetError());
    }
    SDL_SetTextureScaleMode(cps3_canvas, SDL_SCALEMODE_NEAREST);

    // Pre-initialize index buffer (constant for all quads: two triangles per quad)
    if (!batch_buffers_initialized) {
        for (int i = 0; i < RENDER_TASK_MAX; i++) {
            const int idx_offset = i * 6;
            const int vert_offset = i * 4;
            batch_indices[idx_offset + 0] = vert_offset + 0;
            batch_indices[idx_offset + 1] = vert_offset + 1;
            batch_indices[idx_offset + 2] = vert_offset + 2;
            batch_indices[idx_offset + 3] = vert_offset + 1;
            batch_indices[idx_offset + 4] = vert_offset + 2;
            batch_indices[idx_offset + 5] = vert_offset + 3;
        }
        batch_buffers_initialized = true;
    }
}

void SDLGameRendererSDL_Shutdown(void) {
    // Destroy all cached non-indexed textures
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (texture_cache[i] != NULL) {
            SDL_DestroyTexture(texture_cache[i]);
            texture_cache[i] = NULL;
        }
    }

    // Destroy all cached indexed (multi-palette) textures
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        for (int s = 0; s < PALETTE_CACHE_SLOTS; s++) {
            if (idx_tex_cache[i][s] != NULL) {
                SDL_DestroyTexture(idx_tex_cache[i][s]);
                idx_tex_cache[i][s] = NULL;
            }
        }
        idx_tex_next_slot[i] = 0;
    }

    // Destroy all surfaces
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (surfaces[i] != NULL) {
            SDL_DestroySurface(surfaces[i]);
            surfaces[i] = NULL;
        }
    }

    // Destroy all palettes
    for (int i = 0; i < FL_PALETTE_MAX; i++) {
        if (palettes[i] != NULL) {
            SDL_DestroyPalette(palettes[i]);
            palettes[i] = NULL;
        }
    }

    // Destroy deferred textures
    destroy_textures();

    // Destroy canvas
    if (cps3_canvas != NULL) {
        SDL_DestroyTexture(cps3_canvas);
        cps3_canvas = NULL;
    }

    clear_render_tasks();
    batch_buffers_initialized = false;
}

void SDLGameRendererSDL_BeginFrame(void) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();

    // Clear canvas
    const Uint8 r = (flPs2State.FrameClearColor >> 16) & 0xFF;
    const Uint8 g = (flPs2State.FrameClearColor >> 8) & 0xFF;
    const Uint8 b = flPs2State.FrameClearColor & 0xFF;
    const Uint8 a = flPs2State.FrameClearColor >> 24;

    if (a != SDL_ALPHA_TRANSPARENT) {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_SetRenderTarget(renderer, cps3_canvas);
    SDL_RenderClear(renderer);
}

void SDLGameRendererSDL_RenderFrame(void) {
    TRACE_PLOT_INT("PalCacheMiss", palette_cache_misses_frame);
    TRACE_PLOT_INT("RenderTasks", render_task_count);
    palette_cache_misses_frame = 0;
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas);

    if (render_task_count == 0) {
        return;
    }

    // ⚡ Index-based sort: sort 4-byte indices instead of 104-byte RenderTask structs
    for (int i = 0; i < render_task_count; i++) {
        render_task_order[i] = i;
    }

    // ⚡ Adaptive sort: use insertion sort for near-sorted queues, qsort otherwise.
    // Most gameplay frames submit sprites in z-order, yielding 0 inversions.
    if (sort_inversions <= INSERTION_SORT_THRESHOLD) {
        insertion_sort_render_task_indices(render_task_order, render_task_count);
    } else {
        qsort(render_task_order, render_task_count, sizeof(int), compare_render_task_indices);
    }

    // Batch rendering: group consecutive tasks with same texture
    int batch_start = 0;
    SDL_Texture* current_texture = render_tasks[render_task_order[0]].texture;

    for (int i = 0; i <= render_task_count; i++) {
        const bool should_flush =
            (i == render_task_count) || (render_tasks[render_task_order[i]].texture != current_texture);

        if (should_flush) {
            const int batch_size = i - batch_start;
            if (batch_size > 0) {
                SDL_assert(batch_size <= RENDER_TASK_MAX);

                // Copy vertices to batch buffer via sorted indices
                for (int j = 0; j < batch_size; j++) {
                    const int task_idx = render_task_order[batch_start + j];
                    const int vert_offset = j * 4;
                    memcpy(&batch_vertices[vert_offset], render_tasks[task_idx].vertices, 4 * sizeof(SDL_Vertex));
                }

                // Single draw call for entire batch
                SDL_RenderGeometry(
                    renderer, current_texture, batch_vertices, batch_size * 4, batch_indices, batch_size * 6);
            }

            if (i < render_task_count) {
                current_texture = render_tasks[render_task_order[i]].texture;
                batch_start = i;
            }
        }
    }

    // Debug visualization: draw colored borders around quads
    if (draw_rect_borders) {
        const SDL_FColor red = { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = SDL_ALPHA_OPAQUE_FLOAT };
        const SDL_FColor green = { .r = 0.0f, .g = 1.0f, .b = 0.0f, .a = SDL_ALPHA_OPAQUE_FLOAT };
        SDL_FColor border_color;

        for (int i = 0; i < render_task_count; i++) {
            const RenderTask* task = &render_tasks[render_task_order[i]];
            const float x0 = task->vertices[0].position.x;
            const float y0 = task->vertices[0].position.y;
            const float x1 = task->vertices[3].position.x;
            const float y1 = task->vertices[3].position.y;
            const SDL_FRect border_rect = { .x = x0, .y = y0, .w = (x1 - x0), .h = (y1 - y0) };

            const float lerp_factor = (render_task_count > 1) ? (float)i / (float)(render_task_count - 1) : 0.5f;
            lerp_fcolors(&border_color, &red, &green, lerp_factor);

            SDL_SetRenderDrawColorFloat(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
            SDL_RenderRect(renderer, &border_rect);
        }
    }
}

void SDLGameRendererSDL_EndFrame(void) {
    destroy_textures();
    clear_render_tasks();
}

void SDLGameRendererSDL_UnlockPalette(unsigned int ph) {
    const int palette_handle = ph;

    if ((palette_handle > 0) && (palette_handle < FL_PALETTE_MAX)) {
        SDLGameRendererSDL_DestroyPalette(palette_handle);
        SDLGameRendererSDL_CreatePalette(ph << 16);
    }
}

/**
 * ⚡ Lightweight texture invalidation — replaces the old destroy+recreate cycle.
 *
 * The SDL_Surface created by CreateTexture references the system buffer via
 * pointer (SDL_CreateSurfaceFrom).  Since the caller (ppgRenewTexChunkSeqs)
 * writes directly into that system buffer, the surface already reflects the
 * new pixel data.  We only need to invalidate the GPU-side texture caches
 * so that SetTexture will recreate them from the (now-updated) surface.
 */
void SDLGameRendererSDL_UnlockTexture(unsigned int th) {
    const int texture_index = th - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        return;
    }

    /* Invalidate texture binding cache */
    last_set_texture_th = 0;

    /* Invalidate non-indexed GPU texture */
    if (texture_cache[texture_index] != NULL) {
        push_texture_to_destroy(texture_cache[texture_index]);
        texture_cache[texture_index] = NULL;
    }

    /* Invalidate all indexed (multi-palette) GPU textures for this texture */
    for (int s = 0; s < PALETTE_CACHE_SLOTS; s++) {
        if (idx_tex_cache[texture_index][s] != NULL) {
            push_texture_to_destroy(idx_tex_cache[texture_index][s]);
            palette_reverse_remove(idx_tex_palette[texture_index][s], texture_index);
            idx_tex_cache[texture_index][s] = NULL;
            idx_tex_palette[texture_index][s] = 0;
        }
    }
    idx_tex_next_slot[texture_index] = 0;

    /* Surface stays alive — it still references the (now-updated) system buffer.
     * SetTexture will lazily create a new GPU texture from it on next use. */

    /* For non-indexed formats, eagerly recreate the GPU texture since there is
     * no lazy path in SetTexture for non-indexed textures. */
    SDL_Surface* surface = surfaces[texture_index];
    if (surface != NULL && !SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            texture_cache[texture_index] = texture;
        }
    }
}

void SDLGameRendererSDL_CreateTexture(unsigned int th) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    const int texture_index = LO_16_BITS(th) - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        fatal_error("Texture index out of bounds in CreateTexture: %d", texture_index + 1);
    }

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (surfaces[texture_index] != NULL) {
        fatal_error("Overwriting an existing texture at index %d", texture_index);
    }

    switch (fl_texture->format) {
    case SCE_GS_PSMT8:
        pixel_format = SDL_PIXELFORMAT_INDEX8;
        pitch = fl_texture->width;
        break;

    case SCE_GS_PSMT4:
        pixel_format = SDL_PIXELFORMAT_INDEX4LSB;
        pitch = (fl_texture->width + 1) / 2;
        break;

    case SCE_GS_PSMCT16:
        pixel_format = SDL_PIXELFORMAT_ABGR1555;
        pitch = fl_texture->width * 2;
        break;

    default:
        fatal_error("Unhandled pixel format: %d", fl_texture->format);
        break;
    }

    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
    if (!surface) {
        fatal_error("Failed to create surface from memory: %s", SDL_GetError());
    }
    surfaces[texture_index] = surface;

    // For non-indexed formats (e.g. PSMCT16), create the GPU texture eagerly.
    // Indexed textures are created lazily in SetTexture once a palette is known.
    if (!SDL_ISPIXELFORMAT_INDEXED(pixel_format)) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            fatal_error("Failed to create texture from surface: %s", SDL_GetError());
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        texture_cache[texture_index] = texture;
    }
}

void SDLGameRendererSDL_DestroyTexture(unsigned int texture_handle) {
    const int texture_index = texture_handle - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        SDL_Log("Warning: Attempted to destroy invalid texture handle: %u", texture_handle);
        return;
    }

    // ⚡ Invalidate texture binding cache when a texture is destroyed
    last_set_texture_th = 0;

    // Destroy non-indexed cached texture
    if (texture_cache[texture_index] != NULL) {
        push_texture_to_destroy(texture_cache[texture_index]);
        texture_cache[texture_index] = NULL;
    }

    // ⚡ Destroy all indexed (multi-palette) cached textures for this texture
    for (int s = 0; s < PALETTE_CACHE_SLOTS; s++) {
        if (idx_tex_cache[texture_index][s] != NULL) {
            push_texture_to_destroy(idx_tex_cache[texture_index][s]);
            // ⚡ Clean up reverse index entry
            palette_reverse_remove(idx_tex_palette[texture_index][s], texture_index);
            idx_tex_cache[texture_index][s] = NULL;
            idx_tex_palette[texture_index][s] = 0;
        }
    }
    idx_tex_next_slot[texture_index] = 0;

    if (surfaces[texture_index] != NULL) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
    }
}

void SDLGameRendererSDL_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;

    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {
        fatal_error("Palette index out of bounds in CreatePalette: %d", palette_index + 1);
    }

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    SDL_Color colors[256];
    size_t color_size = 0;

    if (palettes[palette_index] != NULL) {
        fatal_error("Overwriting an existing palette at index %d", palette_index);
    }

    switch (fl_palette->format) {
    case SCE_GS_PSMCT32:
        color_size = 4;
        break;

    case SCE_GS_PSMCT16:
        color_size = 2;
        break;

    default:
        fatal_error("Unhandled palette pixel format: %d", fl_palette->format);
        break;
    }

    switch (color_count) {
    case 16:
        for (int i = 0; i < 16; i++) {
            read_color(pixels, i, color_size, &colors[i]);
        }
        break;

    case 256:
        // Apply PS2 GS CLUT shuffle for 256-color palettes
        for (int i = 0; i < 256; i++) {
            const int color_index = clut_shuf(i);
            read_color(pixels, color_index, color_size, &colors[i]);
        }
        break;

    default:
        fatal_error("Unhandled palette dimensions: %dx%d", fl_palette->width, fl_palette->height);
        break;
    }

    SDL_Palette* palette = SDL_CreatePalette(color_count);

    if (!palette) {
        fatal_error("Failed to create SDL palette: %s", SDL_GetError());
    }

    SDL_SetPaletteColors(palette, colors, 0, color_count);
    palettes[palette_index] = palette;
}

// ⚡ Register a texture→palette association in the reverse index.
static void palette_reverse_add(int palette_handle, int texture_index) {
    if (palette_handle <= 0 || palette_handle > FL_PALETTE_MAX)
        return;
    int pi = palette_handle - 1;
    // Check if already registered
    for (int i = 0; i < palette_reverse[pi].count; i++) {
        if (palette_reverse[pi].texture_indices[i] == texture_index)
            return;
    }
    if (palette_reverse[pi].count < PALETTE_REVERSE_MAX) {
        palette_reverse[pi].texture_indices[palette_reverse[pi].count++] = texture_index;
    }
}

// ⚡ Remove a texture from a palette's reverse index.
static void palette_reverse_remove(int palette_handle, int texture_index) {
    if (palette_handle <= 0 || palette_handle > FL_PALETTE_MAX)
        return;
    int pi = palette_handle - 1;
    for (int i = 0; i < palette_reverse[pi].count; i++) {
        if (palette_reverse[pi].texture_indices[i] == texture_index) {
            palette_reverse[pi].texture_indices[i] = palette_reverse[pi].texture_indices[--palette_reverse[pi].count];
            return;
        }
    }
}

// ⚡ Invalidate all multi-palette cache entries that use a specific palette.
// Uses reverse index — O(entries_using_this_palette) instead of O(FL_TEXTURE_MAX × PALETTE_CACHE_SLOTS).
static void invalidate_palette_cache_entries(int palette_handle) {
    if (palette_handle <= 0 || palette_handle > FL_PALETTE_MAX)
        return;
    int pi = palette_handle - 1;
    for (int i = 0; i < palette_reverse[pi].count; i++) {
        int t = palette_reverse[pi].texture_indices[i];
        for (int s = 0; s < PALETTE_CACHE_SLOTS; s++) {
            if (idx_tex_cache[t][s] != NULL && idx_tex_palette[t][s] == palette_handle) {
                push_texture_to_destroy(idx_tex_cache[t][s]);
                idx_tex_cache[t][s] = NULL;
                idx_tex_palette[t][s] = 0;
            }
        }
    }
    palette_reverse[pi].count = 0;
}

void SDLGameRendererSDL_DestroyPalette(unsigned int palette_handle) {
    const int palette_index = palette_handle - 1;

    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {
        SDL_Log("Warning: Attempted to destroy invalid palette handle: %u", palette_handle);
        return;
    }

    // ⚡ Invalidate texture binding cache when a palette is destroyed
    last_set_texture_th = 0;

    // Invalidate cached textures that used this palette (prevents stale reuse)
    invalidate_palette_cache_entries(palette_handle);

    if (palettes[palette_index] != NULL) {
        SDL_DestroyPalette(palettes[palette_index]);
        palettes[palette_index] = NULL;
    }
}

void SDLGameRendererSDL_SetTexture(unsigned int th) {
    // ⚡ Cached texture binding — skip full lookup when same texture+palette was just set
    if (th == last_set_texture_th) {
        // Re-push the same texture that's already on top of the stack
        if (texture_count > 0) {
            push_texture(textures[texture_count - 1]);
            return;
        }
    }
    last_set_texture_th = th;

    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);
    const int texture_index = texture_handle - 1;

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        fatal_error("Invalid texture handle in SetTexture: %d", texture_handle);
    }

    if (palette_handle > FL_PALETTE_MAX) {
        fatal_error("Invalid palette handle in SetTexture: %d", palette_handle);
    }

    SDL_Surface* surface = surfaces[texture_index];

    if (!surface) {
        // Surface may not be loaded yet during game init — skip silently
        return;
    }

    const SDL_Palette* palette = (palette_handle != 0) ? palettes[palette_handle - 1] : NULL;

    if (dump_textures && palette != NULL) {
        save_texture(surface, palette);
    }

    // ⚡ For indexed textures: use multi-palette cache to avoid recreating
    // GPU textures on every palette switch. Each texture caches up to
    // PALETTE_CACHE_SLOTS different palette variants.
    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Texture* texture = NULL;

        // Search multi-palette cache for this texture+palette combo
        for (int s = 0; s < PALETTE_CACHE_SLOTS; s++) {
            if (idx_tex_cache[texture_index][s] != NULL && idx_tex_palette[texture_index][s] == palette_handle) {
                texture = idx_tex_cache[texture_index][s];
                break;
            }
        }

        if (!texture) {
            // Cache miss — create new texture, evicting oldest slot if needed
            if (palette != NULL) {
                SDL_SetSurfacePalette(surface, palette);
            }

            const int slot = idx_tex_next_slot[texture_index];
            if (idx_tex_cache[texture_index][slot] != NULL) {
                push_texture_to_destroy(idx_tex_cache[texture_index][slot]);
                // ⚡ Clean up reverse index for evicted palette association
                palette_reverse_remove(idx_tex_palette[texture_index][slot], texture_index);
            }

            texture = SDL_CreateTextureFromSurface(renderer, surface);
            palette_cache_misses_frame++;
            if (!texture) {
                fatal_error("Failed to create texture from surface: %s", SDL_GetError());
            }
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

            idx_tex_cache[texture_index][slot] = texture;
            idx_tex_palette[texture_index][slot] = palette_handle;
            idx_tex_next_slot[texture_index] = (slot + 1) % PALETTE_CACHE_SLOTS;

            // ⚡ Register in reverse index for fast invalidation
            palette_reverse_add(palette_handle, texture_index);
        }

        push_texture(texture);
    } else {
        // Non-indexed texture — use simple 1:1 cache
        SDL_Texture* texture = texture_cache[texture_index];
        if (texture == NULL) {
            return;
        }
        push_texture(texture);
    }
}

// ⚡ Write directly into the render_tasks array, avoiding a 104-byte stack
// struct allocation + copy that the old push_render_task pattern required.
static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (render_task_count >= RENDER_TASK_MAX) {
        SDL_Log("Warning: render task buffer full, skipping task");
        return;
    }

    RenderTask* task = &render_tasks[render_task_count];
    task->index = render_task_count;
    task->texture = textured ? get_texture() : NULL;
    task->z = flPS2ConvScreenFZ(vertices[0].coord.z);
    task->original_index = render_task_count;

    // ⚡ Track sortedness: count inversions to decide sort strategy in RenderFrame
    if (task->z < last_submitted_z) {
        sort_inversions++;
    }
    last_submitted_z = task->z;

    for (int i = 0; i < 4; i++) {
        task->vertices[i].position.x = vertices[i].coord.x;
        task->vertices[i].position.y = vertices[i].coord.y;

        if (textured) {
            task->vertices[i].tex_coord.x = vertices[i].tex_coord.s;
            task->vertices[i].tex_coord.y = vertices[i].tex_coord.t;
        } else {
            task->vertices[i].tex_coord.x = 0.0f;
            task->vertices[i].tex_coord.y = 0.0f;
        }

        read_rgba32_fcolor(vertices[i].color, &task->vertices[i].color);
    }

    render_task_count++;
}

void SDLGameRendererSDL_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
        vertices[i].tex_coord = sprite->t[i];
    }

    draw_quad(vertices, true);
}

void SDLGameRendererSDL_DrawSolidQuad(const Quad* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
    }

    draw_quad(vertices, false);
}

void SDLGameRendererSDL_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    SDL_zeroa(vertices);

    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = sprite->v[0].z;
        vertices[i].color = color;
    }

    vertices[0].coord.x = sprite->v[0].x;
    vertices[0].coord.y = sprite->v[0].y;
    vertices[3].coord.x = sprite->v[3].x;
    vertices[3].coord.y = sprite->v[3].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite->t[0];
    vertices[3].tex_coord = sprite->t[3];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}

void SDLGameRendererSDL_DrawSprite2(const Sprite2* sprite2) {
    Sprite sprite;
    SDL_zero(sprite);

    sprite.v[0] = sprite2->v[0];
    sprite.v[1].x = sprite2->v[1].x;
    sprite.v[1].y = sprite2->v[0].y;
    sprite.v[2].x = sprite2->v[0].x;
    sprite.v[2].y = sprite2->v[1].y;
    sprite.v[3] = sprite2->v[1];

    sprite.t[0] = sprite2->t[0];
    sprite.t[1].s = sprite2->t[1].s;
    sprite.t[1].t = sprite2->t[0].t;
    sprite.t[2].s = sprite2->t[0].s;
    sprite.t[2].t = sprite2->t[1].t;
    sprite.t[3] = sprite2->t[1];

    for (int i = 0; i < 4; i++) {
        sprite.v[i].z = sprite2->v[0].z;
    }

    SDLGameRendererSDL_DrawSprite(&sprite, sprite2->vertex_color);
}

unsigned int SDLGameRendererSDL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    (void)texture_handle;
    (void)palette_handle;
    // SDL2D mode has no GL textures — return 0 (callers must handle gracefully)
    return 0;
}

/**
 * ⚡ Batch sprite flush for SDL2D — inlines Sprite2 → RenderTask conversion.
 *
 * Eliminates three layers of per-sprite function calls:
 *   DrawSprite2 → DrawSprite → draw_quad
 * by writing directly into the render_tasks array.
 *
 * SetTexture is still called per tex_code change for palette cache lookup.
 */
void SDLGameRendererSDL_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    unsigned int last_tex_code = 0;

    for (int i = 0; i < count; i++) {
        if (!active_layers[chips[i].id])
            continue;

        if (render_task_count >= RENDER_TASK_MAX)
            break;

        const Sprite2* spr = &chips[i];

        /* SetTexture on tex_code change — needed for palette cache lookup */
        unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            SDLGameRendererSDL_SetTexture(tc);
        }

        /* --- Inlined draw_quad: Sprite2 → RenderTask directly --- */
        RenderTask* task = &render_tasks[render_task_count];
        task->index = render_task_count;
        task->texture = (texture_count > 0) ? textures[texture_count - 1] : NULL;
        task->z = flPS2ConvScreenFZ(spr->v[0].z);
        task->original_index = render_task_count;

        /* Track sortedness for adaptive sort in RenderFrame */
        if (task->z < last_submitted_z) {
            sort_inversions++;
        }
        last_submitted_z = task->z;

        /* Pre-compute color floats once per sprite (same as GPU batch path) */
        SDL_FColor fc;
        read_rgba32_fcolor(spr->vertex_color, &fc);

        /* Expand Sprite2 (2 corners) → 4 vertices */
        const float x0 = spr->v[0].x, y0 = spr->v[0].y;
        const float x1 = spr->v[1].x, y1 = spr->v[1].y;
        const float s0 = spr->t[0].s,  t0 = spr->t[0].t;
        const float s1 = spr->t[1].s,  t1 = spr->t[1].t;

        task->vertices[0].position.x = x0; task->vertices[0].position.y = y0;
        task->vertices[0].tex_coord.x = s0; task->vertices[0].tex_coord.y = t0;
        task->vertices[0].color = fc;

        task->vertices[1].position.x = x1; task->vertices[1].position.y = y0;
        task->vertices[1].tex_coord.x = s1; task->vertices[1].tex_coord.y = t0;
        task->vertices[1].color = fc;

        task->vertices[2].position.x = x0; task->vertices[2].position.y = y1;
        task->vertices[2].tex_coord.x = s0; task->vertices[2].tex_coord.y = t1;
        task->vertices[2].color = fc;

        task->vertices[3].position.x = x1; task->vertices[3].position.y = y1;
        task->vertices[3].tex_coord.x = s1; task->vertices[3].tex_coord.y = t1;
        task->vertices[3].color = fc;

        render_task_count++;
    }
}
