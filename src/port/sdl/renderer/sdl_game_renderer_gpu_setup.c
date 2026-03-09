/**
 * @file sdl_game_renderer_gpu_setup.c
 * @brief GPU renderer initialization and shutdown.
 *
 * Contains LoadShaderCode, CreateGPUShader, SDLGameRendererGPU_Init, and
 * SDLGameRendererGPU_Shutdown — extracted from sdl_game_renderer_gpu.c to
 * reduce file size. All shared state is accessed via the internal header.
 */
#include "sdl_game_renderer_gpu_internal.h"

#include "port/sdl/app/sdl_app.h"
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stdio.h>

/** @brief Load a shader file from disk, returning the raw bytes. */
static void* LoadShaderCode(const char* filename, size_t* size) {
    void* code = SDL_LoadFile(filename, size);
    if (!code) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Failed to load shader: %s", filename);
    }
    return code;
}

/** @brief Compile a SPIR-V shader into a GPU shader via SDL_ShaderCross. */
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
    gpu_window = SDLApp_GetWindow();

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
    memset(tex_array_layer, -1, sizeof(tex_array_layer)); // 1D: per-texture only

    s_lz77_ctx = LZ77_Create(device, LoadShaderCode);

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
    if (s_lz77_ctx)
        LZ77_Destroy(s_lz77_ctx, device);
    SDL_ShaderCross_Quit();
}
