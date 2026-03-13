/**
 * @file sdl_game_renderer_gpu.c
 * @brief SDL_GPU rendering backend implementation.
 *
 * Full renderer using SDL3's GPU API with compute shader-based texture
 * decoding, batched vertex rendering, and palette management. Alternative
 * to the OpenGL backend for platforms with SDL_GPU support.
 */
#include "sdl_game_renderer_gpu_internal.h"

#include "port/mods/modded_stage.h"

#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>
/* ─── Global Variable Definitions ─────────────────────────────────────── */
/* Declared extern in sdl_game_renderer_gpu_internal.h                     */

SDL_GPUDevice* device = NULL;
LZ77Context* s_lz77_ctx = NULL;
SDL_Window* gpu_window = NULL;
SDL_GPUCommandBuffer* current_cmd_buf = NULL;
SDL_GPUGraphicsPipeline* pipeline = NULL;
SDL_GPUTexture* s_palette_texture = NULL;
SDL_GPUTransferBuffer* s_palette_transfer = NULL;
SDL_GPUSampler* palette_sampler = NULL;
SDL_GPUSampler* sampler = NULL;

SDL_GPUBuffer* vertex_buffer = NULL;
SDL_GPUBuffer* index_buffer = NULL;
SDL_GPUTransferBuffer* transfer_buffers[VERTEX_TRANSFER_BUFFER_COUNT] = { NULL };
SDL_GPUTransferBuffer* index_transfer_buffer = NULL;
int current_transfer_idx = 0;

SDL_GPUFence* s_frame_fences[GPU_FENCE_RING_SIZE] = { NULL };
int s_fence_write_idx = 0;

SDL_GPUTransferBuffer* s_compute_staging_buffer = NULL;
u8* s_compute_staging_ptr = NULL;
size_t s_compute_staging_offset = 0;
int s_compute_drops_last_frame = 0;

float* mapped_vertex_ptr = NULL;
unsigned int vertex_count = 0;

SDL_GPUTexture* s_swapchain_texture = NULL;
SDL_GPUTexture* s_canvas_texture = NULL;

SDL_GPUTexture* texture_array = NULL;
int tex_array_free[TEX_ARRAY_MAX_LAYERS];
int tex_array_free_count = 0;
int16_t tex_array_layer[FL_TEXTURE_MAX];

int texture_layers[MAX_VERTICES];
float texture_uv_sx[MAX_VERTICES];
float texture_uv_sy[MAX_VERTICES];
float texture_palette_idx[MAX_VERTICES];
int texture_count = 0;

bool s_palette_uploaded[FL_PALETTE_MAX];
int s_pal_upload_dirty_indices[FL_PALETTE_MAX];
int s_pal_upload_dirty_count = 0;

unsigned int s_last_set_texture_handle = 0;

SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };

bool texture_dirty_flags[FL_TEXTURE_MAX] = { false };
bool palette_dirty_flags[FL_PALETTE_MAX] = { false };
int dirty_texture_indices[FL_TEXTURE_MAX];
int dirty_texture_count = 0;
int dirty_palette_indices[FL_PALETTE_MAX];
int dirty_palette_count = 0;

uint32_t palette_hash[FL_PALETTE_MAX] = { 0 };
uint32_t texture_hash[FL_TEXTURE_MAX] = { 0 };

TextureUploadJob s_tex_upload_jobs[MAX_COMPUTE_JOBS];
int s_tex_upload_count = 0;

PaletteUploadJob s_pal_upload_jobs[MAX_COMPUTE_JOBS];
int s_pal_upload_count = 0;

QuadSortKey quad_sort_keys[MAX_QUADS];
unsigned int quad_count = 0;
QuadSortKey quad_sort_temp[MAX_QUADS];

/** @brief Begin a new frame: acquire command buffer and swapchain texture. */
void SDLGameRendererGPU_BeginFrame(void) {
    TRACE_ZONE_N("GPU:BeginFrame");
    if (!device) {
        TRACE_ZONE_END();
        return;
    }

    // ⚡ Opt9: Wait on the oldest in-flight fence before reusing its resources.
    // With ring size == 3: we wait on frame N-3's fence, which is ≥2 frames old.
    // In practice this never actually blocks because the GPU finishes in <16ms.
    {
        int wait_idx = s_fence_write_idx; // oldest slot (write_idx has wrapped past it)
        if (s_frame_fences[wait_idx]) {
            SDL_WaitForGPUFences(device, true, &s_frame_fences[wait_idx], 1);
            SDL_ReleaseGPUFence(device, s_frame_fences[wait_idx]);
            s_frame_fences[wait_idx] = NULL;
        }
    }

    current_cmd_buf = SDL_AcquireGPUCommandBuffer(device);
    s_swapchain_texture = NULL; // Acquired lazily via GetSwapchainTexture()

    // Drain dirty-index lists
    for (int d = 0; d < dirty_texture_count; d++) {
        const int i = dirty_texture_indices[d];
        // 1D: free the single layer for this texture
        if (tex_array_layer[i] >= 0) {
            tex_array_free[tex_array_free_count++] = tex_array_layer[i];
            tex_array_layer[i] = -1;
        }
        if (surfaces[i]) {
            SDL_DestroySurface(surfaces[i]);
            surfaces[i] = NULL;
        }
        SDLGameRendererGPU_CreateTexture(i + 1);
        texture_dirty_flags[i] = false;
    }
    dirty_texture_count = 0;

    for (int d = 0; d < dirty_palette_count; d++) {
        const int i = dirty_palette_indices[d];
        // Palette changes don't invalidate indexed texture array layers —
        // the palette row is looked up separately by the fragment shader.
        if (palettes[i]) {
            SDL_DestroyPalette(palettes[i]);
            palettes[i] = NULL;
        }
        SDLGameRendererGPU_CreatePalette((i + 1) << 16);
        s_palette_uploaded[i] = false; // Mark for re-upload to palette atlas

        bool already_queued = false;
        for (int d = 0; d < s_pal_upload_dirty_count; d++) {
            if (s_pal_upload_dirty_indices[d] == i) {
                already_queued = true;
                break;
            }
        }
        if (!already_queued && s_pal_upload_dirty_count < FL_PALETTE_MAX) {
            s_pal_upload_dirty_indices[s_pal_upload_dirty_count++] = i; // ⚡ Opt9b
        }

        palette_dirty_flags[i] = false;
    }
    dirty_palette_count = 0;

    current_transfer_idx = (current_transfer_idx + 1) % VERTEX_TRANSFER_BUFFER_COUNT;
    // ⚡ Opt10a: cycle=false — buffer is already triple-buffered behind the fence ring,
    // so the driver doesn't need to orphan/rename it. Avoids implicit allocation overhead.
    mapped_vertex_ptr = (float*)SDL_MapGPUTransferBuffer(device, transfer_buffers[current_transfer_idx], false);

    // ⚡ Opt10b: Deferred staging map — the 32MB compute staging buffer is now mapped
    // lazily in SetTexture/RenderFrame only when a texture or palette upload is needed.
    // This eliminates a ~200μs driver stall on frames with no cache misses.
    s_compute_staging_ptr = NULL;
    s_compute_staging_offset = 0;

    vertex_count = 0;
    quad_count = 0;
    texture_count = 0;
    s_tex_upload_count = 0;
    s_pal_upload_count = 0;
    s_last_set_texture_handle = 0; // ⚡ Reset back-to-back cache each frame

    LZ77_BeginFrame(s_lz77_ctx);

    if (s_compute_drops_last_frame > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                    "Compute staging overflow: dropped %d texture(s) last frame",
                    s_compute_drops_last_frame);
    }
    s_compute_drops_last_frame = 0;

    TRACE_ZONE_END();
}

/** @brief Bottom-up merge sort for QuadSortKeys — O(n log n) stable sort matching GL backend. */
static void stable_sort_quads(void) {
    const unsigned int n = quad_count;
    if (n <= 1)
        return;

    QuadSortKey* src = quad_sort_keys;
    QuadSortKey* tmp = quad_sort_temp;

    for (unsigned int width = 1; width < n; width *= 2) {
        for (unsigned int i = 0; i < n; i += 2 * width) {
            unsigned int left = i;
            unsigned int mid = (i + width < n) ? i + width : n;
            unsigned int right = (i + 2 * width < n) ? i + 2 * width : n;
            unsigned int l = left, r = mid, k = left;

            while (l < mid && r < right) {
                if (src[l].z <= src[r].z)
                    tmp[k++] = src[l++];
                else
                    tmp[k++] = src[r++];
            }
            while (l < mid)
                tmp[k++] = src[l++];
            while (r < right)
                tmp[k++] = src[r++];
        }
        // Swap src/tmp
        QuadSortKey* swap = src;
        src = tmp;
        tmp = swap;
    }

    if (src != quad_sort_keys) {
        memcpy(quad_sort_keys, src, n * sizeof(QuadSortKey));
    }
}

/** @brief Flush buffered vertices to the GPU and execute the render pass. */
void SDLGameRendererGPU_RenderFrame(void) {
    TRACE_ZONE_N("GPU:RenderFrame");

    if (!current_cmd_buf || !gpu_window) {
        TRACE_ZONE_END();
        return;
    }

    // Z-depth sort
    Uint16* sorted_indices = NULL;
    unsigned int index_count = 0;
    if (quad_count > 0) {
        if (quad_count > 1) {
            stable_sort_quads();
        }
        sorted_indices = (Uint16*)SDL_MapGPUTransferBuffer(device, index_transfer_buffer, true);
        if (sorted_indices) {
            for (unsigned int i = 0; i < quad_count; i++) {
                const int vert_offset = quad_sort_keys[i].original_index * 4;
                const int idx_offset = i * 6;
                sorted_indices[idx_offset + 0] = vert_offset + 0;
                sorted_indices[idx_offset + 1] = vert_offset + 1;
                sorted_indices[idx_offset + 2] = vert_offset + 2;
                sorted_indices[idx_offset + 3] = vert_offset + 2;
                sorted_indices[idx_offset + 4] = vert_offset + 1;
                sorted_indices[idx_offset + 5] = vert_offset + 3;
            }
            index_count = quad_count * 6;
            SDL_UnmapGPUTransferBuffer(device, index_transfer_buffer);
        }
    }

    SDL_UnmapGPUTransferBuffer(device, transfer_buffers[current_transfer_idx]);
    mapped_vertex_ptr = NULL;

    // Stage dirty palette data into the main staging buffer BEFORE unmapping.
    // ⚡ Opt10b: Lazily map the staging buffer only when dirty palettes exist.
    if (s_compute_staging_ptr || s_pal_upload_dirty_count > 0) {
        if (!s_compute_staging_ptr) {
            s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
            s_compute_staging_offset = 0;
        }

        int processed_count = 0;
        while (processed_count < s_pal_upload_dirty_count) {
            const int i = s_pal_upload_dirty_indices[processed_count];
            if (i < 0 || i >= FL_PALETTE_MAX || s_palette_uploaded[i] || !palettes[i]) {
                processed_count++;
                continue;
            }

            size_t pal_size = (size_t)PALETTE_TEX_WIDTH * 4; // 256 colors × 4 bytes = 1024
            if (s_pal_upload_count >= MAX_COMPUTE_JOBS || s_compute_staging_offset + pal_size > COMPUTE_STORAGE_SIZE)
                break;

            u32* dst = (u32*)(s_compute_staging_ptr + s_compute_staging_offset);
            SDL_Palette* pal = palettes[i];
            // Write palette colors
            int c;
            for (c = 0; c < pal->ncolors && c < PALETTE_TEX_WIDTH; c++) {
                SDL_Color col = pal->colors[c];
                dst[c] = (col.a << 24) | (col.b << 16) | (col.g << 8) | col.r;
            }
            // Zero remaining entries (important for 16-color palettes)
            for (; c < PALETTE_TEX_WIDTH; c++) {
                dst[c] = 0;
            }

            PaletteUploadJob* job = &s_pal_upload_jobs[s_pal_upload_count++];
            job->row = i;
            job->offset = (Uint32)s_compute_staging_offset;
            s_compute_staging_offset += pal_size;
            // ⚡ Vulkan/SDL_GPU requires copy offsets to be highly aligned (typically 256/512 bytes)
            s_compute_staging_offset = (s_compute_staging_offset + 511) & ~511;
            s_palette_uploaded[i] = true;
            processed_count++;
        }

        // Shift remaining items to the front if we broke early
        if (processed_count < s_pal_upload_dirty_count) {
            int remaining = s_pal_upload_dirty_count - processed_count;
            memmove(s_pal_upload_dirty_indices, &s_pal_upload_dirty_indices[processed_count], remaining * sizeof(int));
            s_pal_upload_dirty_count = remaining;
        } else {
            s_pal_upload_dirty_count = 0;
        }
    }

    // Unmap staging buffer (only if it was mapped this frame)
    if (s_compute_staging_ptr) {
        SDL_UnmapGPUTransferBuffer(device, s_compute_staging_buffer);
        s_compute_staging_ptr = NULL;
    }

    // --- 1. Copy Pass (Textures + Palettes + Buffers) ---
    {
        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(current_cmd_buf);

        // Upload RGBA8 textures to texture array layers
        for (int i = 0; i < s_tex_upload_count; i++) {
            TextureUploadJob* job = &s_tex_upload_jobs[i];
            SDL_GPUTextureTransferInfo src = {
                .transfer_buffer = s_compute_staging_buffer,
                .offset = job->offset,
                .pixels_per_row = job->width,
                .rows_per_layer = job->height,
            };
            SDL_GPUTextureRegion dst = {
                .texture = texture_array,
                .layer = job->layer,
                .w = job->width,
                .h = job->height,
                .d = 1,
            };
            SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
        }

        // Upload dirty palettes to the palette atlas (from staging buffer offsets)
        for (int i = 0; i < s_pal_upload_count; i++) {
            PaletteUploadJob* job = &s_pal_upload_jobs[i];
            SDL_GPUTextureTransferInfo pal_src = {
                .transfer_buffer = s_compute_staging_buffer,
                .offset = job->offset,
                .pixels_per_row = PALETTE_TEX_WIDTH,
                .rows_per_layer = 1,
            };
            SDL_GPUTextureRegion pal_dst = {
                .texture = s_palette_texture,
                .y = job->row,
                .w = PALETTE_TEX_WIDTH,
                .h = 1,
                .d = 1,
            };
            SDL_UploadToGPUTexture(copy_pass, &pal_src, &pal_dst, false);
        }

        // Upload Vertex Data
        if (vertex_count > 0) {
            SDL_GPUTransferBufferLocation loc = { .transfer_buffer = transfer_buffers[current_transfer_idx],
                                                  .offset = 0 };
            SDL_GPUBufferRegion region = { .buffer = vertex_buffer,
                                           .offset = 0,
                                           .size = vertex_count * sizeof(GPUVertex) };
            SDL_UploadToGPUBuffer(copy_pass, &loc, &region, true);
        }

        // Upload Index Data
        if (index_count > 0) {
            SDL_GPUTransferBufferLocation loc = { .transfer_buffer = index_transfer_buffer, .offset = 0 };
            SDL_GPUBufferRegion region = { .buffer = index_buffer, .offset = 0, .size = index_count * sizeof(Uint16) };
            SDL_UploadToGPUBuffer(copy_pass, &loc, &region, true);
        }

        LZ77_Upload(s_lz77_ctx, device, copy_pass);

        SDL_EndGPUCopyPass(copy_pass);
    }

    // --- 1.5. Compute Pass (⚡ Opt6: LZ77 decode) ---
    LZ77_Dispatch(s_lz77_ctx, current_cmd_buf, texture_array, tex_array_layer);

    // --- 2. Render Pass ---
    if (s_canvas_texture) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture = s_canvas_texture;
        color_target.clear_color.r = ((flPs2State.FrameClearColor >> 16) & 0xFF) / 255.0f;
        color_target.clear_color.g = ((flPs2State.FrameClearColor >> 8) & 0xFF) / 255.0f;
        color_target.clear_color.b = (flPs2State.FrameClearColor & 0xFF) / 255.0f;
        color_target.clear_color.a = ModdedStage_IsActiveForCurrentStage() ? 0.0f : 1.0f;
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;
        color_target.cycle = true;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(current_cmd_buf, &color_target, 1, NULL);
        if (pass) {
            if (pipeline && vertex_count > 0) {
                // Fixed viewport for Canvas
                SDL_GPUViewport viewport;
                SDL_zero(viewport);
                viewport.x = 0;
                viewport.y = 0;
                viewport.w = 384;
                viewport.h = 224;
                viewport.min_depth = 0.0f;
                viewport.max_depth = 1.0f;
                SDL_SetGPUViewport(pass, &viewport);

                SDL_Rect scissor = { 0, 0, 384, 224 };
                SDL_SetGPUScissor(pass, &scissor);

                float matrix[4][4] = { { 2.0f / 384.0f, 0.0f, 0.0f, 0.0f },
                                       { 0.0f, -2.0f / 224.0f, 0.0f, 0.0f },
                                       { 0.0f, 0.0f, -1.0f, 0.0f },
                                       { -1.0f, 1.0f, 0.0f, 1.0f } };
                SDL_BindGPUGraphicsPipeline(pass, pipeline);

                SDL_PushGPUVertexUniformData(current_cmd_buf, 0, matrix, sizeof(matrix));

                SDL_GPUBufferBinding vb_binding;
                vb_binding.buffer = vertex_buffer;
                vb_binding.offset = 0;
                SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

                SDL_GPUBufferBinding ib_binding;
                ib_binding.buffer = index_buffer;
                ib_binding.offset = 0;
                SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                // Bind 2 fragment samplers: indexed texture array + palette atlas
                SDL_GPUTextureSamplerBinding tex_bindings[2];
                tex_bindings[0].texture = texture_array;
                tex_bindings[0].sampler = sampler;
                tex_bindings[1].texture = s_palette_texture;
                tex_bindings[1].sampler = palette_sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 2);

                SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, 0, 0, 0);
            }
            SDL_EndGPURenderPass(pass);
        }
    }

    TRACE_ZONE_END();
}

/** @brief End the frame: submit the command buffer. */
void SDLGameRendererGPU_EndFrame(void) {
    TRACE_ZONE_N("GPU:EndFrame");

    if (current_cmd_buf) {
        // ⚡ Opt9: Non-blocking submit — acquire fence for deferred wait in BeginFrame
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(current_cmd_buf);
        // Store in ring, releasing any old fence still in this slot
        if (s_frame_fences[s_fence_write_idx]) {
            SDL_ReleaseGPUFence(device, s_frame_fences[s_fence_write_idx]);
        }
        s_frame_fences[s_fence_write_idx] = fence;
        s_fence_write_idx = (s_fence_write_idx + 1) % GPU_FENCE_RING_SIZE;
        current_cmd_buf = NULL;
    }
    s_swapchain_texture = NULL;
    TRACE_ZONE_END();
}

SDL_GPUCommandBuffer* SDLGameRendererGPU_GetCommandBuffer(void) {
    return current_cmd_buf;
}

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (!mapped_vertex_ptr || vertex_count + 4 > MAX_VERTICES)
        return;

    float layer = -1.0f; // Bug 2 fix: negative sentinel → shader uses FgColor for solid quads
    float uv_sx = 1.0f, uv_sy = 1.0f;
    float palIdx = -1.0f;

    if (textured && texture_count > 0) {
        layer = (float)texture_layers[texture_count - 1];
        uv_sx = texture_uv_sx[texture_count - 1];
        uv_sy = texture_uv_sy[texture_count - 1];
        palIdx = texture_palette_idx[texture_count - 1];
    }

    GPUVertex* v = (GPUVertex*)(mapped_vertex_ptr) + vertex_count;

    Uint32 c = vertices[0].color;
    float b = (c & 0xFF) / 255.0f;
    float g = ((c >> 8) & 0xFF) / 255.0f;
    float r = ((c >> 16) & 0xFF) / 255.0f;
    float a = ((c >> 24) & 0xFF) / 255.0f;

    for (int i = 0; i < 4; i++) {
        v[i].x = vertices[i].coord.x;
        v[i].y = vertices[i].coord.y;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
        v[i].a = a;
        v[i].u = vertices[i].tex_coord.s * uv_sx;
        v[i].v = vertices[i].tex_coord.t * uv_sy;
        v[i].layer = layer;
        v[i].paletteIdx = palIdx;
    }

    if (quad_count < MAX_QUADS) {
        quad_sort_keys[quad_count].z = flPS2ConvScreenFZ(vertices[0].coord.z);
        quad_sort_keys[quad_count].original_index = quad_count;
        quad_count++;
    }

    vertex_count += 4;
}

/** @brief Submit a textured quad to the batch. */
void SDLGameRendererGPU_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    for (int i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].color = color;
        vertices[i].tex_coord = sprite->t[i];
    }
    draw_quad(vertices, true);
}

/** @brief Submit a solid-color quad to the batch. */
void SDLGameRendererGPU_DrawSolidQuad(const Quad* q, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    for (int i = 0; i < 4; i++) {
        vertices[i].coord.x = q->v[i].x;
        vertices[i].coord.y = q->v[i].y;
        vertices[i].coord.z = q->v[i].z;
        vertices[i].color = color;
        vertices[i].tex_coord.s = 0;
        vertices[i].tex_coord.t = 0;
    }
    draw_quad(vertices, false);
}

/** @brief Submit a sprite (legacy format) to the batch. */
void SDLGameRendererGPU_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];

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

/** @brief Submit a Sprite2 to the batch. */
void SDLGameRendererGPU_DrawSprite2(const Sprite2* sprite2) {
    SDLGameRenderer_Vertex vertices[4];

    const float z = sprite2->v[0].z;
    const unsigned int color = sprite2->vertex_color;

    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = z;
        vertices[i].color = color;
    }

    vertices[0].coord.x = sprite2->v[0].x;
    vertices[0].coord.y = sprite2->v[0].y;
    vertices[3].coord.x = sprite2->v[1].x;
    vertices[3].coord.y = sprite2->v[1].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite2->t[0];
    vertices[3].tex_coord = sprite2->t[1];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}
/**
 * @brief ⚡ Batch sprite flush for GPU backend (Opt 2+4).
 *
 * Inlines SetTexture + draw_quad to avoid per-sprite function call overhead,
 * and pre-computes color floats once per sprite.
 * Preserves original submission order — no tex_code sorting, because sprites
 * with the same Z value rely on draw order for correct layering.
 */
void SDLGameRendererGPU_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    if (!mapped_vertex_ptr || count <= 0)
        return;

    unsigned int last_tex_code = 0;

    for (int i = 0; i < count; i++) {
        if (!active_layers[chips[i].id])
            continue;

        if (vertex_count + 4 > MAX_VERTICES)
            break;

        const Sprite2* spr = &chips[i];

        // Inlined SetTexture — only call when tex_code changes
        unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            SDLGameRendererGPU_SetTexture(tc);
        }

        // --- Inlined draw_quad with pre-computed color ---
        float layer = 0.0f;
        float uv_sx = 1.0f, uv_sy = 1.0f;

        if (texture_count > 0) {
            layer = (float)texture_layers[texture_count - 1];
            uv_sx = texture_uv_sx[texture_count - 1];
            uv_sy = texture_uv_sy[texture_count - 1];
        }

        GPUVertex* v = (GPUVertex*)(mapped_vertex_ptr) + vertex_count;

        // ⚡ Bolt: SIMD color unpack — extract 4 u8 channels to floats in one shot
        const Uint32 c = spr->vertex_color;
        const simde__m128i ci = simde_mm_set_epi32((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        const simde__m128 cf = simde_mm_mul_ps(simde_mm_cvtepi32_ps(ci), simde_mm_set1_ps(1.0f / 255.0f));
        float color_f[4];
        simde_mm_storeu_ps(color_f, cf);
        const float cb = color_f[0], cg = color_f[1], cr = color_f[2], ca = color_f[3];

        // Expand Sprite2 (2 corners) to 4-vertex quad
        const float x0 = spr->v[0].x;
        const float y0 = spr->v[0].y;
        const float x1 = spr->v[1].x;
        const float y1 = spr->v[1].y;
        const float s0 = spr->t[0].s * uv_sx;
        const float t0 = spr->t[0].t * uv_sy;
        const float s1 = spr->t[1].s * uv_sx;
        const float t1 = spr->t[1].t * uv_sy;

        float palIdx = (texture_count > 0) ? texture_palette_idx[texture_count - 1] : -1.0f;
        v[0].x = x0;
        v[0].y = y0;
        v[0].r = cr;
        v[0].g = cg;
        v[0].b = cb;
        v[0].a = ca;
        v[0].u = s0;
        v[0].v = t0;
        v[0].layer = layer;
        v[0].paletteIdx = palIdx;
        v[1].x = x1;
        v[1].y = y0;
        v[1].r = cr;
        v[1].g = cg;
        v[1].b = cb;
        v[1].a = ca;
        v[1].u = s1;
        v[1].v = t0;
        v[1].layer = layer;
        v[1].paletteIdx = palIdx;
        v[2].x = x0;
        v[2].y = y1;
        v[2].r = cr;
        v[2].g = cg;
        v[2].b = cb;
        v[2].a = ca;
        v[2].u = s0;
        v[2].v = t1;
        v[2].layer = layer;
        v[2].paletteIdx = palIdx;
        v[3].x = x1;
        v[3].y = y1;
        v[3].r = cr;
        v[3].g = cg;
        v[3].b = cb;
        v[3].a = ca;
        v[3].u = s1;
        v[3].v = t1;
        v[3].layer = layer;
        v[3].paletteIdx = palIdx;

        if (quad_count < MAX_QUADS) {
            quad_sort_keys[quad_count].z = flPS2ConvScreenFZ(spr->v[0].z);
            quad_sort_keys[quad_count].original_index = quad_count;
            quad_count++;
        }

        vertex_count += 4;
    }
}

unsigned int SDLGameRendererGPU_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    return 0; // Not applicable
}

SDL_GPUTexture* SDLGameRendererGPU_GetSwapchainTexture(void) {
    if (!s_swapchain_texture && current_cmd_buf && gpu_window) {
        if (!SDL_AcquireGPUSwapchainTexture(current_cmd_buf, gpu_window, &s_swapchain_texture, NULL, NULL)) {
            s_swapchain_texture = NULL;
        }
    }
    return s_swapchain_texture;
}

SDL_GPUTexture* SDLGameRendererGPU_GetCanvasTexture(void) {
    return s_canvas_texture;
}

// ⚡ Opt6: LZ77 GPU compute API
int SDLGameRendererGPU_LZ77Available(void) {
    return LZ77_IsAvailable(s_lz77_ctx);
}

int SDLGameRendererGPU_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size, int texture_handle,
                                   int palette_handle, u32 code, u32 tile_dim) {
    if (!LZ77_IsAvailable(s_lz77_ctx))
        return 0;

    int ti = texture_handle - 1;
    if (ti < 0 || ti >= FL_TEXTURE_MAX)
        return 0;
    if (palette_handle < 0 || palette_handle > FL_PALETTE_MAX)
        return 0;

    int layer = tex_array_layer[ti]; // 1D: keyed by texture only
    if (layer < 0) {
        if (tex_array_free_count <= 0)
            return 0;
        layer = tex_array_free[--tex_array_free_count];
        tex_array_layer[ti] = layer; // 1D
    }

    return LZ77_Enqueue(s_lz77_ctx, device, compressed, comp_size, decomp_size, ti, layer, code, tile_dim);
}
