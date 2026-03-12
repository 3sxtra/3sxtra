/**
 * @file sdl_game_renderer_gl_resources.c
 * @brief OpenGL renderer texture and palette resource management.
 *
 * Handles creation, destruction, upload, and caching of OpenGL textures
 * and palettes. Implements PS2 CLUT shuffle, palette hashing for dirty
 * detection, and the texture cache live-set. Part of the GL rendering backend.
 */
#include "port/config/config.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/renderer/sdl_game_renderer_gl_internal.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <libgraph.h>
#include <simde/x86/sse4.2.h> // ⚡ Bolt: CRC32 intrinsics for fast palette hashing
#include <stdio.h>
#include <stdlib.h>

// ⚡ Bolt: 5-bit → 8-bit expansion LUT
static const u8 s_5to8[32] = { 0,   8,   16,  25,  33,  41,  49,  58,  66,  74,  82,  90,  99,  107, 115, 123,
                               132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255 };

// --- Helpers ---

void check_gl_error(const char* operation) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OpenGL error after %s: 0x%x", operation, err);
    }
}

static void read_rgba32_color(Uint32 pixel, float* out_rgba) {
    out_rgba[0] = ((pixel >> 16) & 0xFF) / 255.0f;
    out_rgba[1] = ((pixel >> 8) & 0xFF) / 255.0f;
    out_rgba[2] = (pixel & 0xFF) / 255.0f;
    out_rgba[3] = ((pixel >> 24) & 0xFF) / 255.0f;
}

static void read_rgba16_color(Uint16 pixel, float* out_rgba) {
    out_rgba[0] = s_5to8[pixel & 0x1F] / 255.0f;
    out_rgba[1] = s_5to8[(pixel >> 5) & 0x1F] / 255.0f;
    out_rgba[2] = s_5to8[(pixel >> 10) & 0x1F] / 255.0f;
    out_rgba[3] = ((pixel & 0x8000) ? 1.0f : 0.0f);
}

// --- CLUT Shuffle for PS2 ---
// The PS2 GS stores 256-color CLUTs in a non-linear memory order.
// This LUT maps linear index (0-255) to the shuffled GS index.
static const Uint8 ps2_clut_shuffle[256] = {
    0,   1,   2,   3,   4,   5,   6,   7,   16,  17,  18,  19,  20,  21,  22,  23,  8,   9,   10,  11,  12,  13,
    14,  15,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  48,  49,  50,  51,
    52,  53,  54,  55,  40,  41,  42,  43,  44,  45,  46,  47,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,
    66,  67,  68,  69,  70,  71,  80,  81,  82,  83,  84,  85,  86,  87,  72,  73,  74,  75,  76,  77,  78,  79,
    88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 112, 113, 114, 115, 116, 117,
    118, 119, 104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
    132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151, 136, 137, 138, 139, 140, 141, 142, 143, 152, 153,
    154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183,
    168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 208, 209, 210, 211, 212, 213, 214, 215, 200, 201, 202, 203, 204, 205, 206, 207, 216, 217, 218, 219,
    220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247, 232, 233,
    234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255
};
void tcache_live_init(void) {
    gl_state.tcache_live_count = 0;
}

void tcache_live_add(int tex_idx, int pal_idx) {
#ifndef NDEBUG
    for (int i = 0; i < gl_state.tcache_live_count; i++) {
        SDL_assert(!(gl_state.tcache_live[i].tex_idx == tex_idx && gl_state.tcache_live[i].pal_idx == pal_idx));
    }
#endif
    if (gl_state.tcache_live_count >= TCACHE_LIVE_MAX) {
        SDL_Log("Warning: tcache_live overflow (%d pairs)", gl_state.tcache_live_count);
        return;
    }
    gl_state.tcache_live[gl_state.tcache_live_count].tex_idx = (uint16_t)tex_idx;
    gl_state.tcache_live[gl_state.tcache_live_count].pal_idx = (uint16_t)pal_idx;
    gl_state.tcache_live_count++;
}

void push_texture_to_destroy(GLuint texture) {
    if (gl_state.textures_to_destroy_count >= TEXTURES_TO_DESTROY_MAX) {
        SDL_Log("Warning: textures_to_destroy buffer full, destroying texture immediately");
        glDeleteTextures(1, &texture);
        return;
    }
    gl_state.textures_to_destroy[gl_state.textures_to_destroy_count] = texture;
    gl_state.textures_to_destroy_count += 1;
}

static void push_texture_with_layer(GLuint texture, int layer, int pal_slot, float uv_sx, float uv_sy) {
    if (gl_state.texture_count >= RENDER_TASK_MAX) {
        fatal_error("Texture stack overflow in push_texture");
    }
    gl_state.textures[gl_state.texture_count] = texture;
    gl_state.texture_layers[gl_state.texture_count] = layer;
    gl_state.texture_pal_slots[gl_state.texture_count] = pal_slot;
    gl_state.texture_uv_sx[gl_state.texture_count] = uv_sx;
    gl_state.texture_uv_sy[gl_state.texture_count] = uv_sy;
    gl_state.texture_count += 1;
}

// --- Lifecycle ---

void SDLGameRendererGL_Init() {
    // Create FBO
    glGenFramebuffers(1, &gl_state.cps3_canvas_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_state.cps3_canvas_fbo);

    glGenTextures(1, &cps3_canvas_texture);
    glBindTexture(GL_TEXTURE_2D, cps3_canvas_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 384, 224);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cps3_canvas_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fatal_error("Failed to create framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    check_gl_error("FBO creation");

    gl_state.draw_rect_borders = Config_GetBool(CFG_KEY_DRAW_RECT_BORDERS);
    gl_state.dump_textures = Config_GetBool(CFG_KEY_DUMP_TEXTURES);

    gl_state.use_persistent_mapping = false;
#ifdef GL_ARB_buffer_storage
    if (GLAD_GL_ARB_buffer_storage) {
        gl_state.use_persistent_mapping = true;
        SDL_Log(
            "Optimized Path: GL_ARB_buffer_storage detected. Enabling Persistent Mapped Buffers (Triple Buffering).");
    } else {
        SDL_Log("Fallback Path: GL_ARB_buffer_storage missing. Using glBufferSubData.");
    }
#else
    SDL_Log("Fallback Path: Built without GL_ARB_buffer_storage. Using glBufferSubData.");
#endif

    const int buffer_count = gl_state.use_persistent_mapping ? OFFSET_BUFFER_COUNT : 1;

    for (int i = 0; i < RENDER_TASK_MAX; i++) {
        const int base = i * 4;
        const int idx = i * 6;
        gl_state.batch_indices[idx + 0] = base + 0;
        gl_state.batch_indices[idx + 1] = base + 1;
        gl_state.batch_indices[idx + 2] = base + 2;
        gl_state.batch_indices[idx + 3] = base + 2;
        gl_state.batch_indices[idx + 4] = base + 1;
        gl_state.batch_indices[idx + 5] = base + 3;
    }

    for (int i = 0; i < buffer_count; i++) {
        glGenVertexArrays(1, &gl_state.persistent_vaos[i]);
        glGenBuffers(1, &gl_state.persistent_vbos[i]);
        glGenBuffers(1, &gl_state.persistent_ebos[i]);
        glGenBuffers(1, &gl_state.persistent_layer_vbos[i]);
        glGenBuffers(1, &gl_state.persistent_pal_vbos[i]);

        glBindVertexArray(gl_state.persistent_vaos[i]);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_vbos[i]);
        if (gl_state.use_persistent_mapping) {
            const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            glBufferStorage(GL_ARRAY_BUFFER, sizeof(gl_state.batch_vertices), NULL, flags);
            gl_state.persistent_vbo_ptr[i] =
                (SDL_Vertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(gl_state.batch_vertices), flags);
        } else {
            glBufferData(GL_ARRAY_BUFFER, sizeof(gl_state.batch_vertices), NULL, GL_DYNAMIC_DRAW);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_state.persistent_ebos[i]);
        if (gl_state.use_persistent_mapping) {
            glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(gl_state.batch_indices), gl_state.batch_indices, 0);
        } else {
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER, sizeof(gl_state.batch_indices), gl_state.batch_indices, GL_STATIC_DRAW);
        }

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, tex_coord));

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_layer_vbos[i]);
        if (gl_state.use_persistent_mapping) {
            const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            glBufferStorage(GL_ARRAY_BUFFER, sizeof(gl_state.batch_layers), NULL, flags);
            gl_state.persistent_layer_ptr[i] =
                (float*)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(gl_state.batch_layers), flags);
        } else {
            glBufferData(GL_ARRAY_BUFFER, sizeof(gl_state.batch_layers), NULL, GL_DYNAMIC_DRAW);
        }
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_pal_vbos[i]);
        if (gl_state.use_persistent_mapping) {
            const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            glBufferStorage(GL_ARRAY_BUFFER, sizeof(gl_state.batch_pal_indices), NULL, flags);
            gl_state.persistent_pal_ptr[i] =
                (float*)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(gl_state.batch_pal_indices), flags);
        } else {
            glBufferData(GL_ARRAY_BUFFER, sizeof(gl_state.batch_pal_indices), NULL, GL_DYNAMIC_DRAW);
        }
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);

        glBindVertexArray(0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenTextures(1, &gl_state.white_texture);
    glBindTexture(GL_TEXTURE_2D, gl_state.white_texture);
    unsigned char white_pixel[4] = { 255, 255, 255, 255 };
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);

    glGenTextures(1, &gl_state.tex_array_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_id);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8UI, TEX_ARRAY_SIZE, TEX_ARRAY_SIZE, TEX_ARRAY_MAX_LAYERS);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl_state.tex_array_free_count = TEX_ARRAY_MAX_LAYERS;
    for (int i = 0; i < gl_state.tex_array_free_count; i++) {
        gl_state.tex_array_free[i] = TEX_ARRAY_MAX_LAYERS - 1 - i;
    }
    memset(gl_state.tex_array_layer, -1, sizeof(gl_state.tex_array_layer));

    // RGBA8 texture array for direct-color textures (PSMCT16/PSMCT32)
    glGenTextures(1, &gl_state.tex_array_rgba_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_rgba_id);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, TEX_ARRAY_SIZE, TEX_ARRAY_SIZE, TEX_ARRAY_RGBA_MAX_LAYERS);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl_state.tex_array_rgba_free_count = TEX_ARRAY_RGBA_MAX_LAYERS;
    for (int i = 0; i < gl_state.tex_array_rgba_free_count; i++) {
        gl_state.tex_array_rgba_free[i] = TEX_ARRAY_RGBA_MAX_LAYERS - 1 - i;
    }
    memset(gl_state.tex_array_rgba_layer, -1, sizeof(gl_state.tex_array_rgba_layer));

    // Reserve one RGBA array layer for the white pixel (solid-color quad support)
    {
        gl_state.white_array_layer = gl_state.tex_array_rgba_free[--gl_state.tex_array_rgba_free_count];
        const u32 white_pixel = 0xFFFFFFFFu;
        glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_rgba_id);
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY, 0, 0, 0, gl_state.white_array_layer, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    }

    tcache_live_init();

    glGenBuffers(1, &gl_state.palette_buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, gl_state.palette_buffer);
    glBufferData(GL_TEXTURE_BUFFER, PALETTE_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);

    glGenTextures(1, &gl_state.palette_tbo);
    glBindTexture(GL_TEXTURE_BUFFER, gl_state.palette_tbo);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, gl_state.palette_buffer);

    for (int i = 0; i < FL_PALETTE_MAX; ++i) {
        gl_state.palette_slots[i] = -1;
        gl_state.palette_slot_free[i] = true;
    }

    gl_state.use_pbo = gl_state.use_persistent_mapping;
    if (gl_state.use_pbo) {
        glGenBuffers(1, &gl_state.pbo_upload);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_state.pbo_upload);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, CONVERSION_BUFFER_BYTES, NULL, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        SDL_Log("Optimized Path: PBO async texture uploads enabled.");
    }

    gl_state.loc_projection = -1;
    gl_state.loc_source = -1;
    gl_state.arr_loc_projection = -1;
    gl_state.arr_loc_source = -1;
    gl_state.arr_loc_palette = -1;
    gl_state.arr_loc_source_rgba = -1;
}

void SDLGameRendererGL_Shutdown() {
    for (int i = 0; i < OFFSET_BUFFER_COUNT; i++) {
        if (gl_state.persistent_vaos[i])
            glDeleteVertexArrays(1, &gl_state.persistent_vaos[i]);
        if (gl_state.persistent_vbos[i])
            glDeleteBuffers(1, &gl_state.persistent_vbos[i]);
        if (gl_state.persistent_ebos[i])
            glDeleteBuffers(1, &gl_state.persistent_ebos[i]);
        if (gl_state.persistent_layer_vbos[i])
            glDeleteBuffers(1, &gl_state.persistent_layer_vbos[i]);
        if (gl_state.persistent_pal_vbos[i])
            glDeleteBuffers(1, &gl_state.persistent_pal_vbos[i]);
        if (gl_state.fences[i])
            glDeleteSync(gl_state.fences[i]);
    }
    if (gl_state.pbo_upload)
        glDeleteBuffers(1, &gl_state.pbo_upload);
    if (gl_state.tex_array_id)
        glDeleteTextures(1, &gl_state.tex_array_id);
    if (gl_state.tex_array_rgba_id)
        glDeleteTextures(1, &gl_state.tex_array_rgba_id);
    if (gl_state.palette_tbo)
        glDeleteTextures(1, &gl_state.palette_tbo);
    if (gl_state.palette_buffer)
        glDeleteBuffers(1, &gl_state.palette_buffer);
}

void SDLGameRendererGL_CreateTexture(unsigned int th) {
    const int texture_index = LO_16_BITS(th) - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (gl_state.surfaces[texture_index] != NULL) {
        SDL_DestroySurface(gl_state.surfaces[texture_index]);
        gl_state.surfaces[texture_index] = NULL;
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
    case SCE_GS_PSMCT32:
        pixel_format = SDL_PIXELFORMAT_ABGR8888;
        pitch = fl_texture->width * 4;
        break;
    default:
        return;
    }

    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
    gl_state.surfaces[texture_index] = surface;
}

void SDLGameRendererGL_DestroyTexture(unsigned int texture_handle) {
    const int texture_index = texture_handle - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    for (int i = gl_state.tcache_live_count - 1; i >= 0; i--) {
        if (gl_state.tcache_live[i].tex_idx == (uint16_t)texture_index) {
            int pal = gl_state.tcache_live[i].pal_idx;
            GLuint* texture_p = &gl_state.texture_cache[texture_index][pal];
            if (*texture_p != 0) {
                push_texture_to_destroy(*texture_p);
                *texture_p = 0;
            }
            GLuint* stale_p = &gl_state.stale_texture_cache[texture_index][pal];
            if (*stale_p != 0) {
                push_texture_to_destroy(*stale_p);
                *stale_p = 0;
            }
            gl_state.texture_cache_w[texture_index][pal] = 0;
            gl_state.texture_cache_h[texture_index][pal] = 0;

            if (gl_state.tex_array_rgba_layer[texture_index][pal] >= 0) {
                gl_state.tex_array_rgba_free[gl_state.tex_array_rgba_free_count++] =
                    gl_state.tex_array_rgba_layer[texture_index][pal];
                gl_state.tex_array_rgba_layer[texture_index][pal] = -1;
            }
            gl_state.tcache_live[i] = gl_state.tcache_live[--gl_state.tcache_live_count];
        }
    }

    // R8UI layer is per-texture (shared across palettes) — free once
    if (gl_state.tex_array_layer[texture_index] >= 0) {
        gl_state.tex_array_free[gl_state.tex_array_free_count++] = gl_state.tex_array_layer[texture_index];
        gl_state.tex_array_layer[texture_index] = -1;
    }

    if (gl_state.surfaces[texture_index] != NULL) {
        SDL_DestroySurface(gl_state.surfaces[texture_index]);
        gl_state.surfaces[texture_index] = NULL;
    }
}

void SDLGameRendererGL_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;

    int slot = gl_state.palette_slots[palette_index];
    if (slot < 0) {
        for (int i = 0; i < FL_PALETTE_MAX; ++i) {
            if (gl_state.palette_slot_free[i]) {
                slot = i;
                gl_state.palette_slot_free[i] = false;
                break;
            }
        }
    }
    gl_state.palette_slots[palette_index] = slot;
    if (slot < 0)
        return;

    float color_data[256 * 4];
    size_t color_size = (fl_palette->format == SCE_GS_PSMCT32) ? 4 : 2;

    switch (color_count) {
    case 16:
        if (color_size == 4) {
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 16; i++)
                read_rgba32_color(rgba32[i], &color_data[i * 4]);
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 16; i++)
                read_rgba16_color(rgba16[i], &color_data[i * 4]);
        }
        color_data[3] = 0.0f;
        break;

    case 256:
        if (color_size == 4) {
            // ⚡ Bolt: SIMD RGBA32→float4 — process 4 colors per iteration
            const Uint32* rgba32 = (const Uint32*)pixels;
            const simde__m128 inv255 = simde_mm_set1_ps(1.0f / 255.0f);
            for (int i = 0; i < 256; i += 4) {
                for (int j = 0; j < 4; j++) {
                    Uint32 px = rgba32[ps2_clut_shuffle[i + j]];
                    simde__m128i ci =
                        simde_mm_set_epi32((px >> 24) & 0xFF, (px >> 16) & 0xFF, (px >> 8) & 0xFF, px & 0xFF);
                    simde__m128 cf = simde_mm_mul_ps(simde_mm_cvtepi32_ps(ci), inv255);
                    simde_mm_storeu_ps(&color_data[(i + j) * 4], cf);
                }
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba16_color(rgba16[ps2_clut_shuffle[i]], &color_data[i * 4]);
        }
        color_data[3] = 0.0f;
        break;
    }

    glBindBuffer(GL_TEXTURE_BUFFER, gl_state.palette_buffer);
    glBufferSubData(GL_TEXTURE_BUFFER, slot * 256 * 4 * sizeof(float), color_count * 4 * sizeof(float), color_data);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    // ⚡ Bolt: SIMD float→u8 pack — 4 colors (16 floats → 16 bytes) per iteration
    SDL_Color sdl_colors[256];
    {
        const simde__m128 scale = simde_mm_set1_ps(255.0f);
        int i = 0;
        for (; i + 3 < color_count; i += 4) {
            // Load 4 colors (16 floats), scale to 0–255, convert to int32
            simde__m128i c0 = simde_mm_cvtps_epi32(simde_mm_mul_ps(simde_mm_loadu_ps(&color_data[(i + 0) * 4]), scale));
            simde__m128i c1 = simde_mm_cvtps_epi32(simde_mm_mul_ps(simde_mm_loadu_ps(&color_data[(i + 1) * 4]), scale));
            simde__m128i c2 = simde_mm_cvtps_epi32(simde_mm_mul_ps(simde_mm_loadu_ps(&color_data[(i + 2) * 4]), scale));
            simde__m128i c3 = simde_mm_cvtps_epi32(simde_mm_mul_ps(simde_mm_loadu_ps(&color_data[(i + 3) * 4]), scale));
            // Pack i32→i16→u8: [R,G,B,A, R,G,B,A, R,G,B,A, R,G,B,A] → 16 bytes
            simde__m128i p01 = simde_mm_packs_epi32(c0, c1);       // 8 × i16
            simde__m128i p23 = simde_mm_packs_epi32(c2, c3);       // 8 × i16
            simde__m128i packed = simde_mm_packus_epi16(p01, p23); // 16 × u8
            // Store 16 bytes = 4 SDL_Color structs
            simde_mm_storeu_si128((simde__m128i*)&sdl_colors[i], packed);
        }
        // Scalar tail
        for (; i < color_count; ++i) {
            sdl_colors[i].r = (Uint8)(color_data[i * 4 + 0] * 255.0f);
            sdl_colors[i].g = (Uint8)(color_data[i * 4 + 1] * 255.0f);
            sdl_colors[i].b = (Uint8)(color_data[i * 4 + 2] * 255.0f);
            sdl_colors[i].a = (Uint8)(color_data[i * 4 + 3] * 255.0f);
        }
    }
    SDL_Palette* palette = SDL_CreatePalette(color_count);
    SDL_SetPaletteColors(palette, sdl_colors, 0, color_count);
    gl_state.palettes[palette_index] = palette;
}

void SDLGameRendererGL_DestroyPalette(unsigned int palette_handle) {
    const int palette_index = palette_handle - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;

    for (int i = gl_state.tcache_live_count - 1; i >= 0; i--) {
        if (gl_state.tcache_live[i].pal_idx == (uint16_t)palette_handle) {
            int tex = gl_state.tcache_live[i].tex_idx;
            GLuint* texture_p = &gl_state.texture_cache[tex][palette_handle];
            if (*texture_p != 0) {
                push_texture_to_destroy(*texture_p);
                *texture_p = 0;
            }
            GLuint* stale_p = &gl_state.stale_texture_cache[tex][palette_handle];
            if (*stale_p != 0) {
                push_texture_to_destroy(*stale_p);
                *stale_p = 0;
            }
            gl_state.texture_cache_w[tex][palette_handle] = 0;
            gl_state.texture_cache_h[tex][palette_handle] = 0;
            // R8UI layer is per-texture, not per-palette — don't free here
            if (gl_state.tex_array_rgba_layer[tex][palette_handle] >= 0) {
                gl_state.tex_array_rgba_free[gl_state.tex_array_rgba_free_count++] =
                    gl_state.tex_array_rgba_layer[tex][palette_handle];
                gl_state.tex_array_rgba_layer[tex][palette_handle] = -1;
            }
            gl_state.tcache_live[i] = gl_state.tcache_live[--gl_state.tcache_live_count];
        }
    }

    int slot = gl_state.palette_slots[palette_index];
    if (slot >= 0) {
        gl_state.palette_slot_free[slot] = true;
        gl_state.palette_slots[palette_index] = -1;
    }
}

void SDLGameRendererGL_UnlockPalette(unsigned int ph) {
    const int palette_handle = ph;
    if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX) {
        if (!gl_state.palette_dirty_flags[palette_handle - 1]) {
            gl_state.palette_dirty_flags[palette_handle - 1] = true;
            gl_state.dirty_palette_indices[gl_state.dirty_palette_count++] = palette_handle - 1;
        }
    }
}

/**
 * ⚡ Bolt: O(1) deferred unlock — pushes tex_idx to pending batch instead of
 * scanning tcache_live per call. The actual stale promotion happens in
 * SDLGameRendererGL_FlushPendingUnlocks() as a single O(pending + live) pass.
 */
void SDLGameRendererGL_UnlockTexture(unsigned int th) {
    const int texture_handle = th;
    if (texture_handle > 0 && texture_handle <= FL_TEXTURE_MAX) {
        const int tex_idx = texture_handle - 1;

        // ⚡ Bolt: Defer tcache_live scan — just push to pending batch
        if (!gl_state.pending_unlock_flags[tex_idx]) {
            gl_state.pending_unlock_flags[tex_idx] = true;
            gl_state.pending_unlock_indices[gl_state.pending_unlock_count++] = tex_idx;
        }

        if (!gl_state.texture_dirty_flags[texture_handle - 1]) {
            gl_state.texture_dirty_flags[texture_handle - 1] = true;
            gl_state.dirty_texture_indices[gl_state.dirty_texture_count++] = texture_handle - 1;
        }
    }
}

/**
 * ⚡ Bolt: Batch stale-promotion pass — single O(pending + live) scan replaces
 * the previous O(dirty × live) per-UnlockTexture approach.
 *
 * For each tcache_live entry whose tex_idx is in the pending set, promotes
 * texture_cache → stale_texture_cache (so SetTexture can reuse the GL object
 * via glTexSubImage2D instead of glGenTextures). Called once between Phase 1
 * (texture upload) and Phase 2 (sprite draw) of seqsAfterProcess.
 */
void SDLGameRendererGL_FlushPendingUnlocks(void) {
    if (gl_state.pending_unlock_count == 0)
        return;

    // Single reverse pass through tcache_live — check each entry against the boolean lookup
    for (int i = gl_state.tcache_live_count - 1; i >= 0; i--) {
        const int tex_idx = gl_state.tcache_live[i].tex_idx;
        if (gl_state.pending_unlock_flags[tex_idx]) {
            int pal = gl_state.tcache_live[i].pal_idx;
            GLuint* texture_p = &gl_state.texture_cache[tex_idx][pal];
            if (*texture_p != 0) {
                GLuint stale = gl_state.stale_texture_cache[tex_idx][pal];
                if (stale != 0)
                    push_texture_to_destroy(stale);
                gl_state.stale_texture_cache[tex_idx][pal] = *texture_p;
                *texture_p = 0;
            }
            gl_state.tcache_live[i] = gl_state.tcache_live[--gl_state.tcache_live_count];
        }
    }

    // Clear the pending set
    for (int i = 0; i < gl_state.pending_unlock_count; i++) {
        gl_state.pending_unlock_flags[gl_state.pending_unlock_indices[i]] = false;
    }
    gl_state.pending_unlock_count = 0;
}

void SDLGameRendererGL_SetTexture(unsigned int th) {
    // TRACE_ZONE_N("SetTexture"); // Commented out to reduce header deps for now
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;

    if (th == gl_state.last_set_texture_th && gl_state.texture_count > 0) {
        push_texture_with_layer(gl_state.textures[gl_state.texture_count - 1],
                                gl_state.texture_layers[gl_state.texture_count - 1],
                                gl_state.texture_pal_slots[gl_state.texture_count - 1],
                                gl_state.texture_uv_sx[gl_state.texture_count - 1],
                                gl_state.texture_uv_sy[gl_state.texture_count - 1]);
        return;
    }
    gl_state.last_set_texture_th = th;

    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX)
        fatal_error("Invalid texture handle");

    SDL_Surface* surface = gl_state.surfaces[texture_handle - 1];
    if (!surface)
        fatal_error("Surface is NULL");

    GLuint texture = gl_state.texture_cache[texture_handle - 1][palette_handle];

    if (texture == 0) {
        const FLTexture* fl_texture = &flTexture[texture_handle - 1];

        const int16_t cached_w = gl_state.texture_cache_w[texture_handle - 1][palette_handle];
        const int16_t cached_h = gl_state.texture_cache_h[texture_handle - 1][palette_handle];
        GLuint stale = gl_state.stale_texture_cache[texture_handle - 1][palette_handle];
        const bool can_sub_image = (stale != 0 && cached_w == surface->w && cached_h == surface->h);

        if (can_sub_image) {
            texture = stale;
            gl_state.stale_texture_cache[texture_handle - 1][palette_handle] = 0;
        } else {
            if (stale != 0) {
                push_texture_to_destroy(stale);
                gl_state.stale_texture_cache[texture_handle - 1][palette_handle] = 0;
            }
            glGenTextures(1, &texture);
        }
        glBindTexture(GL_TEXTURE_2D, texture);

        const bool is_16bit = (fl_texture->format == SCE_GS_PSMCT16);

        if (is_16bit) {
            // PSMCT16: direct-color — go straight to RGBA array, skip R8UI entirely
            int rgba_layer = gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle];
            if (rgba_layer < 0 && surface->w <= TEX_ARRAY_SIZE && surface->h <= TEX_ARRAY_SIZE &&
                gl_state.tex_array_rgba_free_count > 0) {
                rgba_layer = gl_state.tex_array_rgba_free[--gl_state.tex_array_rgba_free_count];
                gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle] = (int16_t)rgba_layer;
            }

            if (rgba_layer >= 0) {
                // Convert PSMCT16 → RGBA8 into conversion_buffer
                u32* conv_buf = gl_state.conversion_buffer;
                const simde__m128i mask5 = simde_mm_set1_epi32(0x1F);
                const simde__m128i mask_a = simde_mm_set1_epi32(0x8000);
                const simde__m128i alpha_ff = simde_mm_set1_epi32((int)0xFF000000u);
                const simde__m128i zero = simde_mm_setzero_si128();
                const Uint8* src_bytes = (const Uint8*)surface->pixels;
                for (int y = 0; y < surface->h; y++) {
                    const Uint16* row = (const Uint16*)(src_bytes + y * surface->pitch);
                    u32* out_row = conv_buf + y * surface->w;
                    int x = 0;
                    for (; x + 7 < surface->w; x += 8) {
                        simde__m128i px = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                        simde__m128i lo32 = simde_mm_unpacklo_epi16(px, zero);
                        simde__m128i hi32 = simde_mm_unpackhi_epi16(px, zero);
                        for (int half = 0; half < 2; half++) {
                            simde__m128i v = (half == 0) ? lo32 : hi32;
                            simde__m128i r = simde_mm_and_si128(v, mask5);
                            simde__m128i g = simde_mm_and_si128(simde_mm_srli_epi32(v, 5), mask5);
                            simde__m128i b = simde_mm_and_si128(simde_mm_srli_epi32(v, 10), mask5);
                            simde__m128i a = simde_mm_and_si128(v, mask_a);
                            r = simde_mm_or_si128(simde_mm_slli_epi32(r, 3), simde_mm_srli_epi32(r, 2));
                            g = simde_mm_or_si128(simde_mm_slli_epi32(g, 3), simde_mm_srli_epi32(g, 2));
                            b = simde_mm_or_si128(simde_mm_slli_epi32(b, 3), simde_mm_srli_epi32(b, 2));
                            a = simde_mm_and_si128(simde_mm_cmpeq_epi32(a, mask_a), alpha_ff);
                            simde__m128i result = simde_mm_or_si128(a, r);
                            result = simde_mm_or_si128(result, simde_mm_slli_epi32(g, 8));
                            result = simde_mm_or_si128(result, simde_mm_slli_epi32(b, 16));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + half * 4), result);
                        }
                    }
                    for (; x < surface->w; x++) {
                        float rgba[4];
                        read_rgba16_color(row[x], rgba);
                        out_row[x] = ((Uint32)(rgba[3] * 255) << 24) | ((Uint32)(rgba[2] * 255) << 16) |
                                     ((Uint32)(rgba[1] * 255) << 8) | (Uint32)(rgba[0] * 255);
                    }
                }

                // Upload to RGBA texture array
                glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_rgba_id);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                0,
                                0,
                                0,
                                rgba_layer,
                                surface->w,
                                surface->h,
                                1,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                conv_buf);
                glBindTexture(GL_TEXTURE_2D, texture);
            }
            // If rgba_layer < 0 (array full), falls through to legacy path below
        } else {
            // Indexed formats (PSMT8/PSMT4/PSMCT32): use R8UI array
            int direct_layer = gl_state.tex_array_layer[texture_handle - 1];
            if (direct_layer < 0 && surface->w <= TEX_ARRAY_SIZE && surface->h <= TEX_ARRAY_SIZE &&
                gl_state.tex_array_free_count > 0) {
                direct_layer = gl_state.tex_array_free[--gl_state.tex_array_free_count];
                gl_state.tex_array_layer[texture_handle - 1] = (int16_t)direct_layer;
            }

            if (direct_layer >= 0) {
                Uint8* pixel_data = (Uint8*)gl_state.conversion_buffer;
                const int pixel_count = surface->w * surface->h;

                if (fl_texture->format == SCE_GS_PSMT4) {
                    // ⚡ Bolt: SIMD 4-bit → R8 nibble unpack (ported from GPU backend)
                    const Uint8* src = (const Uint8*)surface->pixels;
                    const simde__m128i lo_mask = simde_mm_set1_epi8(0x0F);
                    for (int y = 0; y < surface->h; ++y) {
                        const Uint8* row = src + y * surface->pitch;
                        Uint8* dst_row = pixel_data + y * surface->w;
                        int x = 0;
                        // Process 16 input bytes = 32 output bytes at a time
                        for (; x + 31 < surface->w; x += 32) {
                            simde__m128i packed = simde_mm_loadu_si128((const simde__m128i*)(row + x / 2));
                            simde__m128i lo = simde_mm_and_si128(packed, lo_mask);
                            simde__m128i hi = simde_mm_and_si128(simde_mm_srli_epi16(packed, 4), lo_mask);
                            simde__m128i out_lo = simde_mm_unpacklo_epi8(lo, hi);
                            simde__m128i out_hi = simde_mm_unpackhi_epi8(lo, hi);
                            simde_mm_storeu_si128((simde__m128i*)(dst_row + x), out_lo);
                            simde_mm_storeu_si128((simde__m128i*)(dst_row + x + 16), out_hi);
                        }
                        // Scalar tail
                        for (; x < surface->w; x += 2) {
                            Uint8 b = row[x / 2];
                            dst_row[x] = b & 0x0F;
                            if (x + 1 < surface->w)
                                dst_row[x + 1] = (b >> 4) & 0x0F;
                        }
                    }
                } else {
                    memcpy(pixel_data, surface->pixels, pixel_count);
                }

                glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_id);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                0,
                                0,
                                0,
                                direct_layer,
                                surface->w,
                                surface->h,
                                1,
                                GL_RED_INTEGER,
                                GL_UNSIGNED_BYTE,
                                pixel_data);
                glBindTexture(GL_TEXTURE_2D, texture);
            } else if (palette_handle > 0 && surface->w <= TEX_ARRAY_SIZE && surface->h <= TEX_ARRAY_SIZE) {
                // R8UI array full — fall back to RGBA array with pre-resolved palette
                int rgba_layer = gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle];
                if (rgba_layer < 0 && gl_state.tex_array_rgba_free_count > 0) {
                    rgba_layer = gl_state.tex_array_rgba_free[--gl_state.tex_array_rgba_free_count];
                    gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle] = (int16_t)rgba_layer;
                }
                if (rgba_layer >= 0) {
                    u32* conv_buf = gl_state.conversion_buffer;
                    SDL_Palette* pal = gl_state.palettes[palette_handle - 1];
                    const Uint8* src = (const Uint8*)surface->pixels;
                    const int pixel_count = surface->w * surface->h;
                    for (int pi = 0; pi < pixel_count; ++pi) {
                        int idx;
                        if (fl_texture->format == SCE_GS_PSMT4) {
                            Uint8 b = src[pi / 2];
                            idx = (pi & 1) ? (b >> 4) : (b & 0xF);
                        } else {
                            idx = src[pi];
                        }
                        SDL_Color c = pal->colors[idx];
                        conv_buf[pi] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r;
                    }

                    glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_rgba_id);
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                    0,
                                    0,
                                    0,
                                    rgba_layer,
                                    surface->w,
                                    surface->h,
                                    1,
                                    GL_RGBA,
                                    GL_UNSIGNED_BYTE,
                                    conv_buf);
                    glBindTexture(GL_TEXTURE_2D, texture);
                }
            }
        }

        // Legacy fallback: only if BOTH R8UI and RGBA arrays don't have a layer
        if (gl_state.tex_array_layer[texture_handle - 1] < 0 &&
            gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle] < 0) {
            u32* conv_buf = gl_state.conversion_buffer;
            const int pixel_count = surface->w * surface->h;

            if (fl_texture->format == SCE_GS_PSMCT16) {
                // PSMCT16 legacy fallback (RGBA array was full)
                const simde__m128i mask5 = simde_mm_set1_epi32(0x1F);
                const simde__m128i mask_a = simde_mm_set1_epi32(0x8000);
                const simde__m128i alpha_ff = simde_mm_set1_epi32((int)0xFF000000u);
                const simde__m128i zero = simde_mm_setzero_si128();
                const Uint8* src_bytes = (const Uint8*)surface->pixels;
                for (int y = 0; y < surface->h; y++) {
                    const Uint16* row = (const Uint16*)(src_bytes + y * surface->pitch);
                    u32* out_row = conv_buf + y * surface->w;
                    int x = 0;
                    for (; x + 7 < surface->w; x += 8) {
                        simde__m128i px = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                        simde__m128i lo32 = simde_mm_unpacklo_epi16(px, zero);
                        simde__m128i hi32 = simde_mm_unpackhi_epi16(px, zero);
                        for (int half = 0; half < 2; half++) {
                            simde__m128i v = (half == 0) ? lo32 : hi32;
                            simde__m128i r = simde_mm_and_si128(v, mask5);
                            simde__m128i g = simde_mm_and_si128(simde_mm_srli_epi32(v, 5), mask5);
                            simde__m128i b = simde_mm_and_si128(simde_mm_srli_epi32(v, 10), mask5);
                            simde__m128i a = simde_mm_and_si128(v, mask_a);
                            r = simde_mm_or_si128(simde_mm_slli_epi32(r, 3), simde_mm_srli_epi32(r, 2));
                            g = simde_mm_or_si128(simde_mm_slli_epi32(g, 3), simde_mm_srli_epi32(g, 2));
                            b = simde_mm_or_si128(simde_mm_slli_epi32(b, 3), simde_mm_srli_epi32(b, 2));
                            a = simde_mm_and_si128(simde_mm_cmpeq_epi32(a, mask_a), alpha_ff);
                            simde__m128i result = simde_mm_or_si128(a, r);
                            result = simde_mm_or_si128(result, simde_mm_slli_epi32(g, 8));
                            result = simde_mm_or_si128(result, simde_mm_slli_epi32(b, 16));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + half * 4), result);
                        }
                    }
                    for (; x < surface->w; x++) {
                        float rgba[4];
                        read_rgba16_color(row[x], rgba);
                        out_row[x] = ((Uint32)(rgba[3] * 255) << 24) | ((Uint32)(rgba[2] * 255) << 16) |
                                     ((Uint32)(rgba[1] * 255) << 8) | (Uint32)(rgba[0] * 255);
                    }
                }
            } else if (palette_handle > 0) {
                SDL_Palette* pal = gl_state.palettes[palette_handle - 1];
                const Uint8* src = (const Uint8*)surface->pixels;
                for (int i = 0; i < pixel_count; ++i) {
                    int idx;
                    if (fl_texture->format == SCE_GS_PSMT4) {
                        Uint8 b = src[i / 2];
                        idx = (i & 1) ? (b >> 4) : (b & 0xF);
                    } else {
                        idx = src[i];
                    }
                    SDL_Color c = pal->colors[idx];
                    conv_buf[i] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r;
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv_buf);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        gl_state.texture_cache_w[texture_handle - 1][palette_handle] = (int16_t)surface->w;
        gl_state.texture_cache_h[texture_handle - 1][palette_handle] = (int16_t)surface->h;
        gl_state.texture_cache[texture_handle - 1][palette_handle] = texture;
        tcache_live_add(texture_handle - 1, palette_handle);
    }

    {
        int layer = gl_state.tex_array_layer[texture_handle - 1];
        if (layer < 0) {
            // Check RGBA array — encode as negative: -(rgba_layer + 2)
            int rgba_layer = gl_state.tex_array_rgba_layer[texture_handle - 1][palette_handle];
            if (rgba_layer >= 0) {
                layer = -(rgba_layer + 2);
            }
        }
        int pal_slot = (palette_handle > 0) ? gl_state.palette_slots[palette_handle - 1] : 0;
        float uv_sx = (float)surface->w / (float)TEX_ARRAY_SIZE;
        float uv_sy = (float)surface->h / (float)TEX_ARRAY_SIZE;
        push_texture_with_layer(texture, layer, pal_slot, uv_sx, uv_sy);
    }
}

unsigned int SDLGameRendererGL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX)
        return 0;
    if (palette_handle > FL_PALETTE_MAX)
        return 0;

    GLuint cached = gl_state.texture_cache[texture_handle - 1][palette_handle];
    if (cached != 0)
        return cached;

    unsigned int combined = (palette_handle << 16) | texture_handle;
    SDLGameRendererGL_SetTexture(combined);
    return gl_state.texture_cache[texture_handle - 1][palette_handle];
}

void SDLGameRendererGL_DumpTextures(void) {
    SDL_CreateDirectory("textures");
    int tex_index = 0;
    int count = 0;

    // Use tcache_live — the authoritative list of tex+pal pairs that have been rendered.
    for (int li = 0; li < gl_state.tcache_live_count; li++) {
        int ti = gl_state.tcache_live[li].tex_idx;
        int pi = gl_state.tcache_live[li].pal_idx;

        SDL_Surface* surf = gl_state.surfaces[ti];
        if (!surf || !SDL_ISPIXELFORMAT_INDEXED(surf->format))
            continue;

        SDL_Palette* pal = (pi > 0 && pi <= FL_PALETTE_MAX) ? gl_state.palettes[pi - 1] : NULL;
        if (!pal)
            continue;

        char filename[128];
        snprintf(filename, sizeof(filename), "textures/%d_t%d_p%d.tga", tex_index++, ti, pi);
        FILE* f = fopen(filename, "wb");
        if (!f)
            continue;

        const Uint8* pixels = (const Uint8*)surf->pixels;
        const int w = surf->w, h = surf->h;
        uint8_t header[18] = { 0 };
        header[2] = 2;
        header[12] = w & 0xFF;
        header[13] = (w >> 8) & 0xFF;
        header[14] = h & 0xFF;
        header[15] = (h >> 8) & 0xFF;
        header[16] = 32;
        header[17] = 0x20;
        fwrite(header, 1, 18, f);

        for (int i = 0; i < w * h; i++) {
            Uint8 idx;
            if (pal->ncolors == 16) {
                Uint8 byte = pixels[i / 2];
                idx = (i & 1) ? (byte >> 4) : (byte & 0x0F);
            } else {
                idx = pixels[i];
            }
            // GL palette has R and B swapped vs SDL convention (bits16-23 of PS2 uint32 → .r)
            // so write .r as TGA-B and .b as TGA-R to get correct display
            const SDL_Color* c = &pal->colors[idx];
            Uint8 bgra[] = { c->b, c->g, c->r, c->a };
            fwrite(bgra, 1, 4, f);
        }
        fclose(f);
        count++;
    }

    SDL_Log("[TextureDump] Wrote %d texture(s) to textures/", count);
}

/**
 * Scans tcache_live and prints per-texture palette-count stats.
 * Run in-game (same trigger as DumpTextures) to calibrate IDX_PAL_SLOTS
 * for the SDL2D multi-slot palette cache.
 */
void SDLGameRendererGL_DumpPaletteStats(void) {
    // Count palettes per texture index
    static uint8_t pal_count[FL_TEXTURE_MAX];
    memset(pal_count, 0, sizeof(pal_count));

    int total_pairs = gl_state.tcache_live_count;
    for (int li = 0; li < total_pairs; li++) {
        int ti = gl_state.tcache_live[li].tex_idx;
        if (ti >= 0 && ti < FL_TEXTURE_MAX && pal_count[ti] < 255)
            pal_count[ti]++;
    }

    // Build histogram: how many textures have exactly N palettes
    int hist[33] = { 0 }; // 0..31, overflow bucket at [32]
    int max_pals = 0;
    int textures_with_pals = 0;
    for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
        if (pal_count[ti] == 0)
            continue;
        textures_with_pals++;
        if (pal_count[ti] > max_pals)
            max_pals = pal_count[ti];
        int bucket = (pal_count[ti] <= 32) ? pal_count[ti] : 32;
        hist[bucket]++;
    }

    SDL_Log("[PaletteStats] tcache_live=%d pairs, %d textures with palettes, max_pals_per_tex=%d",
            total_pairs,
            textures_with_pals,
            max_pals);

    // Print histogram
    for (int n = 1; n <= (max_pals < 32 ? max_pals : 32); n++) {
        if (hist[n] > 0)
            SDL_Log("[PaletteStats]   %2d pal(s): %d texture(s)", n, hist[n]);
    }
    if (hist[32] > 0)
        SDL_Log("[PaletteStats]  >32 pal(s): %d texture(s)", hist[32]);

    // Print top outliers (textures with the most palettes)
    SDL_Log("[PaletteStats] Top outliers (tex_idx: pal_count: WxH):");
    for (int rank = 0; rank < 10; rank++) {
        int best_ti = -1, best_n = 0;
        for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
            if (pal_count[ti] > best_n) {
                best_n = pal_count[ti];
                best_ti = ti;
            }
        }
        if (best_ti < 0 || best_n == 0)
            break;
        SDL_Surface* surf = gl_state.surfaces[best_ti];
        if (surf)
            SDL_Log("[PaletteStats]   tex[%4d]: %d palettes (%dx%d)", best_ti, best_n, surf->w, surf->h);
        else
            SDL_Log("[PaletteStats]   tex[%4d]: %d palettes (no surface)", best_ti, best_n);
        pal_count[best_ti] = 0; // remove from future ranking
    }
}
