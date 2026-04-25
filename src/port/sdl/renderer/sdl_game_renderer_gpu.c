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
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/sdl/renderer/sdl_gpu_metadata.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>
#include "port/render_pass.h"
#include "port/render_job.h"

#include "radix_sort.h"
/* ─── Global Variable Definitions ─────────────────────────────────────── */
/* Declared extern in sdl_game_renderer_gpu_internal.h                     */

SDL_GPUDevice* device = NULL;
LZ77Context* s_lz77_ctx = NULL;
SDL_Window* gpu_window = NULL;
SDL_GPUCommandBuffer* current_cmd_buf = NULL;
SDL_GPUGraphicsPipeline* pipelines[3] = { NULL };
RendererBlendMode s_current_blend_mode = RENDERER_BLEND_NORMAL;

SDL_GPUTexture* s_palette_texture = NULL;
SDL_GPUTransferBuffer* s_palette_transfer = NULL;
SDL_GPUSampler* palette_sampler = NULL;
SDL_GPUSampler* sampler = NULL;

SDL_GPUBuffer* vertex_buffer = NULL;
SDL_GPUBuffer* index_buffer = NULL;
SDL_GPUTransferBuffer* transfer_buffers[VERTEX_TRANSFER_BUFFER_COUNT] = { NULL };
SDL_GPUTransferBuffer* index_transfer_buffer = NULL;
int current_transfer_idx = 0;


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

PassRecordingState pass_state[8];
static int s_sort_inversions = 0;
static float s_last_submitted_z = -1e30f;
SDL_GPUTexture* s_1x1_white_texture = NULL; /* fallback for overlay sampler */

/** @brief Begin a new frame: acquire command buffer and swapchain texture. */
void SDLGameRendererGPU_SetBlendMode(RendererBlendMode mode) {
    s_current_blend_mode = mode;
}

void SDLGameRendererGPU_BeginFrame(void) {
    TRACE_ZONE_N("GPU:BeginFrame");
    if (!device) {
        TRACE_ZONE_END();
        return;
    }

    // ⚡ Opt11: No per-frame fence — rely on SDL's built-in resource cycling.
    // Transfer buffers are mapped with cycle=true, so SDL internally manages
    // the backing memory and avoids CPU–GPU hazards without explicit fences.

    {
        Uint64 t0 = SDL_GetPerformanceCounter();
        current_cmd_buf = SDL_AcquireGPUCommandBuffer(device);
        Uint64 dt_us = (SDL_GetPerformanceCounter() - t0) * 1000000 / SDL_GetPerformanceFrequency();
        if (dt_us > 200) {
            TRACE_MSG_COLOR("GPU:AcquireCmdBuf SPIKE", 0xFF4400);
        }
    }
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
    // ⚡ Opt11: cycle=true — let SDL internally manage the backing memory.
    // Combined with fire-and-forget submit (no fence), this eliminates ~108µs/frame
    // of explicit CPU-GPU synchronization overhead.
    {
        Uint64 t0 = SDL_GetPerformanceCounter();
        mapped_vertex_ptr = (float*)SDL_MapGPUTransferBuffer(device, transfer_buffers[current_transfer_idx], true);
        Uint64 dt_us = (SDL_GetPerformanceCounter() - t0) * 1000000 / SDL_GetPerformanceFrequency();
        if (dt_us > 200) {
            TRACE_MSG_COLOR("GPU:MapTransfer SPIKE", 0xFF8800);
        }
    }

    // ⚡ Opt10b: Deferred staging map — the 32MB compute staging buffer is now mapped
    // lazily in SetTexture/RenderFrame only when a texture or palette upload is needed.
    // This eliminates a ~200μs driver stall on frames with no cache misses.
    s_compute_staging_ptr = NULL;
    s_compute_staging_offset = 0;

    vertex_count = 0;
    for (int p = 0; p < 8; p++) { memset(&pass_state[p], 0, sizeof(PassRecordingState)); pass_state[p].last_submitted_z = -1e30f; }
    s_sort_inversions = 0;
    s_last_submitted_z = -1e30f;
    texture_count = 0;
    s_tex_upload_count = 0;
    s_pal_upload_count = 0;
    s_last_set_texture_handle = 0; // ⭐ Reset back-to-back cache each frame

    extern void SDLGameRendererGPU_FreeOverlayLayers(void);
    SDLGameRendererGPU_FreeOverlayLayers(); // Free temp overlay array layers from last frame

    LZ77_BeginFrame(s_lz77_ctx);

    if (s_compute_drops_last_frame > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                    "Compute staging overflow: dropped %d texture(s) last frame",
                    s_compute_drops_last_frame);
    }
    s_compute_drops_last_frame = 0;

    TRACE_ZONE_END();
}

// ⚡ Radix sort scratch buffers — per-pass for thread safety (Phase 4)
#define NUM_SORT_PASSES 3
static uint32_t radix_keys[NUM_SORT_PASSES][MAX_QUADS];
static int radix_scratch[NUM_SORT_PASSES][MAX_QUADS];
static int quad_order[NUM_SORT_PASSES][MAX_QUADS];
static float quad_z_values[NUM_SORT_PASSES][MAX_QUADS];
static QuadSortKey quad_sort_temp[NUM_SORT_PASSES][MAX_QUADS];

#define INSERTION_SORT_THRESHOLD 16

static void insertion_sort_quads(int p) {
    for (unsigned int i = 1; i < pass_state[p].quad_count; i++) {
        const QuadSortKey key = pass_state[p].sort_keys[i];
        int j = i - 1;
        while (j >= 0) {
            if (pass_state[p].sort_keys[j].z <= key.z)
                break;
            pass_state[p].sort_keys[j + 1] = pass_state[p].sort_keys[j];
            j--;
        }
        pass_state[p].sort_keys[j + 1] = key;
    }
}

// turbo
// What: Convert O(N log N) quad merge sort into an O(N) adaptive radix/insertion sort.
// Target: CPU Algorithmic Complexity / Branch Prediction.
// Expected Impact: Eliminates the O(N log N) merge sort overhead and associated branch mispredictions. Gameplay
// typically features near-sorted z-depths where insertion sort achieves O(N).
static void stable_sort_quads(int p) {
    if (pass_state[p].quad_count <= 1)
        return;

    if (s_sort_inversions <= INSERTION_SORT_THRESHOLD) {
        insertion_sort_quads(p);
    } else {
        for (unsigned int i = 0; i < pass_state[p].quad_count; i++) {
            quad_z_values[p][i] = pass_state[p].sort_keys[i].z;
            quad_order[p][i] = i;
        }

        radix_sort_render_task_indices(quad_order[p], quad_z_values[p], pass_state[p].quad_count, radix_keys[p], radix_scratch[p]);

        for (unsigned int i = 0; i < pass_state[p].quad_count; i++) {
            quad_sort_temp[p][i] = pass_state[p].sort_keys[quad_order[p][i]];
        }
        memcpy(pass_state[p].sort_keys, quad_sort_temp[p], pass_state[p].quad_count * sizeof(QuadSortKey));
    }
}

/** @brief RenderJob callback: sort a single pass's quads. */
static void sort_pass_job(int pass_index, void* userdata) {
    (void)userdata;
    stable_sort_quads(pass_index);
}

/** @brief Flush buffered vertices to the GPU and execute the render pass. */
void SDLGameRendererGPU_RenderFrame(void) {
    TRACE_ZONE_N("GPU:RenderFrame");

    if (!current_cmd_buf || !gpu_window) {
        TRACE_ZONE_END();
        return;
    }

    // Step 1: RenderGraph compilation
    for (int p = 0; p < 3; p++) {
        g_render_passes.passes[p].has_geometry = (pass_state[p].quad_count > 0);
    }
    RenderGraph_Compile();

    // Z-depth sort (per-pass, parallel via job queue)
    Uint16* sorted_indices = NULL;
    unsigned int index_count = 0;
    unsigned int total_quads = pass_state[0].quad_count + pass_state[1].quad_count + pass_state[2].quad_count;
    if (total_quads > 0) {
        // Phase 4: dispatch per-pass sorts to worker threads
        RenderJob sort_jobs[3];
        int sort_job_count = 0;
        for (int p = 0; p < 3; p++) {
            if (pass_state[p].quad_count > 1) {
                sort_jobs[sort_job_count++] = (RenderJob){ .pass_index = p, .fn = sort_pass_job, .userdata = NULL };
            }
        }
        if (sort_job_count > 0) {
            RenderJobQueue_Submit(sort_jobs, sort_job_count);
            RenderJobQueue_WaitAll();
        }
        sorted_indices = (Uint16*)SDL_MapGPUTransferBuffer(device, index_transfer_buffer, true);
        if (sorted_indices) {
            for (int p = 0; p < 3; p++) {
                for (unsigned int i = 0; i < pass_state[p].quad_count; i++) {
                    const int vert_offset = pass_state[p].sort_keys[i].global_quad_index * 4;
                    const int idx_offset = index_count * 6 + i * 6;
                    sorted_indices[idx_offset + 0] = vert_offset + 0;
                    sorted_indices[idx_offset + 1] = vert_offset + 1;
                    sorted_indices[idx_offset + 2] = vert_offset + 2;
                    sorted_indices[idx_offset + 3] = vert_offset + 2;
                    sorted_indices[idx_offset + 4] = vert_offset + 1;
                    sorted_indices[idx_offset + 5] = vert_offset + 3;
                }
                index_count += pass_state[p].quad_count;
            }
            index_count *= 6;
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
    if (s_canvas_texture && !g_render_passes.passes[0].skip_this_frame) {
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
            if (pass_state[0].quad_count > 0) {
                // Fixed viewport for Canvas (scaled)
                const int sw = 384 * g_resolution_scale;
                const int sh = 224 * g_resolution_scale;
                SDL_GPUViewport viewport;
                SDL_zero(viewport);
                viewport.x = 0;
                viewport.y = 0;
                viewport.w = sw;
                viewport.h = sh;
                viewport.min_depth = 0.0f;
                viewport.max_depth = 1.0f;
                SDL_SetGPUViewport(pass, &viewport);

                SDL_Rect scissor = { 0, 0, sw, sh };
                SDL_SetGPUScissor(pass, &scissor);

                float matrix[4][4] = { { 2.0f / (float)sw, 0.0f, 0.0f, 0.0f },
                                       { 0.0f, -2.0f / (float)sh, 0.0f, 0.0f },
                                       { 0.0f, 0.0f, -1.0f, 0.0f },
                                       { -1.0f, 1.0f, 0.0f, 1.0f } };

                SDL_PushGPUVertexUniformData(current_cmd_buf, 0, matrix, sizeof(matrix));

                SDL_GPUBufferBinding vb_binding;
                vb_binding.buffer = vertex_buffer;
                vb_binding.offset = 0;
                SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

                SDL_GPUBufferBinding ib_binding;
                ib_binding.buffer = index_buffer;
                ib_binding.offset = 0;
                SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                // Bind 3 fragment samplers: texture array + palette atlas + overlay
                SDL_GPUTextureSamplerBinding tex_bindings[3];
                tex_bindings[0].texture = texture_array;
                tex_bindings[0].sampler = sampler;
                tex_bindings[1].texture = s_palette_texture;
                tex_bindings[1].sampler = palette_sampler;
                tex_bindings[2].texture = s_1x1_white_texture ? s_1x1_white_texture : texture_array;
                tex_bindings[2].sampler = sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 3);

                // Split-draw: batch consecutive quads with the same overlay texture AND blend mode.
                unsigned int draw_start = 0;
                SDL_GPUTexture* current_overlay = (SDL_GPUTexture*)-1;
                RendererBlendMode current_applied_blend = (RendererBlendMode)-1;

                for (unsigned int qi = 0; qi <= pass_state[0].quad_count; qi++) {
                    SDL_GPUTexture* this_overlay = NULL;
                    RendererBlendMode this_blend = RENDERER_BLEND_NORMAL;

                    if (qi < pass_state[0].quad_count) {
                        int orig_idx = pass_state[0].sort_keys[qi].original_index;
                        this_overlay = pass_state[0].overlay_tex[orig_idx];
                        this_blend = pass_state[0].sort_keys[qi].blend_mode;
                    }

                    bool blend_changed = (this_blend != current_applied_blend);
                    bool overlay_changed = (this_overlay != current_overlay);

                    if (qi == pass_state[0].quad_count || blend_changed || overlay_changed) {
                        // Flush previous segment
                        unsigned int segment_quads = qi - draw_start;
                        if (segment_quads > 0) {
                            // ⚡ Skip standalone overlays (e.g. HD backgrounds) in the native canvas FBO pass.
                            // They are deferred to SDLGameRendererGPU_RenderHDPass.
                            if (current_overlay == NULL) {
                                SDL_DrawGPUIndexedPrimitives(pass, segment_quads * 6, 1, draw_start * 6, 0, 0);
                            }
                        }

                        if (qi < pass_state[0].quad_count) {
                            if (blend_changed) {
                                current_applied_blend = this_blend;
                                SDL_BindGPUGraphicsPipeline(pass, pipelines[current_applied_blend]);
                            }

                            if (overlay_changed) {
                                current_overlay = this_overlay;
                                tex_bindings[2].texture = current_overlay ? current_overlay : (s_1x1_white_texture ? s_1x1_white_texture : texture_array);
                                tex_bindings[2].sampler = sampler;
                                SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 3);
                            }
                        }
                        draw_start = qi;
                    }
                }
            }
            SDL_EndGPURenderPass(pass);
        }
        TRACE_ZONE_END();
    }
}

void SDLGameRendererGPU_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h) {
    if (!s_swapchain_texture || !current_cmd_buf)
        return;

    int p = pass_index;
    if (p < 0 || p >= g_render_passes.count || g_render_passes.passes[p].skip_this_frame)
        return;

    TRACE_ZONE_N("GPU:ExecutePass");

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = s_swapchain_texture;
    color_target.load_op = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(current_cmd_buf, &color_target, 1, NULL);
    if (pass) {
        SDL_GPUViewport viewport;
        SDL_zero(viewport);
        viewport.x = viewport_x;
        viewport.y = viewport_y;
        viewport.w = viewport_w;
        viewport.h = viewport_h;
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        SDL_SetGPUViewport(pass, &viewport);

        SDL_Rect scissor = { viewport_x, viewport_y, viewport_w, viewport_h };
        SDL_SetGPUScissor(pass, &scissor);

        // Map the game's 384x224 * scale logical coordinate space to the full physical viewport.
        const int sw = 384 * g_resolution_scale;
        const int sh = 224 * g_resolution_scale;
        float matrix[4][4] = { { 2.0f / (float)sw, 0.0f, 0.0f, 0.0f },
                               { 0.0f, -2.0f / (float)sh, 0.0f, 0.0f },
                               { 0.0f, 0.0f, -1.0f, 0.0f },
                               { -1.0f, 1.0f, 0.0f, 1.0f } };

        SDL_PushGPUVertexUniformData(current_cmd_buf, 0, matrix, sizeof(matrix));

        SDL_GPUBufferBinding vb_binding;
        vb_binding.buffer = vertex_buffer;
        vb_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_GPUBufferBinding ib_binding;
        ib_binding.buffer = index_buffer;
        ib_binding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_GPUTextureSamplerBinding tex_bindings[3];
        tex_bindings[0].texture = texture_array;
        tex_bindings[0].sampler = sampler;
        tex_bindings[1].texture = s_palette_texture;
        tex_bindings[1].sampler = palette_sampler;
        tex_bindings[2].texture = texture_array;
        tex_bindings[2].sampler = sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 3);

        // Compute index buffer offset: pass 0's quads come first, then pass 1, then pass 2
        unsigned int pass_index_offset = pass_state[0].quad_count;
        if (p == 2) pass_index_offset += pass_state[1].quad_count;

        unsigned int draw_start = 0;
        SDL_GPUTexture* current_overlay = (SDL_GPUTexture*)-1;
        RendererBlendMode current_applied_blend = (RendererBlendMode)-1;

        for (unsigned int qi = 0; qi <= pass_state[p].quad_count; qi++) {
            SDL_GPUTexture* this_overlay = NULL;
            RendererBlendMode this_blend = RENDERER_BLEND_NORMAL;

            if (qi < pass_state[p].quad_count) {
                int orig_idx = pass_state[p].sort_keys[qi].original_index;
                this_overlay = pass_state[p].overlay_tex[orig_idx];
                this_blend = pass_state[p].sort_keys[qi].blend_mode;
            }

            bool blend_changed = (this_blend != current_applied_blend);
            bool overlay_changed = (this_overlay != current_overlay);

            if (qi == pass_state[p].quad_count || blend_changed || overlay_changed) {
                unsigned int segment_quads = qi - draw_start;
                if (segment_quads > 0 && current_overlay != NULL && current_overlay != (SDL_GPUTexture*)-1) {
                    SDL_DrawGPUIndexedPrimitives(pass, segment_quads * 6, 1, (pass_index_offset + draw_start) * 6, 0, 0);
                }

                if (qi < pass_state[p].quad_count) {
                    if (blend_changed) {
                        current_applied_blend = this_blend;
                        SDL_BindGPUGraphicsPipeline(pass, pipelines[current_applied_blend]);
                    }

                    if (overlay_changed) {
                        current_overlay = this_overlay;
                        tex_bindings[2].texture = current_overlay ? current_overlay : (s_1x1_white_texture ? s_1x1_white_texture : texture_array);
                        tex_bindings[2].sampler = sampler;
                        SDL_BindGPUFragmentSamplers(pass, 0, tex_bindings, 3);
                    }
                    draw_start = qi;
                }
            }
        }
        SDL_EndGPURenderPass(pass);
    }
    TRACE_ZONE_END();
}

/** @brief End the frame: submit the command buffer. */
void SDLGameRendererGPU_EndFrame(void) {
    TRACE_ZONE_N("GPU:EndFrame");

    if (current_cmd_buf) {
        // ⚡ Opt11: Fire-and-forget submit — no fence acquisition.
        // SDL internally tracks resource lifetimes via cycle=true on transfer buffers.
        Uint64 t0 = SDL_GetPerformanceCounter();
        SDL_SubmitGPUCommandBuffer(current_cmd_buf);
        Uint64 dt_us = (SDL_GetPerformanceCounter() - t0) * 1000000 / SDL_GetPerformanceFrequency();
        if (dt_us > 500) {
            TRACE_MSG_COLOR("GPU:Submit SPIKE", 0xFF0000);
        }
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
    const float scale = (float)g_resolution_scale;

    for (int i = 0; i < 4; i++) {
        v[i].x = vertices[i].coord.x * scale;
        v[i].y = vertices[i].coord.y * scale;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
        v[i].a = a;
        v[i].u = vertices[i].tex_coord.s * uv_sx;
        v[i].v = vertices[i].tex_coord.t * uv_sy;
        v[i].layer = layer;
        v[i].paletteIdx = palIdx;
    }

    int p = 0; // Canvas pass
    if (pass_state[p].quad_count < MAX_QUADS) {
        float z = flPS2ConvScreenFZ(vertices[0].coord.z);
        pass_state[p].sort_keys[pass_state[p].quad_count].z = z;
        pass_state[p].sort_keys[pass_state[p].quad_count].original_index = pass_state[p].quad_count;
        pass_state[p].sort_keys[pass_state[p].quad_count].global_quad_index = vertex_count / 4;
        pass_state[p].sort_keys[pass_state[p].quad_count].blend_mode = s_current_blend_mode;
        pass_state[p].overlay_tex[pass_state[p].quad_count] = NULL;

        if (z < s_last_submitted_z) {
            s_sort_inversions++;
        }
        s_last_submitted_z = z;

        pass_state[p].quad_count++;
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
        float layer = -1.0f; // Sentinel: shader uses FgColor when no texture bound
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
        const float scale = (float)g_resolution_scale;
        const float x0 = spr->v[0].x * scale;
        const float y0 = spr->v[0].y * scale;
        const float x1 = spr->v[1].x * scale;
        const float y1 = spr->v[1].y * scale;
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

        int p = 0; // Canvas pass
        if (pass_state[p].quad_count < MAX_QUADS) {
            float z = flPS2ConvScreenFZ(spr->v[0].z);
            pass_state[p].sort_keys[pass_state[p].quad_count].z = z;
            pass_state[p].sort_keys[pass_state[p].quad_count].original_index = pass_state[p].quad_count;
            pass_state[p].sort_keys[pass_state[p].quad_count].global_quad_index = vertex_count / 4;
            pass_state[p].sort_keys[pass_state[p].quad_count].blend_mode = s_current_blend_mode;
            pass_state[p].overlay_tex[pass_state[p].quad_count] = NULL;

            if (z < s_last_submitted_z) {
                s_sort_inversions++;
            }
            s_last_submitted_z = z;

            pass_state[p].quad_count++;
        }

        vertex_count += 4;
    }
}

unsigned int SDLGameRendererGPU_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    return 0; // Not applicable
}

SDL_GPUTexture* SDLGameRendererGPU_GetSwapchainTexture(void) {
    if (!s_swapchain_texture && current_cmd_buf && gpu_window) {
        Uint64 t0 = SDL_GetPerformanceCounter();
        if (!SDL_AcquireGPUSwapchainTexture(current_cmd_buf, gpu_window, &s_swapchain_texture, NULL, NULL)) {
            s_swapchain_texture = NULL;
        }
        Uint64 dt_us = (SDL_GetPerformanceCounter() - t0) * 1000000 / SDL_GetPerformanceFrequency();
        if (dt_us > 500) {
            TRACE_MSG_COLOR("GPU:Swapchain SPIKE", 0xFFFF00);
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

/* ─── Overlay Sprite Drawing ─────────────────────────────────────────── */

/**
 * @brief Track temporary texture array layers used for overlay sprites.
 *
 * Overlay textures (HD sprite overrides, portraits, etc.) need to participate
 * in the z-sorted batch.  We upload their cached CPU-side pixels into
 * temporary texture array layers via the compute staging buffer (same proven
 * path as SetTexture), then push quads with paletteIdx = -1 for direct RGBA.
 */
#define MAX_OVERLAY_LAYERS 16
static int s_overlay_layers[MAX_OVERLAY_LAYERS];
static int s_overlay_layer_count = 0;

/** @brief Free all temporary overlay layers (called from BeginFrame). */
void SDLGameRendererGPU_FreeOverlayLayers(void) {
    for (int i = 0; i < s_overlay_layer_count; i++) {
        if (s_overlay_layers[i] >= 0 && tex_array_free_count < TEX_ARRAY_MAX_LAYERS) {
            tex_array_free[tex_array_free_count++] = s_overlay_layers[i];
        }
    }
    s_overlay_layer_count = 0;
}

/**
 * @brief Upload cached RGBA pixels into a temporary texture array layer.
 *
 * Uses the compute staging buffer (same path as SetTexture) to copy
 * CPU-side pixel data into a free texture array layer.  Returns the
 * layer index, or -1 on failure.
 */
static int upload_overlay_to_array_layer(const uint32_t* pixels, int tex_w, int tex_h) {
    if (!device || !current_cmd_buf || !texture_array || !pixels)
        return -1;
    if (tex_array_free_count <= 0)
        return -1;
    if (s_overlay_layer_count >= MAX_OVERLAY_LAYERS)
        return -1;
    if (tex_w > TEX_ARRAY_SIZE || tex_h > TEX_ARRAY_SIZE)
        return -1;

    /* Lazily map the staging buffer on first overlay this frame */
    if (!s_compute_staging_ptr) {
        s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
        s_compute_staging_offset = 0;
    }
    if (!s_compute_staging_ptr)
        return -1;

    size_t rgba_size = (size_t)tex_w * tex_h * 4;
    if (s_tex_upload_count >= MAX_COMPUTE_JOBS || s_compute_staging_offset + rgba_size > COMPUTE_STORAGE_SIZE)
        return -1;

    int layer = tex_array_free[--tex_array_free_count];

    /* Copy pixel data into staging buffer */
    Uint32 out_offset = (Uint32)s_compute_staging_offset;
    memcpy(s_compute_staging_ptr + s_compute_staging_offset, pixels, rgba_size);
    s_compute_staging_offset += rgba_size;
    /* Vulkan alignment requirement */
    s_compute_staging_offset = (s_compute_staging_offset + 511) & ~511;

    /* Enqueue upload job (processed in RenderFrame copy pass) */
    TextureUploadJob* job = &s_tex_upload_jobs[s_tex_upload_count++];
    job->width = tex_w;
    job->height = tex_h;
    job->layer = layer;
    job->offset = out_offset;

    s_overlay_layers[s_overlay_layer_count++] = layer;
    return layer;
}

/**
 * @brief Push an overlay quad into the GPU batch.
 *
 * For array-layer overlays: standalone_tex = NULL, layer >= 0
 * For standalone overlays:  standalone_tex != NULL, layer = -2 (sentinel)
 */
static void push_overlay_quad(int layer, float x, float y, float w, float h, float z, int tex_w, int tex_h, int flip_x,
                              int flip_y, SDL_GPUTexture* standalone_tex) {
    int p = (z < 0.1f) ? 1 : 2;
    if (!mapped_vertex_ptr || vertex_count + 4 > MAX_VERTICES || pass_state[p].quad_count >= MAX_QUADS)
        return;

    float u0, u1, v0, v1;
    if (standalone_tex) {
        /* Standalone texture: UVs span [0,1] */
        u0 = flip_x ? 1.0f : 0.0f;
        u1 = flip_x ? 0.0f : 1.0f;
        v0 = flip_y ? 1.0f : 0.0f;
        v1 = flip_y ? 0.0f : 1.0f;
    } else {
        /* Array layer: UVs are scaled to sub-rect within the 512×512 layer */
        float u_scale = (float)tex_w / (float)TEX_ARRAY_SIZE;
        float v_scale = (float)tex_h / (float)TEX_ARRAY_SIZE;
        u0 = flip_x ? u_scale : 0.0f;
        u1 = flip_x ? 0.0f : u_scale;
        v0 = flip_y ? v_scale : 0.0f;
        v1 = flip_y ? 0.0f : v_scale;
    }

    GPUVertex* v = (GPUVertex*)mapped_vertex_ptr + vertex_count;
    float fl = (float)layer;
    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    /* Top-left */
    v[0] = (GPUVertex) { sx, sy, 1, 1, 1, 1, u0, v0, fl, -1.0f };
    /* Top-right */
    v[1] = (GPUVertex) { sx + sw, sy, 1, 1, 1, 1, u1, v0, fl, -1.0f };
    /* Bottom-left */
    v[2] = (GPUVertex) { sx, sy + sh, 1, 1, 1, 1, u0, v1, fl, -1.0f };
    /* Bottom-right */
    v[3] = (GPUVertex) { sx + sw, sy + sh, 1, 1, 1, 1, u1, v1, fl, -1.0f };

    pass_state[p].sort_keys[pass_state[p].quad_count].z = z;
    pass_state[p].sort_keys[pass_state[p].quad_count].original_index = pass_state[p].quad_count;
    pass_state[p].sort_keys[pass_state[p].quad_count].global_quad_index = vertex_count / 4;
    pass_state[p].sort_keys[pass_state[p].quad_count].blend_mode = s_current_blend_mode;
    pass_state[p].overlay_tex[pass_state[p].quad_count] = standalone_tex;

    if (z < s_last_submitted_z) {
        s_sort_inversions++;
    }
    s_last_submitted_z = z;

    pass_state[p].quad_count++;
    vertex_count += 4;
}

/** @brief Draw an overlay sprite using cached CPU-side pixel data. */
void SDLGameRendererGPU_DrawOverlaySprite(const uint32_t* pixels, int tex_w, int tex_h, float x, float y, float w,
                                          float h, float z) {
    int layer = upload_overlay_to_array_layer(pixels, tex_w, tex_h);
    if (layer < 0)
        return;
    push_overlay_quad(layer, x, y, w, h, z, tex_w, tex_h, 0, 0, NULL);
}

/** @brief Draw an overlay sprite with optional flipping. */
void SDLGameRendererGPU_DrawOverlaySpriteEx(const uint32_t* pixels, int tex_w, int tex_h, float x, float y, float w,
                                            float h, float z, int flip_x, int flip_y) {
    int layer = upload_overlay_to_array_layer(pixels, tex_w, tex_h);
    if (layer < 0)
        return;
    push_overlay_quad(layer, x, y, w, h, z, tex_w, tex_h, flip_x, flip_y, NULL);
}

/** @brief Queue a standalone GPU texture into the z-sorted batch (oversized overlays). */
void SDLGameRendererGPU_QueueDeferredBlit(SDL_GPUTexture* texture, int tex_w, int tex_h, float x, float y, float w,
                                          float h, float z, int flip_x, int flip_y) {
    if (!texture)
        return;
    /* Push a quad with layer=-2 (standalone overlay sentinel) */
    push_overlay_quad(-2, x, y, w, h, z, tex_w, tex_h, flip_x, flip_y, texture);
}

static void push_overlay_subquad(int layer, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                                 float z, SDL_GPUTexture* standalone_tex) {
    int p = (z < 0.1f) ? 1 : 2;
    if (!mapped_vertex_ptr || vertex_count + 4 > MAX_VERTICES || pass_state[p].quad_count >= MAX_QUADS)
        return;

    GPUVertex* v = (GPUVertex*)mapped_vertex_ptr + vertex_count;
    float fl = (float)layer;
    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    v[0] = (GPUVertex) { sx, sy, 1, 1, 1, 1, u0, v0, fl, -1.0f };
    v[1] = (GPUVertex) { sx + sw, sy, 1, 1, 1, 1, u1, v0, fl, -1.0f };
    v[2] = (GPUVertex) { sx, sy + sh, 1, 1, 1, 1, u0, v1, fl, -1.0f };
    v[3] = (GPUVertex) { sx + sw, sy + sh, 1, 1, 1, 1, u1, v1, fl, -1.0f };

    pass_state[p].sort_keys[pass_state[p].quad_count].z = z;
    pass_state[p].sort_keys[pass_state[p].quad_count].original_index = pass_state[p].quad_count;
    pass_state[p].sort_keys[pass_state[p].quad_count].global_quad_index = vertex_count / 4;
    pass_state[p].sort_keys[pass_state[p].quad_count].blend_mode = s_current_blend_mode;
    pass_state[p].overlay_tex[pass_state[p].quad_count] = standalone_tex;

    if (z < s_last_submitted_z) {
        s_sort_inversions++;
    }
    s_last_submitted_z = z;

    pass_state[p].quad_count++;
    vertex_count += 4;
}

void SDLGameRendererGPU_DrawOverlaySubSprite(const uint32_t* pixels, int tex_w, int tex_h, float x, float y, float w,
                                             float h, float u0, float v0, float u1, float v1, float z) {
    int layer = upload_overlay_to_array_layer(pixels, tex_w, tex_h);
    if (layer < 0)
        return;
    push_overlay_subquad(layer, x, y, w, h, u0, v0, u1, v1, z, NULL);
}

void SDLGameRendererGPU_QueueDeferredSubBlit(SDL_GPUTexture* texture, int tex_w, int tex_h, float x, float y, float w,
                                             float h, float u0, float v0, float u1, float v1, float z) {
    if (!texture)
        return;
    push_overlay_subquad(-2, x, y, w, h, u0, v0, u1, v1, z, texture);
}

void SDLGameRendererGPU_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z) {
    GPUTextureMetadataC meta;
    if (!TextureUtil_GetGPUMetadata(texture, &meta))
        return;
    if (meta.w > 512 || meta.h > 512) {
        SDLGameRendererGPU_QueueDeferredBlit((SDL_GPUTexture*)meta.texture, meta.w, meta.h, x, y, w, h, z, 0, 0);
    } else {
        SDLGameRendererGPU_DrawOverlaySprite(meta.pixels, meta.w, meta.h, x, y, w, h, z);
    }
}

void SDLGameRendererGPU_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                          int flip_y) {
    GPUTextureMetadataC meta;
    if (!TextureUtil_GetGPUMetadata(texture, &meta))
        return;
    if (meta.w > 512 || meta.h > 512) {
        SDLGameRendererGPU_QueueDeferredBlit((SDL_GPUTexture*)meta.texture, meta.w, meta.h, x, y, w, h, z, flip_x,
                                             flip_y);
    } else {
        SDLGameRendererGPU_DrawOverlaySpriteEx(meta.pixels, meta.w, meta.h, x, y, w, h, z, flip_x, flip_y);
    }
}

void SDLGameRendererGPU_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0, float v0,
                                             float u1, float v1, float z) {
    GPUTextureMetadataC meta;
    if (!TextureUtil_GetGPUMetadata(texture, &meta))
        return;
    if (meta.w > 512 || meta.h > 512) {
        SDLGameRendererGPU_QueueDeferredSubBlit((SDL_GPUTexture*)meta.texture, meta.w, meta.h, x, y, w, h, u0, v0, u1,
                                                v1, z);
    } else {
        SDLGameRendererGPU_DrawOverlaySubSprite(meta.pixels, meta.w, meta.h, x, y, w, h, u0, v0, u1, v1, z);
    }
}

