/**
 * @file sdl_game_renderer_gl_resources.c
 * @brief OpenGL renderer texture and palette resource management.
 *
 * Handles creation, destruction, upload, and caching of OpenGL textures
 * and palettes. Implements PS2 CLUT shuffle, palette hashing for dirty
 * detection, and the texture cache live-set. Part of the GL rendering backend.
 */
#include "port/config.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_app_config.h"
#include "port/sdl/sdl_game_renderer_gl_internal.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <libgraph.h>
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

// FNV-1a hash of palette color data — fast, good distribution, 1 cache line
static inline uint32_t hash_palette(const void* data, size_t size) {
    uint32_t h = 2166136261u;
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

// --- CLUT Shuffle for PS2 ---
#define clut_shuf(x) (((x) & ~0x18) | ((((x) & 0x08) << 1) | (((x) & 0x10) >> 1)))

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
    if (gl_state.texture_count >= FL_PALETTE_MAX) {
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

            if (gl_state.tex_array_layer[texture_index][pal] >= 0) {
                gl_state.tex_array_free[gl_state.tex_array_free_count++] = gl_state.tex_array_layer[texture_index][pal];
                gl_state.tex_array_layer[texture_index][pal] = -1;
            }
            gl_state.tcache_live[i] = gl_state.tcache_live[--gl_state.tcache_live_count];
        }
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
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba32_color(rgba32[clut_shuf(i)], &color_data[i * 4]);
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba16_color(rgba16[clut_shuf(i)], &color_data[i * 4]);
        }
        color_data[3] = 0.0f;
        break;
    }

    glBindBuffer(GL_TEXTURE_BUFFER, gl_state.palette_buffer);
    glBufferSubData(GL_TEXTURE_BUFFER, slot * 256 * 4 * sizeof(float), color_count * 4 * sizeof(float), color_data);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    SDL_Color sdl_colors[256];
    for (int i = 0; i < color_count; ++i) {
        sdl_colors[i].r = (Uint8)(color_data[i * 4 + 0] * 255.0f);
        sdl_colors[i].g = (Uint8)(color_data[i * 4 + 1] * 255.0f);
        sdl_colors[i].b = (Uint8)(color_data[i * 4 + 2] * 255.0f);
        sdl_colors[i].a = (Uint8)(color_data[i * 4 + 3] * 255.0f);
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
            if (gl_state.tex_array_layer[tex][palette_handle] >= 0) {
                gl_state.tex_array_free[gl_state.tex_array_free_count++] =
                    gl_state.tex_array_layer[tex][palette_handle];
                gl_state.tex_array_layer[tex][palette_handle] = -1;
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
        const FLTexture* fl_pal = &flPalette[palette_handle - 1];
        const void* pixels = flPS2GetSystemBuffAdrs(fl_pal->mem_handle);
        size_t size = fl_pal->width * fl_pal->height * ((fl_pal->format == SCE_GS_PSMCT32) ? 4 : 2);

        if (pixels) {
            uint32_t new_hash = hash_palette(pixels, size);
            if (new_hash == gl_state.palette_hash[palette_handle - 1]) {
                return;
            }
            gl_state.palette_hash[palette_handle - 1] = new_hash;
        }

        if (!gl_state.palette_dirty_flags[palette_handle - 1]) {
            gl_state.palette_dirty_flags[palette_handle - 1] = true;
            gl_state.dirty_palette_indices[gl_state.dirty_palette_count++] = palette_handle - 1;
        }
    }
}

void SDLGameRendererGL_UnlockTexture(unsigned int th) {
    const int texture_handle = th;
    if (texture_handle > 0 && texture_handle <= FL_TEXTURE_MAX) {
        const int tex_idx = texture_handle - 1;

        for (int i = gl_state.tcache_live_count - 1; i >= 0; i--) {
            if (gl_state.tcache_live[i].tex_idx == (uint16_t)tex_idx) {
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
        if (!gl_state.texture_dirty_flags[texture_handle - 1]) {
            gl_state.texture_dirty_flags[texture_handle - 1] = true;
            gl_state.dirty_texture_indices[gl_state.dirty_texture_count++] = texture_handle - 1;
        }
    }
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

        int direct_layer = gl_state.tex_array_layer[texture_handle - 1][palette_handle];
        if (direct_layer < 0 && surface->w <= TEX_ARRAY_SIZE && surface->h <= TEX_ARRAY_SIZE &&
            gl_state.tex_array_free_count > 0) {
            direct_layer = gl_state.tex_array_free[--gl_state.tex_array_free_count];
            gl_state.tex_array_layer[texture_handle - 1][palette_handle] = (int16_t)direct_layer;
        }

        if (direct_layer >= 0) {
            bool is_16bit = (fl_texture->format == SCE_GS_PSMCT16);

            if (is_16bit) {
                gl_state.tex_array_free[gl_state.tex_array_free_count++] = direct_layer;
                gl_state.tex_array_layer[texture_handle - 1][palette_handle] = -1;
                direct_layer = -1;
            } else {
                Uint8* pixel_data = (Uint8*)gl_state.conversion_buffer;
                const int pixel_count = surface->w * surface->h;

                if (fl_texture->format == SCE_GS_PSMT4) {
                    const Uint8* src = (const Uint8*)surface->pixels;
                    for (int y = 0; y < surface->h; ++y) {
                        const Uint8* row = src + y * surface->pitch;
                        Uint8* dst_row = pixel_data + y * surface->w;
                        for (int x = 0; x < surface->w; ++x) {
                            Uint8 b = row[x / 2];
                            dst_row[x] = (x & 1) ? (b >> 4) : (b & 0xF);
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
            }
        }

        if (direct_layer < 0) {
            u32* conv_buf = gl_state.conversion_buffer;
            const int pixel_count = surface->w * surface->h;

            if (fl_texture->format == SCE_GS_PSMCT16) {
                const Uint16* src = (const Uint16*)surface->pixels;
                for (int i = 0; i < pixel_count; ++i) {
                    float rgba[4];
                    read_rgba16_color(src[i], rgba);
                    conv_buf[i] = ((Uint32)(rgba[3] * 255) << 24) | ((Uint32)(rgba[2] * 255) << 16) |
                                  ((Uint32)(rgba[1] * 255) << 8) | (Uint32)(rgba[0] * 255);
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
        int layer = gl_state.tex_array_layer[texture_handle - 1][palette_handle];
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
