#ifndef SDL_GAME_RENDERER_GL_INTERNAL_H
#define SDL_GAME_RENDERER_GL_INTERNAL_H

#include "common.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>

// ⚡ Bolt: SIMDe — portable SIMD intrinsics
#include <simde/x86/sse2.h>
#include <simde/x86/ssse3.h>

#define RENDER_TASK_MAX 8192
#define TEXTURES_TO_DESTROY_MAX 1024
#define TEX_ARRAY_SIZE 512
#define TEX_ARRAY_MAX_LAYERS 128
#define PALETTE_BUFFER_SIZE (FL_PALETTE_MAX * 256 * 4 * sizeof(float))
#define OFFSET_BUFFER_COUNT 3
#define CONVERSION_BUFFER_MAX_PIXELS (512 * 512)
#define CONVERSION_BUFFER_BYTES (CONVERSION_BUFFER_MAX_PIXELS * sizeof(u32))
#define TCACHE_LIVE_MAX 4096

typedef struct {
    uint16_t tex_idx; // texture_handle - 1
    uint16_t pal_idx; // palette_handle (0 = no palette)
} TCacheLivePair;

typedef struct RenderTask {
    GLuint texture;
    int vertex_offset; // Offset into the global batch_vertices buffer
    float z;
    int index;
    int original_index; // Preserves submission order for stable sorting
    int array_layer;    // ⚡ Bolt: >= 0 means use texture array, -1 means legacy path
    int palette_slot;   // Slot index in the palette buffer
} RenderTask;

// Global GL State Container
typedef struct {
    // Resources
    GLuint tex_array_id;
    int tex_array_free[TEX_ARRAY_MAX_LAYERS];
    int tex_array_free_count;
    int16_t tex_array_layer[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];

    GLuint palette_tbo;
    GLuint palette_buffer;
    int palette_slots[FL_PALETTE_MAX];
    bool palette_slot_free[FL_PALETTE_MAX];

    GLuint cps3_canvas_fbo;
    GLuint white_texture;

    // Buffers (Triple Buffering)
    GLuint persistent_vaos[OFFSET_BUFFER_COUNT];
    GLuint persistent_vbos[OFFSET_BUFFER_COUNT];
    GLuint persistent_ebos[OFFSET_BUFFER_COUNT];
    GLuint persistent_layer_vbos[OFFSET_BUFFER_COUNT];
    GLuint persistent_pal_vbos[OFFSET_BUFFER_COUNT];

    SDL_Vertex* persistent_vbo_ptr[OFFSET_BUFFER_COUNT];
    float* persistent_layer_ptr[OFFSET_BUFFER_COUNT];
    float* persistent_pal_ptr[OFFSET_BUFFER_COUNT];

    GLsync fences[OFFSET_BUFFER_COUNT];
    bool use_persistent_mapping;
    int buffer_index;

    // Batching & Tasks
    RenderTask render_tasks[RENDER_TASK_MAX];
    int render_task_count;
    RenderTask merge_temp[RENDER_TASK_MAX]; // For sorting

    SDL_Vertex batch_vertices[RENDER_TASK_MAX * 4];
    int batch_indices[RENDER_TASK_MAX * 6];
    float batch_layers[RENDER_TASK_MAX * 4];
    float batch_pal_indices[RENDER_TASK_MAX * 4];

    // Texture State Stack (per frame) — sized to match max render tasks
    GLuint textures[RENDER_TASK_MAX];
    int texture_layers[RENDER_TASK_MAX];
    int texture_pal_slots[RENDER_TASK_MAX];
    float texture_uv_sx[RENDER_TASK_MAX];
    float texture_uv_sy[RENDER_TASK_MAX];
    int texture_count;
    unsigned int last_set_texture_th;

    // Caches & Dirty Tracking
    SDL_Surface* surfaces[FL_TEXTURE_MAX];
    SDL_Palette* palettes[FL_PALETTE_MAX];

    GLuint texture_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];
    int16_t texture_cache_w[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];
    int16_t texture_cache_h[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];
    GLuint stale_texture_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];

    TCacheLivePair tcache_live[TCACHE_LIVE_MAX];
    int tcache_live_count;

    GLuint textures_to_destroy[TEXTURES_TO_DESTROY_MAX];
    int textures_to_destroy_count;

    bool texture_dirty_flags[FL_TEXTURE_MAX];
    int dirty_texture_indices[FL_TEXTURE_MAX];
    int dirty_texture_count;

    bool palette_dirty_flags[FL_PALETTE_MAX];
    int dirty_palette_indices[FL_PALETTE_MAX];
    int dirty_palette_count;
    uint32_t palette_hash[FL_PALETTE_MAX];

    // Upload & Conversion
    u32 conversion_buffer[CONVERSION_BUFFER_MAX_PIXELS];
    GLuint pbo_upload;
    bool use_pbo;

    // Shader Uniform Locations
    GLint loc_projection;
    GLint loc_source;
    GLint arr_loc_projection;
    GLint arr_loc_source;
    GLint arr_loc_palette;

    // Config
    bool draw_rect_borders;
    bool dump_textures;

} GLRendererState;

extern GLRendererState gl_state;

// Helpers shared between Resources and Draw
void check_gl_error(const char* operation);
void push_texture_to_destroy(GLuint texture);
void tcache_live_add(int tex_idx, int pal_idx);

#endif // SDL_GAME_RENDERER_GL_INTERNAL_H
