/**
 * @file sdl_game_renderer_gpu.c
 * @brief SDL_GPU rendering backend implementation.
 *
 * Full renderer using SDL3's GPU API with compute shader-based texture
 * decoding, batched vertex rendering, and palette management. Alternative
 * to the OpenGL backend for platforms with SDL_GPU support.
 */
#include "common.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <libgraph.h>
#include <stdio.h>
#include <stdlib.h>

// ⚡ Bolt: SIMDe — portable SIMD intrinsics (SSE2 on x86, NEON on ARM, scalar fallback).
#include <simde/x86/sse2.h>
#include <simde/x86/ssse3.h> // pshufb for 4-bit palette LUT vectorization

static SDL_GPUDevice* device = NULL;
static SDL_Window* window = NULL;
static SDL_GPUCommandBuffer* current_cmd_buf = NULL;
static SDL_GPUGraphicsPipeline* pipeline = NULL;
static SDL_GPUComputePipeline* s_compute_pipeline = NULL; // Palette conversion pipeline
static SDL_GPUSampler* sampler = NULL;

static SDL_GPUBuffer* vertex_buffer = NULL;
static SDL_GPUBuffer* index_buffer = NULL;
// ⚡ Triple-buffered vertex transfer — decouples CPU writes from GPU reads.
// Mirrors GL backend's OFFSET_BUFFER_COUNT=3 persistent-mapped pattern.
#define VERTEX_TRANSFER_BUFFER_COUNT 3
static SDL_GPUTransferBuffer* transfer_buffers[VERTEX_TRANSFER_BUFFER_COUNT] = { NULL };
static SDL_GPUTransferBuffer* index_transfer_buffer = NULL; // Dynamic index uploads each frame
static int current_transfer_idx = 0;

// Compute Shader Resources
#define COMPUTE_STORAGE_SIZE (16 * 1024 * 1024)                // 16MB shared buffer for raw pixels + palettes
static SDL_GPUBuffer* s_compute_storage_buffer = NULL;         // GPU-resident
static SDL_GPUTransferBuffer* s_compute_staging_buffer = NULL; // CPU-to-GPU transfer
static u8* s_compute_staging_ptr = NULL;                       // Mapped pointer
static size_t s_compute_staging_offset = 0;
static int s_compute_drops_last_frame = 0;

static float* mapped_vertex_ptr = NULL;
static unsigned int vertex_count = 0;

static SDL_GPUTexture* s_swapchain_texture = NULL;
static SDL_GPUTexture* s_canvas_texture = NULL;

// Texture Array Management
#define TEX_ARRAY_SIZE 512
#define TEX_ARRAY_MAX_LAYERS 256
static SDL_GPUTexture* texture_array = NULL;
static int tex_array_free[TEX_ARRAY_MAX_LAYERS];
static int tex_array_free_count = 0;
// Map (texture_handle-1, palette_handle) → array layer index, or -1 if not in array
static int16_t tex_array_layer[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];

// Stacks for current frame texture state
static int texture_layers[FL_PALETTE_MAX];
static float texture_uv_sx[FL_PALETTE_MAX];
static float texture_uv_sy[FL_PALETTE_MAX];
static int texture_count = 0;

static SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };

// Dirty flags
static bool texture_dirty_flags[FL_TEXTURE_MAX] = { false };
static bool palette_dirty_flags[FL_PALETTE_MAX] = { false };

// Dirty-index lists
static int dirty_texture_indices[FL_TEXTURE_MAX];
static int dirty_texture_count = 0;
static int dirty_palette_indices[FL_PALETTE_MAX];
static int dirty_palette_count = 0;

// Hash-based dirty detection
static uint32_t palette_hash[FL_PALETTE_MAX] = { 0 };
static uint32_t texture_hash[FL_TEXTURE_MAX] = { 0 };

// FNV-1a hash of a raw memory block
static inline uint32_t hash_memory(const void* ptr, size_t len) {
    uint32_t h = 2166136261u;
    const uint8_t* data = (const uint8_t*)ptr;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

// Compute Job Queue
#define MAX_COMPUTE_JOBS 256
typedef struct {
    Uint32 width;
    Uint32 height;
    Uint32 format; // 0=8bit, 1=4bit, 2=16bit
    Uint32 layer;
    Uint32 pixel_offset;
    Uint32 palette_offset;
    Uint32 pitch;
} ComputeJob;
static ComputeJob s_compute_jobs[MAX_COMPUTE_JOBS];
static int s_compute_job_count = 0;

#define MAX_VERTICES 65536
#define MAX_QUADS (MAX_VERTICES / 4)

// Z-depth sorting
typedef struct {
    float z;
    int original_index; // quad index in submission order
} QuadSortKey;
static QuadSortKey quad_sort_keys[MAX_QUADS];
static unsigned int quad_count = 0;

// Merge sort temp buffer
static QuadSortKey quad_sort_temp[MAX_QUADS];

typedef struct GPUVertex {
    float x, y;
    float r, g, b, a;
    float u, v;
    float layer;
} GPUVertex;

// --- CLUT Shuffle for PS2 ---
#define clut_shuf(x) (((x) & ~0x18) | ((((x) & 0x08) << 1) | (((x) & 0x10) >> 1)))

static void read_rgba32_color(Uint32 pixel, SDL_Color* color) {
    color->r = (pixel >> 0) & 0xFF;
    color->g = (pixel >> 8) & 0xFF;
    color->b = (pixel >> 16) & 0xFF;
    color->a = (pixel >> 24) & 0xFF;
}

static void read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = (pixel & 0x1F) * 255 / 31;
    color->g = ((pixel >> 5) & 0x1F) * 255 / 31;
    color->b = ((pixel >> 10) & 0x1F) * 255 / 31;
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void* LoadShaderCode(const char* filename, size_t* size) {
    void* code = SDL_LoadFile(filename, size);
    if (!code) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Failed to load shader: %s", filename);
    }
    return code;
}

static SDL_GPUShader* CreateGPUShader(const char* filename, SDL_GPUShaderStage stage) {
    size_t size;
    void* code = LoadShaderCode(filename, &size);
    if (!code)
        return NULL;

    SDL_ShaderCross_SPIRV_Info info;
    SDL_zero(info);
    info.bytecode = (const Uint8*)code;
    info.bytecode_size = size;
    info.entrypoint = "main";
    info.shader_stage = (SDL_ShaderCross_ShaderStage)stage;

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);

    if (!metadata) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to reflect SPIRV: %s", filename);
        SDL_free(code);
        return NULL;
    }

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &info, &metadata->resource_info, 0);

    if (!shader) {
        SDL_LogError(
            SDL_LOG_CATEGORY_RENDER, "CompileGraphicsShaderFromSPIRV failed for %s: %s", filename, SDL_GetError());
    }

    SDL_free(metadata);
    SDL_free(code);
    return shader;
}

static SDL_GPUComputePipeline* CreateGPUComputePipeline(const char* filename) {
    size_t size;
    void* code = LoadShaderCode(filename, &size);
    if (!code)
        return NULL;

    SDL_ShaderCross_SPIRV_Info info;
    SDL_zero(info);
    info.bytecode = (const Uint8*)code;
    info.bytecode_size = size;
    info.entrypoint = "main";
    info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;

    SDL_ShaderCross_ComputePipelineMetadata* metadata =
        SDL_ShaderCross_ReflectComputeSPIRV(info.bytecode, info.bytecode_size, 0);

    if (!metadata) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to reflect Compute SPIRV: %s", filename);
        SDL_free(code);
        return NULL;
    }

    SDL_GPUComputePipeline* pipeline = SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &info, metadata, 0);

    if (!pipeline) {
        SDL_LogError(
            SDL_LOG_CATEGORY_RENDER, "CompileComputePipelineFromSPIRV failed for %s: %s", filename, SDL_GetError());
    }

    SDL_free(metadata);
    SDL_free(code);
    return pipeline;
}

/** @brief Initialize the SDL_GPU renderer backend (Device, Window, Shaders, Pipelines). */
void SDLGameRendererGPU_Init(void) {
    SDL_Log("SDLGameRendererGPU_Init: Initializing SDL_GPU renderer backend.");
    device = SDLApp_GetGPUDevice();
    window = SDLApp_GetWindow();

    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDLGameRendererGPU_Init: No GPU device found!");
        return;
    }

    if (!SDL_ShaderCross_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to initialize SDL_ShaderCross: %s", SDL_GetError());
        return;
    }

    // Load Shaders (Expects SPIR-V)
    const char* base_path = SDL_GetBasePath();
    char vert_path[1024];
    char frag_path[1024];
    char comp_path[1024];
    snprintf(vert_path, sizeof(vert_path), "%sshaders/vert.spv", base_path);
    snprintf(frag_path, sizeof(frag_path), "%sshaders/scene.spv", base_path);
    snprintf(comp_path, sizeof(comp_path), "%sshaders/palette_convert.comp.spv", base_path);

    SDL_GPUShader* vert_shader = CreateGPUShader(vert_path, SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* frag_shader = CreateGPUShader(frag_path, SDL_GPU_SHADERSTAGE_FRAGMENT);
    s_compute_pipeline = CreateGPUComputePipeline(comp_path);

    if (!vert_shader || !frag_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create graphics shaders.");
        return;
    }
    if (!s_compute_pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                     "Failed to create compute pipeline: %s. "
                     "GPU backend requires compute shader support. "
                     "Use --renderer gl for devices without compute shaders.",
                     SDL_GetError());
        return;
    }
    SDL_Log("GPU Compute palette conversion pipeline initialized successfully.");

    // Create Graphics Pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader = vert_shader;
    pipeline_info.fragment_shader = frag_shader;

    SDL_GPUVertexAttribute attributes[4];
    SDL_zero(attributes);
    // Pos
    attributes[0].location = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = 0;
    attributes[0].buffer_slot = 0;
    // Color
    attributes[1].location = 1;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[1].offset = 2 * sizeof(float);
    attributes[1].buffer_slot = 0;
    // TexCoord
    attributes[2].location = 2;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[2].offset = 6 * sizeof(float);
    attributes[2].buffer_slot = 0;
    // Layer
    attributes[3].location = 3;
    attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    attributes[3].offset = 8 * sizeof(float);
    attributes[3].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes = 4;

    SDL_GPUVertexBufferDescription bindings[1];
    SDL_zero(bindings);
    bindings[0].slot = 0;
    bindings[0].pitch = 9 * sizeof(float);
    bindings[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = bindings;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    // Must match canvas texture format (R8G8B8A8_UNORM), NOT the swapchain format.
    // The scene pipeline renders to the off-screen canvas, not directly to the swapchain.
    color_target_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    color_target_desc.blend_state.enable_blend = true;
    color_target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    SDL_ReleaseGPUShader(device, vert_shader);
    SDL_ReleaseGPUShader(device, frag_shader);

    if (!pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create GPU pipeline: %s", SDL_GetError());
        return;
    }

    // Create Vertex Buffer & Transfer Buffer
    SDL_GPUBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size = MAX_VERTICES * sizeof(GPUVertex);
    vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);

    SDL_GPUTransferBufferCreateInfo tb_info;
    SDL_zero(tb_info);
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = MAX_VERTICES * sizeof(GPUVertex);
    for (int i = 0; i < VERTEX_TRANSFER_BUFFER_COUNT; i++) {
        transfer_buffers[i] = SDL_CreateGPUTransferBuffer(device, &tb_info);
    }

    // Create Compute Storage Buffer (GPU Resident)
    SDL_GPUBufferCreateInfo sb_info = { .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
                                        .size = COMPUTE_STORAGE_SIZE };
    s_compute_storage_buffer = SDL_CreateGPUBuffer(device, &sb_info);

    // Create Compute Staging Buffer (Transfer)
    SDL_GPUTransferBufferCreateInfo ttb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                 .size = COMPUTE_STORAGE_SIZE };
    s_compute_staging_buffer = SDL_CreateGPUTransferBuffer(device, &ttb_info);

    // Create Index Buffer (Static Quad Indices)
    const int max_quads = MAX_VERTICES / 4;
    size_t ib_size = max_quads * 6 * sizeof(Uint16);
    SDL_GPUBufferCreateInfo ib_info;
    SDL_zero(ib_info);
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = ib_size;
    index_buffer = SDL_CreateGPUBuffer(device, &ib_info);

    bool any_transfer_missing = false;
    for (int i = 0; i < VERTEX_TRANSFER_BUFFER_COUNT; i++) {
        if (!transfer_buffers[i])
            any_transfer_missing = true;
    }
    if (!vertex_buffer || any_transfer_missing || !s_compute_staging_buffer || !index_buffer ||
        !s_compute_storage_buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create GPU buffers: %s", SDL_GetError());
        return;
    }

    // Upload indices
    SDL_GPUTransferBufferCreateInfo ib_tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = ib_size };
    SDL_GPUTransferBuffer* ib_tb = SDL_CreateGPUTransferBuffer(device, &ib_tb_info);
    if (!ib_tb) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create index transfer buffer: %s", SDL_GetError());
        return;
    }

    Uint16* indices = (Uint16*)SDL_MapGPUTransferBuffer(device, ib_tb, false);
    if (indices) {
        for (int i = 0; i < max_quads; i++) {
            indices[i * 6 + 0] = i * 4 + 0;
            indices[i * 6 + 1] = i * 4 + 1;
            indices[i * 6 + 2] = i * 4 + 2;
            indices[i * 6 + 3] = i * 4 + 2;
            indices[i * 6 + 4] = i * 4 + 1;
            indices[i * 6 + 5] = i * 4 + 3;
        }
        SDL_UnmapGPUTransferBuffer(device, ib_tb);

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);
        SDL_GPUTransferBufferLocation src = { .transfer_buffer = ib_tb, .offset = 0 };
        SDL_GPUBufferRegion dst = { .buffer = index_buffer, .offset = 0, .size = ib_size };
        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_SubmitGPUCommandBuffer(cb);
    }
    SDL_ReleaseGPUTransferBuffer(device, ib_tb);

    // Index Transfer Buffer for dynamic per-frame index uploads
    SDL_GPUTransferBufferCreateInfo itb_info;
    SDL_zero(itb_info);
    itb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    itb_info.size = ib_size;
    index_transfer_buffer = SDL_CreateGPUTransferBuffer(device, &itb_info);

    // Create Sampler
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &sampler_info);

    // Create Texture Array
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    // Note: Added COMPUTE_STORAGE_WRITE usage
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    tex_info.width = TEX_ARRAY_SIZE;
    tex_info.height = TEX_ARRAY_SIZE;
    tex_info.layer_count_or_depth = TEX_ARRAY_MAX_LAYERS;
    tex_info.num_levels = 1;

    texture_array = SDL_CreateGPUTexture(device, &tex_info);

    // Create Canvas Texture (384x224)
    SDL_GPUTextureCreateInfo canvas_info;
    SDL_zero(canvas_info);
    canvas_info.type = SDL_GPU_TEXTURETYPE_2D;
    canvas_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    canvas_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    canvas_info.width = 384;
    canvas_info.height = 224;
    canvas_info.layer_count_or_depth = 1;
    canvas_info.num_levels = 1;
    s_canvas_texture = SDL_CreateGPUTexture(device, &canvas_info);

    if (!sampler || !texture_array || !s_canvas_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create sampler, texture array, or canvas: %s", SDL_GetError());
        return;
    }

    // Init free list (layers 0..MAX-1)
    // Note: We used to clear layer 0 to white, but now we can just leave it or clear via compute later if needed.
    // For simplicity, we initialize all layers as free.
    tex_array_free_count = TEX_ARRAY_MAX_LAYERS;
    for (int i = 0; i < tex_array_free_count; i++) {
        tex_array_free[i] = TEX_ARRAY_MAX_LAYERS - 1 - i;
    }
    memset(tex_array_layer, -1, sizeof(tex_array_layer));

    SDL_Log("SDLGameRendererGPU_Init: Complete.");
}

/** @brief Shutdown the SDL_GPU renderer and release all resources. */
void SDLGameRendererGPU_Shutdown(void) {
    if (pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    if (s_compute_pipeline)
        SDL_ReleaseGPUComputePipeline(device, s_compute_pipeline);
    if (vertex_buffer)
        SDL_ReleaseGPUBuffer(device, vertex_buffer);
    if (index_buffer)
        SDL_ReleaseGPUBuffer(device, index_buffer);
    for (int i = 0; i < VERTEX_TRANSFER_BUFFER_COUNT; i++) {
        if (transfer_buffers[i])
            SDL_ReleaseGPUTransferBuffer(device, transfer_buffers[i]);
    }
    if (s_compute_staging_buffer)
        SDL_ReleaseGPUTransferBuffer(device, s_compute_staging_buffer);
    if (s_compute_storage_buffer)
        SDL_ReleaseGPUBuffer(device, s_compute_storage_buffer);
    if (texture_array)
        SDL_ReleaseGPUTexture(device, texture_array);
    if (s_canvas_texture)
        SDL_ReleaseGPUTexture(device, s_canvas_texture);
    if (sampler)
        SDL_ReleaseGPUSampler(device, sampler);
    SDL_ShaderCross_Quit();
}

/** @brief Begin a new frame: acquire command buffer and swapchain texture. */
void SDLGameRendererGPU_BeginFrame(void) {
    TRACE_ZONE_N("GPU:BeginFrame");
    if (!device) {
        TRACE_ZONE_END();
        return;
    }

    current_cmd_buf = SDL_AcquireGPUCommandBuffer(device);
    s_swapchain_texture = NULL; // Acquired lazily via GetSwapchainTexture()

    // Drain dirty-index lists
    for (int d = 0; d < dirty_texture_count; d++) {
        const int i = dirty_texture_indices[d];
        for (int pal = 0; pal <= FL_PALETTE_MAX; ++pal) {
            if (tex_array_layer[i][pal] >= 0) {
                tex_array_free[tex_array_free_count++] = tex_array_layer[i][pal];
                tex_array_layer[i][pal] = -1;
            }
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
        for (int tex = 0; tex < FL_TEXTURE_MAX; ++tex) {
            if (tex_array_layer[tex][i + 1] >= 0) {
                tex_array_free[tex_array_free_count++] = tex_array_layer[tex][i + 1];
                tex_array_layer[tex][i + 1] = -1;
            }
        }
        if (palettes[i]) {
            SDL_DestroyPalette(palettes[i]);
            palettes[i] = NULL;
        }
        SDLGameRendererGPU_CreatePalette((i + 1) << 16);
        palette_dirty_flags[i] = false;
    }
    dirty_palette_count = 0;

    current_transfer_idx = (current_transfer_idx + 1) % VERTEX_TRANSFER_BUFFER_COUNT;
    mapped_vertex_ptr = (float*)SDL_MapGPUTransferBuffer(device, transfer_buffers[current_transfer_idx], true);

    // Map Compute Staging Buffer for the frame
    s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
    s_compute_staging_offset = 0;

    vertex_count = 0;
    quad_count = 0;
    texture_count = 0;
    s_compute_job_count = 0;

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

    if (!current_cmd_buf || !window) {
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

    // Unmap staging buffer
    SDL_UnmapGPUTransferBuffer(device, s_compute_staging_buffer);
    s_compute_staging_ptr = NULL;

    // --- 1. Copy Pass (Buffers) ---
    if (s_compute_staging_offset > 0 || vertex_count > 0 || index_count > 0) {
        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(current_cmd_buf);

        // Upload Compute Staging -> Compute Storage
        if (s_compute_staging_offset > 0) {
            SDL_GPUTransferBufferLocation src = { .transfer_buffer = s_compute_staging_buffer, .offset = 0 };
            SDL_GPUBufferRegion dst = { .buffer = s_compute_storage_buffer,
                                        .offset = 0,
                                        .size = s_compute_staging_offset };
            SDL_UploadToGPUBuffer(copy_pass, &src, &dst, true);
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

        SDL_EndGPUCopyPass(copy_pass);
    }

    // --- 2. Compute Pass (Palette Conversion) ---
    if (s_compute_job_count > 0 && s_compute_pipeline) {
        // Each job needs a different array layer as write target.
        // Read-write storage textures must be bound at pass creation via
        // SDL_BeginGPUComputePass, so we create one pass per job.

        for (int i = 0; i < s_compute_job_count; i++) {
            ComputeJob* job = &s_compute_jobs[i];

            SDL_GPUStorageTextureReadWriteBinding rw_binding = {
                .texture = texture_array, .mip_level = 0, .layer = job->layer, .cycle = false
            };

            SDL_GPUComputePass* compute_pass =
                SDL_BeginGPUComputePass(current_cmd_buf,
                                        &rw_binding,
                                        1, // Read-write storage texture (imageStore target)
                                        NULL,
                                        0 // No read-write storage buffers
                );

            SDL_BindGPUComputePipeline(compute_pass, s_compute_pipeline);

            // Bind raw pixel+palette data as read-only storage buffer
            SDL_GPUBuffer* storage_buffers[] = { s_compute_storage_buffer };
            SDL_BindGPUComputeStorageBuffers(compute_pass, 0, storage_buffers, 1);

            Uint32 uniforms[7] = { job->width,        job->height,         job->format, 0,
                                   job->pixel_offset, job->palette_offset, job->pitch };
            SDL_PushGPUComputeUniformData(current_cmd_buf, 0, uniforms, sizeof(uniforms));

            uint32_t group_x = (job->width + 7) / 8;
            uint32_t group_y = (job->height + 7) / 8;
            SDL_DispatchGPUCompute(compute_pass, group_x, group_y, 1);

            SDL_EndGPUComputePass(compute_pass);
        }
    }

    // --- 3. Render Pass ---
    if (s_canvas_texture) {
        SDL_GPUColorTargetInfo color_target;
        SDL_zero(color_target);
        color_target.texture = s_canvas_texture;
        color_target.clear_color = (SDL_FColor) { 0.0f, 0.0f, 0.0f, 1.0f };
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

                SDL_GPUTextureSamplerBinding tex_binding;
                tex_binding.texture = texture_array;
                tex_binding.sampler = sampler;
                SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

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
        SDL_SubmitGPUCommandBuffer(current_cmd_buf);
        current_cmd_buf = NULL;
    }
    s_swapchain_texture = NULL;
    TRACE_ZONE_END();
}

SDL_GPUCommandBuffer* SDLGameRendererGPU_GetCommandBuffer(void) {
    return current_cmd_buf;
}

/** @brief Create a CPU-side surface for a game texture (lazy upload). */
void SDLGameRendererGPU_CreateTexture(unsigned int th) {
    const int texture_index = LO_16_BITS(th) - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (surfaces[texture_index]) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
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

    surfaces[texture_index] =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
}

/** @brief Mark a texture for destruction. */
void SDLGameRendererGPU_DestroyTexture(unsigned int texture_handle) {
    const int idx = texture_handle - 1;
    if (idx >= 0 && idx < FL_TEXTURE_MAX) {
        if (surfaces[idx]) {
            SDL_DestroySurface(surfaces[idx]);
            surfaces[idx] = NULL;
        }
        if (!texture_dirty_flags[idx]) {
            texture_dirty_flags[idx] = true;
            dirty_texture_indices[dirty_texture_count++] = idx;
        }
        texture_hash[idx] = 0; // Reset hash on destroy
    }
}

/** @brief Create a CPU-side palette. */
void SDLGameRendererGPU_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    SDL_Color colors[256];
    size_t color_size = (fl_palette->format == SCE_GS_PSMCT32) ? 4 : 2;

    switch (color_count) {
    case 16:
        if (color_size == 4) {
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 16; i++) {
                read_rgba32_color(rgba32[i], &colors[i]);
                colors[i].a = (colors[i].a == 0x80) ? 0xFF : (colors[i].a << 1);
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 16; i++)
                read_rgba16_color(rgba16[i], &colors[i]);
        }
        colors[0].a = 0;
        break;
    case 256:
        if (color_size == 4) {
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 256; i++) {
                read_rgba32_color(rgba32[clut_shuf(i)], &colors[i]);
                colors[i].a = (colors[i].a == 0x80) ? 0xFF : (colors[i].a << 1);
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba16_color(rgba16[clut_shuf(i)], &colors[i]);
        }
        colors[0].a = 0;
        break;
    }

    if (palettes[palette_index])
        SDL_DestroyPalette(palettes[palette_index]);
    palettes[palette_index] = SDL_CreatePalette(color_count);
    SDL_SetPaletteColors(palettes[palette_index], colors, 0, color_count);
}

/** @brief Mark a palette for destruction. */
void SDLGameRendererGPU_DestroyPalette(unsigned int palette_handle) {
    const int idx = palette_handle - 1;
    if (idx >= 0 && idx < FL_PALETTE_MAX) {
        if (palettes[idx]) {
            SDL_DestroyPalette(palettes[idx]);
            palettes[idx] = NULL;
        }
        if (!palette_dirty_flags[idx]) {
            palette_dirty_flags[idx] = true;
            dirty_palette_indices[dirty_palette_count++] = idx;
        }
        palette_hash[idx] = 0; // Reset hash on destroy
    }
}

void SDLGameRendererGPU_UnlockTexture(unsigned int th) {
    const int idx = th - 1;
    if (idx >= 0 && idx < FL_TEXTURE_MAX) {
        const FLTexture* fl_tex = &flTexture[idx];
        const void* pixels = flPS2GetSystemBuffAdrs(fl_tex->mem_handle);
        size_t data_size = 0;
        switch (fl_tex->format) {
        case SCE_GS_PSMT8:
            data_size = (size_t)fl_tex->width * fl_tex->height;
            break;
        case SCE_GS_PSMT4:
            data_size = (size_t)((fl_tex->width + 1) / 2) * fl_tex->height;
            break;
        case SCE_GS_PSMCT16:
            data_size = (size_t)fl_tex->width * fl_tex->height * 2;
            break;
        default:
            data_size = (size_t)fl_tex->width * fl_tex->height * 4;
            break;
        }
        if (pixels && data_size > 0) {
            uint32_t new_hash = hash_memory(pixels, data_size);
            if (new_hash == texture_hash[idx]) {
                return;
            }
            texture_hash[idx] = new_hash;
        }
        for (int pal = 0; pal <= FL_PALETTE_MAX; ++pal) {
            if (tex_array_layer[idx][pal] >= 0) {
                tex_array_free[tex_array_free_count++] = tex_array_layer[idx][pal];
                tex_array_layer[idx][pal] = -1;
            }
        }
    }
}

void SDLGameRendererGPU_UnlockPalette(unsigned int ph) {
    const int idx = ph - 1;
    if (idx >= 0 && idx < FL_PALETTE_MAX) {
        const FLTexture* fl_pal = &flPalette[idx];
        const void* pixels = flPS2GetSystemBuffAdrs(fl_pal->mem_handle);
        size_t color_count = (size_t)fl_pal->width * fl_pal->height;
        size_t color_size = (fl_pal->format == SCE_GS_PSMCT32) ? 4 : 2;
        size_t data_size = color_count * color_size;
        if (pixels && data_size > 0) {
            uint32_t new_hash = hash_memory(pixels, data_size);
            if (new_hash == palette_hash[idx]) {
                return;
            }
            palette_hash[idx] = new_hash;
        }
        for (int tex = 0; tex < FL_TEXTURE_MAX; ++tex) {
            if (tex_array_layer[tex][idx + 1] >= 0) {
                tex_array_free[tex_array_free_count++] = tex_array_layer[tex][idx + 1];
                tex_array_layer[tex][idx + 1] = -1;
            }
        }
        if (palettes[idx]) {
            SDL_DestroyPalette(palettes[idx]);
            palettes[idx] = NULL;
        }
        SDLGameRendererGPU_CreatePalette((idx + 1) << 16);
    }
}

/** @brief Prepare a texture for rendering, uploading to the GPU array if needed. */
void SDLGameRendererGPU_SetTexture(unsigned int th) {
    TRACE_ZONE_N("GPU:SetTexture");
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;
    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        TRACE_ZONE_END();
        return;
    }

    if (!surfaces[texture_handle - 1]) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Texture %d has no surface!", texture_handle);
        TRACE_ZONE_END();
        return;
    }

    int layer = tex_array_layer[texture_handle - 1][palette_handle];

    if (layer < 0) {
        if (tex_array_free_count > 0 && s_compute_staging_ptr) {
            layer = tex_array_free[--tex_array_free_count];
            tex_array_layer[texture_handle - 1][palette_handle] = layer;

            SDL_Surface* surface = surfaces[texture_handle - 1];
            const SDL_Palette* palette = (palette_handle > 0) ? palettes[palette_handle - 1] : NULL;
            const FLTexture* fl_texture = &flTexture[texture_handle - 1];

            // Calculate sizes
            // Pitch is already in surface->pitch (which comes from fl_texture->width or (width+1)/2)
            size_t pixel_data_size = surface->h * surface->pitch;
            size_t palette_data_size = 0;
            if (palette)
                palette_data_size = palette->ncolors * 4; // RGBA8888 packed

            size_t total_size = pixel_data_size + palette_data_size;

            // Check if we have space in staging buffer
            if (s_compute_job_count < MAX_COMPUTE_JOBS &&
                s_compute_staging_offset + total_size <= COMPUTE_STORAGE_SIZE) {

                // Copy Pixel Data
                Uint32 pixel_offset = (Uint32)s_compute_staging_offset;
                memcpy(s_compute_staging_ptr + s_compute_staging_offset, surface->pixels, pixel_data_size);
                s_compute_staging_offset += pixel_data_size;

                // Ensure 4-byte alignment for palette data (required for u32* cast and Shader uint reading)
                s_compute_staging_offset = (s_compute_staging_offset + 3) & ~3;

                // Copy Palette Data
                Uint32 palette_offset = 0;
                if (palette) {
                    palette_offset = (Uint32)s_compute_staging_offset;
                    u32* pal_dest = (u32*)(s_compute_staging_ptr + s_compute_staging_offset);
                    for (int i = 0; i < palette->ncolors; i++) {
                        SDL_Color c = palette->colors[i];
                        // Pack RGBA8888
                        pal_dest[i] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r;
                    }
                    s_compute_staging_offset += palette_data_size;
                }

                // Add Job
                ComputeJob* job = &s_compute_jobs[s_compute_job_count++];
                job->width = surface->w;
                job->height = surface->h;
                job->layer = layer;
                job->pixel_offset = pixel_offset;
                job->palette_offset = palette_offset; // Byte offset
                job->pitch = surface->pitch;

                if (fl_texture->format == SCE_GS_PSMT4)
                    job->format = 1;
                else if (fl_texture->format == SCE_GS_PSMCT16)
                    job->format = 2;
                else
                    job->format = 0; // 8-bit

            } else {
                s_compute_drops_last_frame++;
                tex_array_free[tex_array_free_count++] = layer;
                tex_array_layer[texture_handle - 1][palette_handle] = -1;
            }
        }
    }

    if (texture_count >= FL_PALETTE_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Texture stack overflow!");
        return;
    }

    if (layer < 0)
        layer = 0;

    texture_layers[texture_count] = layer;
    texture_uv_sx[texture_count] = (float)surfaces[texture_handle - 1]->w / TEX_ARRAY_SIZE;
    texture_uv_sy[texture_count] = (float)surfaces[texture_handle - 1]->h / TEX_ARRAY_SIZE;
    texture_count++;
    TRACE_ZONE_END();
}

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (!mapped_vertex_ptr || vertex_count + 4 > MAX_VERTICES)
        return;

    float layer = 0.0f;
    float uv_sx = 1.0f, uv_sy = 1.0f;

    if (textured && texture_count > 0) {
        layer = (float)texture_layers[texture_count - 1];
        uv_sx = texture_uv_sx[texture_count - 1];
        uv_sy = texture_uv_sy[texture_count - 1];
    }

    GPUVertex* v = (GPUVertex*)(mapped_vertex_ptr) + vertex_count;

    Uint32 c = vertices[0].color;
    float b = (c & 0xFF) / 255.0f;
    float g = ((c >> 8) & 0xFF) / 255.0f;
    float r = ((c >> 16) & 0xFF) / 255.0f;
    float a = ((c >> 24) & 0xFF) / 255.0f;

    v[0].x = vertices[0].coord.x;
    v[0].y = vertices[0].coord.y;
    v[0].r = r;
    v[0].g = g;
    v[0].b = b;
    v[0].a = a;
    v[0].u = vertices[0].tex_coord.s * uv_sx;
    v[0].v = vertices[0].tex_coord.t * uv_sy;
    v[0].layer = layer;

    v[1].x = vertices[1].coord.x;
    v[1].y = vertices[1].coord.y;
    v[1].r = r;
    v[1].g = g;
    v[1].b = b;
    v[1].a = a;
    v[1].u = vertices[1].tex_coord.s * uv_sx;
    v[1].v = vertices[1].tex_coord.t * uv_sy;
    v[1].layer = layer;

    v[2].x = vertices[2].coord.x;
    v[2].y = vertices[2].coord.y;
    v[2].r = r;
    v[2].g = g;
    v[2].b = b;
    v[2].a = a;
    v[2].u = vertices[2].tex_coord.s * uv_sx;
    v[2].v = vertices[2].tex_coord.t * uv_sy;
    v[2].layer = layer;

    v[3].x = vertices[3].coord.x;
    v[3].y = vertices[3].coord.y;
    v[3].r = r;
    v[3].g = g;
    v[3].b = b;
    v[3].a = a;
    v[3].u = vertices[3].tex_coord.s * uv_sx;
    v[3].v = vertices[3].tex_coord.t * uv_sy;
    v[3].layer = layer;

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

unsigned int SDLGameRendererGPU_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    return 0; // Not applicable
}

SDL_GPUTexture* SDLGameRendererGPU_GetSwapchainTexture(void) {
    if (!s_swapchain_texture && current_cmd_buf && window) {
        if (!SDL_AcquireGPUSwapchainTexture(current_cmd_buf, window, &s_swapchain_texture, NULL, NULL)) {
            s_swapchain_texture = NULL;
        }
    }
    return s_swapchain_texture;
}

SDL_GPUTexture* SDLGameRendererGPU_GetCanvasTexture(void) {
    return s_canvas_texture;
}
