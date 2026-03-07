/**
 * @file sdl_game_renderer_gpu.c
 * @brief SDL_GPU rendering backend implementation.
 *
 * Full renderer using SDL3's GPU API with compute shader-based texture
 * decoding, batched vertex rendering, and palette management. Alternative
 * to the OpenGL backend for platforms with SDL_GPU support.
 */
#include "common.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/tracy_zones.h"
#include "port/mods/modded_stage.h"
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
#include <simde/x86/sse4.2.h> // ⚡ Opt9c: CRC32 hardware hash

static SDL_GPUDevice* device = NULL;
static SDL_Window* window = NULL;
static SDL_GPUCommandBuffer* current_cmd_buf = NULL;
static SDL_GPUGraphicsPipeline* pipeline = NULL;
static SDL_GPUTexture* s_palette_texture = NULL;           // 256×256 RGBA8 palette atlas
static SDL_GPUTransferBuffer* s_palette_transfer = NULL;   // CPU-to-GPU palette upload
static SDL_GPUSampler* palette_sampler = NULL;
static SDL_GPUSampler* sampler = NULL;

static SDL_GPUBuffer* vertex_buffer = NULL;
static SDL_GPUBuffer* index_buffer = NULL;
// ⚡ Triple-buffered vertex transfer — decouples CPU writes from GPU reads.
// Mirrors GL backend's OFFSET_BUFFER_COUNT=3 persistent-mapped pattern.
#define VERTEX_TRANSFER_BUFFER_COUNT 3
static SDL_GPUTransferBuffer* transfer_buffers[VERTEX_TRANSFER_BUFFER_COUNT] = { NULL };
static SDL_GPUTransferBuffer* index_transfer_buffer = NULL; // Dynamic index uploads each frame
static int current_transfer_idx = 0;

// ⚡ Opt9: Fence-based async submit — decouple CPU from GPU completion.
// Ring depth matches VERTEX_TRANSFER_BUFFER_COUNT so that when we wait on
// fence[oldest], the transfer buffer from that frame is guaranteed released.
#define GPU_FENCE_RING_SIZE VERTEX_TRANSFER_BUFFER_COUNT
static SDL_GPUFence* s_frame_fences[GPU_FENCE_RING_SIZE] = { NULL };
static int s_fence_write_idx = 0;

// Texture Staging Resources
#define COMPUTE_STORAGE_SIZE (32 * 1024 * 1024)                // 32MB — RGBA8 texture uploads (4 bytes/pixel)
static SDL_GPUTransferBuffer* s_compute_staging_buffer = NULL; // CPU-to-GPU transfer
static u8* s_compute_staging_ptr = NULL;                       // Mapped pointer
static size_t s_compute_staging_offset = 0;
static int s_compute_drops_last_frame = 0;

// ⚡ Opt6: LZ77 GPU compute decompression
#define LZ77_MAX_JOBS 64
#define LZ77_INPUT_SIZE (512 * 1024)    // 512KB compressed data staging
#define LZ77_SWIZZLE_SIZE (1024 * 4)   // 1024 uint entries

typedef struct {
    Uint32 src_offset, src_size, dst_size, dst_layer;
    Uint32 dst_x, dst_y, tile_dim, texture_index;
} LZ77Job;

static struct {
    Uint32 job_count;
    Uint32 _pad1, _pad2, _pad3;
    LZ77Job jobs[LZ77_MAX_JOBS];
} s_lz77_uniforms;

static SDL_GPUComputePipeline* s_lz77_pipeline = NULL;
static SDL_GPUBuffer* s_lz77_input_buffer = NULL;
static SDL_GPUBuffer* s_lz77_swizzle_buffer = NULL;
static SDL_GPUTransferBuffer* s_lz77_upload_buf = NULL;
static u8* s_lz77_upload_ptr = NULL;
static size_t s_lz77_upload_offset = 0;
static int s_lz77_job_count = 0;
static bool s_lz77_available = false;
static bool s_lz77_swizzle_uploaded = false;

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
// Map texture_handle-1 → array layer index, or -1 if not in array (decoupled from palette)
static int16_t tex_array_layer[FL_TEXTURE_MAX];

#define MAX_VERTICES 65536
#define MAX_QUADS (MAX_VERTICES / 4)

// Stacks for current frame texture state
static int texture_layers[MAX_VERTICES];
static float texture_uv_sx[MAX_VERTICES];
static float texture_uv_sy[MAX_VERTICES];
static float texture_palette_idx[MAX_VERTICES]; // palette row in palette atlas (-1 = direct color)
static int texture_count = 0;

// Palette upload tracking
#define PALETTE_TEX_WIDTH 256
#define PALETTE_TEX_HEIGHT (FL_PALETTE_MAX + 1)  // 1089 rows — one per palette + 1 default identity row
#define DEFAULT_PALETTE_ROW FL_PALETTE_MAX         // Row 1088: identity grayscale ramp for palette-less indexed textures
static bool s_palette_uploaded[FL_PALETTE_MAX]; // per-palette dirty flag for atlas upload

// ⚡ Opt9b: Palette upload dirty list — avoids scanning all 1088 slots each frame.
static int s_pal_upload_dirty_indices[FL_PALETTE_MAX];
static int s_pal_upload_dirty_count = 0;

// ⚡ Back-to-back early-out cache — avoids redundant SetTexture work when the
// same texture+palette combo is set consecutively (common in sprite batches).
static unsigned int s_last_set_texture_handle = 0;

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

// ⚡ Opt9c: CRC32 hardware hash — 4 bytes/cycle vs FNV-1a's 1 byte/cycle.
// SIMDe auto-translates to ARM CRC32 on NEON targets.
static inline uint32_t hash_memory(const void* ptr, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint32_t* data32 = (const uint32_t*)ptr;
    size_t count32 = len / 4;
    for (size_t i = 0; i < count32; i++) {
        crc = simde_mm_crc32_u32(crc, data32[i]);
    }
    const uint8_t* tail = (const uint8_t*)(data32 + count32);
    size_t remaining = len & 3;
    for (size_t i = 0; i < remaining; i++) {
        crc = simde_mm_crc32_u8(crc, tail[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

// Texture Upload Job Queue (R8 indexed pixels → texture array)
#define MAX_COMPUTE_JOBS 256
typedef struct {
    Uint32 width;
    Uint32 height;
    Uint32 layer;
    Uint32 offset;
} TextureUploadJob;
static TextureUploadJob s_tex_upload_jobs[MAX_COMPUTE_JOBS];
static int s_tex_upload_count = 0;

// Palette Upload Job Queue (palette rows staged in s_compute_staging_buffer)
typedef struct {
    Uint32 row;    // palette index (row in palette atlas)
    Uint32 offset; // byte offset in staging buffer
} PaletteUploadJob;
static PaletteUploadJob s_pal_upload_jobs[MAX_COMPUTE_JOBS];
static int s_pal_upload_count = 0;



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
    float paletteIdx; // palette row in atlas, or -1.0 for direct RGBA
} GPUVertex;

// --- CLUT Shuffle for PS2 ---
// The PS2 GS stores 256-color CLUTs in a non-linear memory order.
// This LUT maps linear index (0-255) to the shuffled GS index.
static const Uint8 ps2_clut_shuffle[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23,
    8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 55,
    40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 80, 81, 82, 83, 84, 85, 86, 87,
    72, 73, 74, 75, 76, 77, 78, 79, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 112, 113, 114, 115, 116, 117, 118, 119,
    104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151,
    136, 137, 138, 139, 140, 141, 142, 143, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183,
    168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 208, 209, 210, 211, 212, 213, 214, 215,
    200, 201, 202, 203, 204, 205, 206, 207, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247,
    232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255
};
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
    snprintf(vert_path, sizeof(vert_path), "%sshaders/vert.spv", base_path);
    snprintf(frag_path, sizeof(frag_path), "%sshaders/scene.spv", base_path);
    SDL_GPUShader* vert_shader = CreateGPUShader(vert_path, SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* frag_shader = CreateGPUShader(frag_path, SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (!vert_shader || !frag_shader) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create graphics shaders.");
        return;
    }

    SDL_Log("GPU palette lookup via fragment shader (no compute pipeline needed).");

    // Create Graphics Pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);

    pipeline_info.vertex_shader = vert_shader;
    pipeline_info.fragment_shader = frag_shader;

    SDL_GPUVertexAttribute attributes[5];
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
    // PaletteIdx
    attributes[4].location = 4;
    attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    attributes[4].offset = 9 * sizeof(float);
    attributes[4].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes = 5;

    SDL_GPUVertexBufferDescription bindings[1];
    SDL_zero(bindings);
    bindings[0].slot = 0;
    bindings[0].pitch = 10 * sizeof(float);
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

    // Create Staging Buffer for texture pixel uploads
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
    if (!vertex_buffer || any_transfer_missing || !s_compute_staging_buffer || !index_buffer) {
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

    // Create Texture Array (RGBA8: indexed textures store index in R channel,
    // 16-bit direct color textures store full RGBA)
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    tex_info.width = TEX_ARRAY_SIZE;
    tex_info.height = TEX_ARRAY_SIZE;
    tex_info.layer_count_or_depth = TEX_ARRAY_MAX_LAYERS;
    tex_info.num_levels = 1;

    texture_array = SDL_CreateGPUTexture(device, &tex_info);

    // Create Palette Atlas (256×256, RGBA8 — each row is a 256-color palette)
    SDL_GPUTextureCreateInfo pal_info;
    SDL_zero(pal_info);
    pal_info.type = SDL_GPU_TEXTURETYPE_2D;
    pal_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    pal_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    pal_info.width = PALETTE_TEX_WIDTH;
    pal_info.height = PALETTE_TEX_HEIGHT;
    pal_info.layer_count_or_depth = 1;
    pal_info.num_levels = 1;
    s_palette_texture = SDL_CreateGPUTexture(device, &pal_info);

    // Transfer buffer for palette uploads (256 colors × 4 bytes × 256 palettes = 256KB)
    SDL_GPUTransferBufferCreateInfo ptb_info;
    SDL_zero(ptb_info);
    ptb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ptb_info.size = PALETTE_TEX_WIDTH * 4; // One row at a time (1KB)
    s_palette_transfer = SDL_CreateGPUTransferBuffer(device, &ptb_info);

    // Palette sampler (NEAREST for exact palette lookup)
    SDL_GPUSamplerCreateInfo pal_sampler_info;
    SDL_zero(pal_sampler_info);
    pal_sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    pal_sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    pal_sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    pal_sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    pal_sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    palette_sampler = SDL_CreateGPUSampler(device, &pal_sampler_info);

    memset(s_palette_uploaded, 0, sizeof(s_palette_uploaded));

    // Upload default identity palette to row DEFAULT_PALETTE_ROW (1088).
    // This is a 256-entry grayscale ramp: index i → (i, i, i, i==0 ? 0 : 255).
    // Used as fallback for indexed textures with no palette assigned.
    {
        u32* pal_ptr = (u32*)SDL_MapGPUTransferBuffer(device, s_palette_transfer, false);
        if (pal_ptr) {
            pal_ptr[0] = 0x00000000; // Index 0 is transparent black
            for (int i = 1; i < PALETTE_TEX_WIDTH; i++) {
                pal_ptr[i] = 0xFF000000u | ((u32)i << 16) | ((u32)i << 8) | (u32)i;
            }
            SDL_UnmapGPUTransferBuffer(device, s_palette_transfer);

            SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);
            SDL_GPUTextureTransferInfo src = {
                .transfer_buffer = s_palette_transfer,
                .offset = 0,
                .pixels_per_row = PALETTE_TEX_WIDTH,
                .rows_per_layer = 1,
            };
            SDL_GPUTextureRegion dst = {
                .texture = s_palette_texture,
                .y = DEFAULT_PALETTE_ROW,
                .w = PALETTE_TEX_WIDTH,
                .h = 1,
                .d = 1,
            };
            SDL_UploadToGPUTexture(cp, &src, &dst, false);
            SDL_EndGPUCopyPass(cp);
            SDL_SubmitGPUCommandBuffer(cb);
        }
    }

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
    memset(tex_array_layer, -1, sizeof(tex_array_layer));  // 1D: per-texture only

    // ⚡ Opt6: LZ77 Compute Pipeline
    {
        char comp_path[1024];
        const char* bp = SDL_GetBasePath();
        snprintf(comp_path, sizeof(comp_path), "%sshaders/lz77_decode.comp.spv", bp ? bp : "");
        size_t comp_size;
        void* comp_code = LoadShaderCode(comp_path, &comp_size);
        if (comp_code && comp_size > 0) {
            SDL_ShaderCross_SPIRV_Info spirv_info;
            SDL_zero(spirv_info);
            spirv_info.bytecode = (const Uint8*)comp_code;
            spirv_info.bytecode_size = comp_size;
            spirv_info.entrypoint = "main";
            spirv_info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
            SDL_ShaderCross_ComputePipelineMetadata* meta =
                SDL_ShaderCross_ReflectComputeSPIRV(
                    (const Uint8*)comp_code, comp_size, 0);
            if (meta) {
                s_lz77_pipeline = SDL_ShaderCross_CompileComputePipelineFromSPIRV(
                    device, &spirv_info, meta, 0);
                SDL_free(meta);
            }
            SDL_free(comp_code);
        }
        if (s_lz77_pipeline) {
            SDL_GPUBufferCreateInfo input_info;
            SDL_zero(input_info);
            input_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
            input_info.size = LZ77_INPUT_SIZE;
            s_lz77_input_buffer = SDL_CreateGPUBuffer(device, &input_info);

            SDL_GPUBufferCreateInfo swiz_info;
            SDL_zero(swiz_info);
            swiz_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
            swiz_info.size = LZ77_SWIZZLE_SIZE;
            s_lz77_swizzle_buffer = SDL_CreateGPUBuffer(device, &swiz_info);

            SDL_GPUTransferBufferCreateInfo upload_info;
            SDL_zero(upload_info);
            upload_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            upload_info.size = LZ77_INPUT_SIZE;
            s_lz77_upload_buf = SDL_CreateGPUTransferBuffer(device, &upload_info);

            s_lz77_available = (s_lz77_input_buffer && s_lz77_swizzle_buffer && s_lz77_upload_buf);
            if (s_lz77_available) {
                SDL_Log("LZ77 GPU compute pipeline: ready");
            }
        }
    }

    SDL_Log("SDLGameRendererGPU_Init: Complete.");
}

/** @brief Shutdown the SDL_GPU renderer and release all resources. */
void SDLGameRendererGPU_Shutdown(void) {
    // ⚡ Opt9: Release any outstanding fences before tearing down resources
    for (int i = 0; i < GPU_FENCE_RING_SIZE; i++) {
        if (s_frame_fences[i]) {
            SDL_WaitForGPUFences(device, true, &s_frame_fences[i], 1);
            SDL_ReleaseGPUFence(device, s_frame_fences[i]);
            s_frame_fences[i] = NULL;
        }
    }
    if (pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
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
    if (s_palette_texture)
        SDL_ReleaseGPUTexture(device, s_palette_texture);
    if (s_palette_transfer)
        SDL_ReleaseGPUTransferBuffer(device, s_palette_transfer);
    if (palette_sampler)
        SDL_ReleaseGPUSampler(device, palette_sampler);
    if (texture_array)
        SDL_ReleaseGPUTexture(device, texture_array);
    if (s_canvas_texture)
        SDL_ReleaseGPUTexture(device, s_canvas_texture);
    if (sampler)
        SDL_ReleaseGPUSampler(device, sampler);
    // ⚡ Opt6: LZ77 compute resources
    if (s_lz77_pipeline)
        SDL_ReleaseGPUComputePipeline(device, s_lz77_pipeline);
    if (s_lz77_input_buffer)
        SDL_ReleaseGPUBuffer(device, s_lz77_input_buffer);
    if (s_lz77_swizzle_buffer)
        SDL_ReleaseGPUBuffer(device, s_lz77_swizzle_buffer);
    if (s_lz77_upload_buf)
        SDL_ReleaseGPUTransferBuffer(device, s_lz77_upload_buf);
    SDL_ShaderCross_Quit();
}

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

    // ⚡ Opt6: Reset LZ77 job queue — buffer mapped lazily on first job
    s_lz77_job_count = 0;
    s_lz77_upload_offset = 0;
    s_lz77_upload_ptr = NULL;  // ⚡ Deferred: mapped on first LZ77 submission

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

    // Stage dirty palette data into the main staging buffer BEFORE unmapping.
    // This avoids the per-row transfer buffer cycling issue where copy commands
    // would all resolve to the last-written buffer.
    // ⚡ Opt10b: Lazily map the staging buffer only when dirty palettes exist.
    if (s_compute_staging_ptr || true) {
        if (!s_compute_staging_ptr) {
            s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
            s_compute_staging_offset = 0;
        }
        for (int i = 0; i < FL_PALETTE_MAX && i < PALETTE_TEX_HEIGHT; i++) {
            if (s_palette_uploaded[i] || !palettes[i])
                continue;
            size_t pal_size = (size_t)PALETTE_TEX_WIDTH * 4; // 256 colors × 4 bytes = 1024
            if (s_pal_upload_count >= MAX_COMPUTE_JOBS ||
                s_compute_staging_offset + pal_size > COMPUTE_STORAGE_SIZE)
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
        }
        s_pal_upload_dirty_count = 0;
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

        // ⚡ Opt6: Upload compressed tile data for LZ77 compute
        if (s_lz77_available && s_lz77_job_count > 0 && s_lz77_upload_offset > 0) {
            SDL_UnmapGPUTransferBuffer(device, s_lz77_upload_buf);
            s_lz77_upload_ptr = NULL;

            SDL_GPUTransferBufferLocation lz_src = { .transfer_buffer = s_lz77_upload_buf, .offset = 0 };
            SDL_GPUBufferRegion lz_dst = { .buffer = s_lz77_input_buffer, .offset = 0,
                                            .size = (Uint32)s_lz77_upload_offset };
            SDL_UploadToGPUBuffer(copy_pass, &lz_src, &lz_dst, false);

            // One-time upload of dctex_linear swizzle LUT
            if (!s_lz77_swizzle_uploaded) {
                extern s16* dctex_linear;
                SDL_GPUTransferBufferCreateInfo swiz_tb_info = {
                    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                    .size = LZ77_SWIZZLE_SIZE
                };
                SDL_GPUTransferBuffer* swiz_tb = SDL_CreateGPUTransferBuffer(device, &swiz_tb_info);
                if (swiz_tb) {
                    void* p = SDL_MapGPUTransferBuffer(device, swiz_tb, false);
                    if (p) {
                        // Widen s16 entries to u32 for the GPU shader's uint swizzle[1024]
                        u32* dst = (u32*)p;
                        for (int i = 0; i < 1024; i++)
                            dst[i] = (u32)dctex_linear[i];
                        SDL_UnmapGPUTransferBuffer(device, swiz_tb);
                        SDL_GPUTransferBufferLocation ssrc = { .transfer_buffer = swiz_tb, .offset = 0 };
                        SDL_GPUBufferRegion sdst = { .buffer = s_lz77_swizzle_buffer, .offset = 0,
                                                      .size = LZ77_SWIZZLE_SIZE };
                        SDL_UploadToGPUBuffer(copy_pass, &ssrc, &sdst, false);
                        s_lz77_swizzle_uploaded = true;
                    }
                    SDL_ReleaseGPUTransferBuffer(device, swiz_tb);
                }
            }
        } else if (s_lz77_upload_ptr) {
            SDL_UnmapGPUTransferBuffer(device, s_lz77_upload_buf);
            s_lz77_upload_ptr = NULL;
        }

        SDL_EndGPUCopyPass(copy_pass);
    }

    // --- 1.5. Compute Pass (⚡ Opt6: LZ77 decode) ---
    if (s_lz77_available && s_lz77_job_count > 0) {
        // Filter out stale jobs (if the texture was destroyed/layer freed after enqueue)
        int valid_jobs = 0;
        for (int i = 0; i < s_lz77_job_count; i++) {
            LZ77Job* job = &s_lz77_uniforms.jobs[i];
            if (tex_array_layer[job->texture_index] == (int)job->dst_layer) {
                s_lz77_uniforms.jobs[valid_jobs++] = *job;
            }
        }
        s_lz77_job_count = valid_jobs;

        if (s_lz77_job_count > 0) {
            s_lz77_uniforms.job_count = s_lz77_job_count;

            SDL_GPUComputePass* comp = SDL_BeginGPUComputePass(
                current_cmd_buf,
                &(SDL_GPUStorageTextureReadWriteBinding){ .texture = texture_array },
                1, NULL, 0);
            if (comp) {
                SDL_BindGPUComputePipeline(comp, s_lz77_pipeline);
                SDL_GPUBuffer* lz77_ro_bufs[2] = { s_lz77_input_buffer, s_lz77_swizzle_buffer };
                SDL_BindGPUComputeStorageBuffers(comp, 0, lz77_ro_bufs, 2);
                SDL_PushGPUComputeUniformData(current_cmd_buf, 0,
                    &s_lz77_uniforms, sizeof(s_lz77_uniforms));
                SDL_DispatchGPUCompute(comp, s_lz77_job_count, 1, 1);
                SDL_EndGPUComputePass(comp);
            }
        }
    }

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
    case SCE_GS_PSMCT32:
        pixel_format = SDL_PIXELFORMAT_ABGR8888;
        pitch = fl_texture->width * 4;
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
                read_rgba32_color(rgba32[ps2_clut_shuffle[i]], &colors[i]);
                colors[i].a = (colors[i].a == 0x80) ? 0xFF : (colors[i].a << 1);
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba16_color(rgba16[ps2_clut_shuffle[i]], &colors[i]);
        }
        colors[0].a = 0;
        break;
    }

    if (palettes[palette_index])
        SDL_DestroyPalette(palettes[palette_index]);
    palettes[palette_index] = SDL_CreatePalette(color_count);
    SDL_SetPaletteColors(palettes[palette_index], colors, 0, color_count);

    // ⚡ Fix: Ensure naturally created palettes (like static PPL UI fonts) are actually
    // queued for upload to the GPU atlas!
    s_palette_uploaded[palette_index] = false;

    bool already_queued = false;
    for (int d = 0; d < s_pal_upload_dirty_count; d++) {
        if (s_pal_upload_dirty_indices[d] == palette_index) {
            already_queued = true;
            break;
        }
    }
    if (!already_queued && s_pal_upload_dirty_count < FL_PALETTE_MAX) {
        s_pal_upload_dirty_indices[s_pal_upload_dirty_count++] = palette_index;
    }
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

void SDLGameRendererGPU_DumpTextures(void) {
    SDL_CreateDirectory("textures");
    int tex_index = 0;
    int count = 0;

    // tex_array_layer[ti] >= 0 means this texture is in the array
    for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
        SDL_Surface* surf = surfaces[ti];
        if (!surf || !SDL_ISPIXELFORMAT_INDEXED(surf->format))
            continue;
        if (tex_array_layer[ti] < 0)
            continue;

        for (int pi = 1; pi <= FL_PALETTE_MAX; pi++) {
            SDL_Palette* pal = palettes[pi - 1];
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
            header[12] = w & 0xFF; header[13] = (w >> 8) & 0xFF;
            header[14] = h & 0xFF; header[15] = (h >> 8) & 0xFF;
            header[16] = 32; header[17] = 0x20;
            fwrite(header, 1, 18, f);

            for (int i = 0; i < w * h; i++) {
                Uint8 idx;
                if (pal->ncolors == 16) {
                    Uint8 byte = pixels[i / 2];
                    idx = (i & 1) ? (byte >> 4) : (byte & 0x0F);
                } else {
                    idx = pixels[i];
                }
                // GPU read_rgba32_color: R→.r, G→.g, B→.b (correct SDL convention)
                // TGA stores BGRA in memory, so write { b, g, r, a }
                const SDL_Color* c = &pal->colors[idx];
                Uint8 bgra[] = { c->b, c->g, c->r, c->a };
                fwrite(bgra, 1, 4, f);
            }
            fclose(f);
            count++;
        }
    }

    SDL_Log("[TextureDump] Wrote %d texture(s) to textures/", count);
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
        // 1D: free the single layer for this texture
        if (tex_array_layer[idx] >= 0) {
            tex_array_free[tex_array_free_count++] = tex_array_layer[idx];
            tex_array_layer[idx] = -1;
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
        // Palette changes don't invalidate indexed texture array layers —
        // the palette row is looked up separately by the fragment shader.
        if (palettes[idx]) {
            SDL_DestroyPalette(palettes[idx]);
            palettes[idx] = NULL;
        }
        SDLGameRendererGPU_CreatePalette((idx + 1) << 16);
        s_palette_uploaded[idx] = false; // ⚡ Mark for re-upload to palette atlas

        bool already_queued = false;
        for (int d = 0; d < s_pal_upload_dirty_count; d++) {
            if (s_pal_upload_dirty_indices[d] == idx) {
                already_queued = true;
                break;
            }
        }
        if (!already_queued && s_pal_upload_dirty_count < FL_PALETTE_MAX) {
            s_pal_upload_dirty_indices[s_pal_upload_dirty_count++] = idx; // ⚡ Opt9b
        }
    }
}


/** @brief Prepare a texture for rendering, uploading to the GPU array if needed. */
void SDLGameRendererGPU_SetTexture(unsigned int th) {
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;

    // ⚡ Back-to-back early-out: if the same handle was just set, re-push
    // the cached layer/UV without re-doing the lookup or staging work.
    // NOTE: Trace zone intentionally placed BELOW this check — the early-out
    // path fires ~80% of the time, and the trace overhead was ~1.25s/session.
    if (th == s_last_set_texture_handle && texture_count > 0) {
        if (texture_count < MAX_VERTICES) {
            texture_layers[texture_count] = texture_layers[texture_count - 1];
            texture_uv_sx[texture_count] = texture_uv_sx[texture_count - 1];
            texture_uv_sy[texture_count] = texture_uv_sy[texture_count - 1];
            texture_palette_idx[texture_count] = texture_palette_idx[texture_count - 1];
            texture_count++;
        }
        return;
    }
    s_last_set_texture_handle = th;
    TRACE_ZONE_N("GPU:SetTexture");

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

    int layer = tex_array_layer[texture_handle - 1];  // 1D: keyed by texture only

    if (layer < 0) {
        // ⚡ Opt10b: Lazily map the staging buffer on first texture cache miss this frame.
        if (tex_array_free_count > 0 && !s_compute_staging_ptr) {
            s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
            s_compute_staging_offset = 0;
        }
        if (tex_array_free_count > 0 && s_compute_staging_ptr) {
            layer = tex_array_free[--tex_array_free_count];
            tex_array_layer[texture_handle - 1] = layer;  // 1D

            SDL_Surface* surface = surfaces[texture_handle - 1];
            const FLTexture* fl_texture = &flTexture[texture_handle - 1];

            // Upload as RGBA8 (4 bytes/pixel) to staging buffer
            int w = surface->w;
            int h = surface->h;
            size_t rgba_size = (size_t)w * h * 4;

            if (s_tex_upload_count < MAX_COMPUTE_JOBS &&
                s_compute_staging_offset + rgba_size <= COMPUTE_STORAGE_SIZE) {

                Uint32 out_offset = (Uint32)s_compute_staging_offset;
                u32* dst = (u32*)(s_compute_staging_ptr + s_compute_staging_offset);

                if (fl_texture->format == SCE_GS_PSMT4) {
                    // ⚡ Bolt: SIMD 4-bit indexed → RGBA32 (palette index in R channel)
                    // Process 16 input bytes (32 pixels) at a time via nibble unpack.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i alpha = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero  = simde_mm_setzero_si128();
                    const simde__m128i mask  = simde_mm_set1_epi8(0x0F);
                    for (int y = 0; y < h; y++) {
                        const u8* row = src + y * pitch;
                        u32* out_row = dst + y * w;
                        int x = 0;
                        for (; x + 31 < w; x += 32) {
                            // Load 16 bytes (32 pixels, each 4 bits)
                            simde__m128i bytes = simde_mm_loadu_si128((const simde__m128i*)(row + x / 2));
                            // Extract low nibbles line
                            simde__m128i lo_nibbles = simde_mm_and_si128(bytes, mask);
                            // Extract high nibbles line
                            simde__m128i hi_nibbles = simde_mm_and_si128(simde_mm_srli_epi16(bytes, 4), mask);
                            // Interleave lo and hi so we get [lo, hi, lo, hi...]
                            // Example: byte0=(h0<<4)|l0 -> we want l0, h0.
                            // unpacklo/hi_epi8 interleaves byte-by-byte.
                            simde__m128i interleaved_lo = simde_mm_unpacklo_epi8(lo_nibbles, hi_nibbles);
                            simde__m128i interleaved_hi = simde_mm_unpackhi_epi8(lo_nibbles, hi_nibbles);
                            // Now interleaved_lo has the first 16 pixels (as 8-bit values),
                            // interleaved_hi has the next 16 pixels.
                            // Expand to 32-bit and OR with 0xFF000000
                            // 1) First 8 pixels from interleaved_lo
                            simde__m128i a0 = simde_mm_unpacklo_epi8(interleaved_lo, zero);
                            simde__m128i a1 = simde_mm_unpackhi_epi8(interleaved_lo, zero);
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  0), simde_mm_or_si128(simde_mm_unpacklo_epi16(a0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  4), simde_mm_or_si128(simde_mm_unpackhi_epi16(a0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  8), simde_mm_or_si128(simde_mm_unpacklo_epi16(a1, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 12), simde_mm_or_si128(simde_mm_unpackhi_epi16(a1, zero), alpha));
                            // 2) Next 8 pixels from interleaved_hi
                            simde__m128i a2 = simde_mm_unpacklo_epi8(interleaved_hi, zero);
                            simde__m128i a3 = simde_mm_unpackhi_epi8(interleaved_hi, zero);
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 16), simde_mm_or_si128(simde_mm_unpacklo_epi16(a2, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 20), simde_mm_or_si128(simde_mm_unpackhi_epi16(a2, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 24), simde_mm_or_si128(simde_mm_unpacklo_epi16(a3, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 28), simde_mm_or_si128(simde_mm_unpackhi_epi16(a3, zero), alpha));
                        }
                        // Scalar tail
                        for (; x < w; x += 2) {
                            u8 byte = row[x / 2];
                            out_row[x]     = 0xFF000000u | (u32)(byte & 0x0F);
                            if (x + 1 < w)
                                out_row[x + 1] = 0xFF000000u | (u32)((byte >> 4) & 0x0F);
                        }
                    }
                } else if (fl_texture->format == SCE_GS_PSMCT16) {
                    // ⚡ Bolt: SIMD 16-bit direct color → RGBA32
                    // Process 8 pixels at a time using integer SIMD for bit extraction.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i mask5  = simde_mm_set1_epi32(0x1F);
                    const simde__m128i mask_a = simde_mm_set1_epi32(0x8000);
                    const simde__m128i alpha_ff = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero = simde_mm_setzero_si128();
                    for (int y = 0; y < h; y++) {
                        const u16* row = (const u16*)(src + y * pitch);
                        u32* out_row = dst + y * w;
                        int x = 0;
                        for (; x + 7 < w; x += 8) {
                            // Load 8 u16 pixels and expand to 2×4 u32
                            simde__m128i px = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                            simde__m128i lo32 = simde_mm_unpacklo_epi16(px, zero);
                            simde__m128i hi32 = simde_mm_unpackhi_epi16(px, zero);
                            // Process lo32 (4 pixels)
                            for (int half = 0; half < 2; half++) {
                                simde__m128i v = (half == 0) ? lo32 : hi32;
                                simde__m128i r = simde_mm_and_si128(v, mask5);                                 // R: bits 0-4
                                simde__m128i g = simde_mm_and_si128(simde_mm_srli_epi32(v, 5), mask5);         // G: bits 5-9
                                simde__m128i b = simde_mm_and_si128(simde_mm_srli_epi32(v, 10), mask5);        // B: bits 10-14
                                simde__m128i a = simde_mm_and_si128(v, mask_a);                                 // A: bit 15
                                // Scale 5-bit to 8-bit: val * 255 / 31 ≈ (val * 8 + val / 4) = (val << 3) | (val >> 2)
                                r = simde_mm_or_si128(simde_mm_slli_epi32(r, 3), simde_mm_srli_epi32(r, 2));
                                g = simde_mm_or_si128(simde_mm_slli_epi32(g, 3), simde_mm_srli_epi32(g, 2));
                                b = simde_mm_or_si128(simde_mm_slli_epi32(b, 3), simde_mm_srli_epi32(b, 2));
                                // Alpha: 0x8000 → 0xFF000000, 0 → 0
                                a = simde_mm_and_si128(simde_mm_cmpeq_epi32(a, mask_a), alpha_ff);
                                // Pack: a | (b << 16) | (g << 8) | r
                                simde__m128i result = simde_mm_or_si128(a, r);
                                result = simde_mm_or_si128(result, simde_mm_slli_epi32(g, 8));
                                result = simde_mm_or_si128(result, simde_mm_slli_epi32(b, 16));
                                simde_mm_storeu_si128((simde__m128i*)(out_row + x + half * 4), result);
                            }
                        }
                        // Scalar tail
                        for (; x < w; x++) {
                            SDL_Color c;
                            read_rgba16_color(row[x], &c);
                            out_row[x] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r;
                        }
                    }
                } else if (fl_texture->format == SCE_GS_PSMT8) {
                    // ⚡ Bolt: SIMD 8-bit indexed → RGBA32 (palette index in R channel)
                    // Process 16 pixels at a time: load 16 u8 → expand to 4×__m128i of u32 → OR with 0xFF000000.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i alpha = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero  = simde_mm_setzero_si128();
                    for (int y = 0; y < h; y++) {
                        const u8* row = src + y * pitch;
                        u32* out_row = dst + y * w;
                        int x = 0;
                        // Process 16 bytes (pixels) at a time
                        for (; x + 15 < w; x += 16) {
                            simde__m128i bytes = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                            // Expand bytes → u16 (2 groups)
                            simde__m128i w0 = simde_mm_unpacklo_epi8(bytes, zero);
                            simde__m128i w1 = simde_mm_unpackhi_epi8(bytes, zero);
                            // Expand u16 → u32 (4 groups) and OR with alpha mask
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  0), simde_mm_or_si128(simde_mm_unpacklo_epi16(w0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  4), simde_mm_or_si128(simde_mm_unpackhi_epi16(w0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x +  8), simde_mm_or_si128(simde_mm_unpacklo_epi16(w1, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 12), simde_mm_or_si128(simde_mm_unpackhi_epi16(w1, zero), alpha));
                        }
                        // Scalar tail
                        for (; x < w; x++) {
                            out_row[x] = 0xFF000000u | (u32)row[x];
                        }
                    }
                } else {
                    // 32-bit direct (no palette, no conversion needed)
                    memcpy(dst, surface->pixels, rgba_size);
                }

                s_compute_staging_offset += rgba_size;
                // ⚡ Vulkan/SDL_GPU requires copy offsets to be highly aligned (typically 256/512 bytes)
                s_compute_staging_offset = (s_compute_staging_offset + 511) & ~511;

                TextureUploadJob* job = &s_tex_upload_jobs[s_tex_upload_count++];
                job->width = w;
                job->height = h;
                job->layer = layer;
                job->offset = out_offset;
            } else {
                s_compute_drops_last_frame++;
                tex_array_free[tex_array_free_count++] = layer;
                tex_array_layer[texture_handle - 1] = -1;  // 1D
                layer = -1;
            }
        }
    }

    if (texture_count >= MAX_VERTICES) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Texture stack overflow!");
        return;
    }

    if (layer < 0)
        layer = 0;

    // Determine palette index for the palette atlas.
    // Only enable palette lookup for INDEXED textures (PSMT4/PSMT8) with a valid palette.
    // Direct-color textures (PSMCT16, 32-bit) always get palIdx = -1 even if palette_handle != 0.
    const FLTexture* fl_tex_info = &flTexture[texture_handle - 1];
    bool is_indexed_format = (fl_tex_info->format == SCE_GS_PSMT4 || fl_tex_info->format == SCE_GS_PSMT8);
    float palIdx = -1.0f;
    if (is_indexed_format) {
        // If no palette assigned, fall back to the identity grayscale ramp at DEFAULT_PALETTE_ROW.
        // Using row 0 samples an uninitialized/wrong palette for native UI (main menu, HUD, etc.).
        palIdx = (palette_handle > 0) ? (float)(palette_handle - 1) : (float)DEFAULT_PALETTE_ROW;
    }

    texture_layers[texture_count] = layer;
    // ⚡ Opt10c: Use fl_tex_info (already loaded) instead of pointer-chasing through surfaces[].
    // Multiply by reciprocal constant instead of dividing by TEX_ARRAY_SIZE.
    texture_uv_sx[texture_count] = (float)fl_tex_info->width * (1.0f / TEX_ARRAY_SIZE);
    texture_uv_sy[texture_count] = (float)fl_tex_info->height * (1.0f / TEX_ARRAY_SIZE);
    texture_palette_idx[texture_count] = palIdx;
    texture_count++;
    TRACE_ZONE_END();
}

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (!mapped_vertex_ptr || vertex_count + 4 > MAX_VERTICES)
        return;

    float layer = -1.0f;  // Bug 2 fix: negative sentinel → shader uses FgColor for solid quads
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
        const simde__m128i ci = simde_mm_set_epi32(
            (c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
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
        v[0].x = x0; v[0].y = y0; v[0].r = cr; v[0].g = cg; v[0].b = cb; v[0].a = ca;
        v[0].u = s0; v[0].v = t0; v[0].layer = layer; v[0].paletteIdx = palIdx;
        v[1].x = x1; v[1].y = y0; v[1].r = cr; v[1].g = cg; v[1].b = cb; v[1].a = ca;
        v[1].u = s1; v[1].v = t0; v[1].layer = layer; v[1].paletteIdx = palIdx;
        v[2].x = x0; v[2].y = y1; v[2].r = cr; v[2].g = cg; v[2].b = cb; v[2].a = ca;
        v[2].u = s0; v[2].v = t1; v[2].layer = layer; v[2].paletteIdx = palIdx;
        v[3].x = x1; v[3].y = y1; v[3].r = cr; v[3].g = cg; v[3].b = cb; v[3].a = ca;
        v[3].u = s1; v[3].v = t1; v[3].layer = layer; v[3].paletteIdx = palIdx;

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

// ⚡ Opt6: LZ77 GPU compute API
int SDLGameRendererGPU_LZ77Available(void) {
    return s_lz77_available ? 1 : 0;
}

int SDLGameRendererGPU_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size,
                                    int texture_handle, int palette_handle,
                                    u32 code, u32 tile_dim) {
    if (!s_lz77_available)
        return 0;

    // ⚡ Lazy map: only map the upload buffer on first LZ77 job this frame
    if (!s_lz77_upload_ptr) {
        s_lz77_upload_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_lz77_upload_buf, true);
        if (!s_lz77_upload_ptr)
            return 0;
    }

    if (s_lz77_job_count >= LZ77_MAX_JOBS)
        return 0;

    // Check staging space (align to 4 bytes for uint access in shader)
    size_t aligned_size = (comp_size + 3u) & ~3u;
    if (s_lz77_upload_offset + aligned_size > LZ77_INPUT_SIZE)
        return 0;

    // Pre-allocate a texture array layer for this texture+palette combo
    int ti = texture_handle - 1;
    if (ti < 0 || ti >= FL_TEXTURE_MAX)
        return 0;
    if (palette_handle < 0 || palette_handle > FL_PALETTE_MAX)
        return 0;

    int layer = tex_array_layer[ti];  // 1D: keyed by texture only
    if (layer < 0) {
        if (tex_array_free_count <= 0)
            return 0;
        layer = tex_array_free[--tex_array_free_count];
        tex_array_layer[ti] = layer;  // 1D
    }

    // Compute destination pixel coordinates from tile code
    u32 dst_x, dst_y;
    if (tile_dim <= 16) {
        dst_x = (code & 0x0Fu) * 16u;
        dst_y = ((code >> 4u) & 0x0Fu) * 16u;
    } else {
        dst_x = (code & 7u) * 32u;
        dst_y = ((code >> 3u) & 7u) * 32u;
    }

    // Copy compressed data to staging buffer
    memcpy(s_lz77_upload_ptr + s_lz77_upload_offset, compressed, comp_size);

    // Record job descriptor
    LZ77Job* job = &s_lz77_uniforms.jobs[s_lz77_job_count++];
    job->src_offset = (Uint32)s_lz77_upload_offset;
    job->src_size = comp_size;
    job->dst_size = decomp_size;
    job->dst_layer = layer;
    job->dst_x = dst_x;
    job->dst_y = dst_y;
    job->tile_dim = tile_dim;
    job->texture_index = ti;

    s_lz77_upload_offset += aligned_size;
    return 1;
}
