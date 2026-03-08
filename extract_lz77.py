import sys

with open("src/port/sdl/renderer/sdl_game_renderer_gpu.c", "r", encoding="utf-8") as f:
    orig = f.read()

# I will just write the manual replacement steps correctly.

out = orig
# 1. Variables
lz77_vars = """// ⚡ Opt6: LZ77 GPU compute decompression
#define LZ77_MAX_JOBS 64
#define LZ77_INPUT_SIZE (512 * 1024) // 512KB compressed data staging
#define LZ77_SWIZZLE_SIZE (1024 * 4) // 1024 uint entries

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
static bool s_lz77_swizzle_uploaded = false;"""

if lz77_vars in out:
    out = out.replace(
        lz77_vars, "// LZ77 context has been extracted to sdl_game_renderer_gpu_lz77.c"
    )
else:
    print("Failed to find variables")
    sys.exit(1)

# 2. Init
lz77_init = """    // ⚡ Opt6: LZ77 Compute Pipeline
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
                SDL_ShaderCross_ReflectComputeSPIRV((const Uint8*)comp_code, comp_size, 0);
            if (meta) {
                s_lz77_pipeline = SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &spirv_info, meta, 0);
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
    }"""

if lz77_init in out:
    out = out.replace(
        lz77_init, "    s_lz77_ctx = LZ77_Create(device, LoadShaderCode);"
    )
else:
    print("Failed to find init")
    sys.exit(1)

# 3. Shutdown
lz77_shutdown = """    // ⚡ Opt6: LZ77 compute resources
    if (s_lz77_pipeline)
        SDL_ReleaseGPUComputePipeline(device, s_lz77_pipeline);
    if (s_lz77_input_buffer)
        SDL_ReleaseGPUBuffer(device, s_lz77_input_buffer);
    if (s_lz77_swizzle_buffer)
        SDL_ReleaseGPUBuffer(device, s_lz77_swizzle_buffer);
    if (s_lz77_upload_buf)
        SDL_ReleaseGPUTransferBuffer(device, s_lz77_upload_buf);"""

if lz77_shutdown in out:
    out = out.replace(
        lz77_shutdown, "    if (s_lz77_ctx)\n        LZ77_Destroy(s_lz77_ctx, device);"
    )
else:
    print("Failed to find shutdown")
    sys.exit(1)

# 4. BeginFrame
lz77_begin_frame = """    // ⚡ Opt6: Reset LZ77 job queue — buffer mapped lazily on first job
    s_lz77_job_count = 0;
    s_lz77_upload_offset = 0;
    s_lz77_upload_ptr = NULL; // ⚡ Deferred: mapped on first LZ77 submission"""

if lz77_begin_frame in out:
    out = out.replace(lz77_begin_frame, "    LZ77_BeginFrame(s_lz77_ctx);")
else:
    print("Failed to find BeginFrame")
    sys.exit(1)

# 5. RenderFrame uploads
lz77_uploads = """        // ⚡ Opt6: Upload compressed tile data for LZ77 compute
        if (s_lz77_available && s_lz77_job_count > 0 && s_lz77_upload_offset > 0) {
            SDL_UnmapGPUTransferBuffer(device, s_lz77_upload_buf);
            s_lz77_upload_ptr = NULL;

            SDL_GPUTransferBufferLocation lz_src = { .transfer_buffer = s_lz77_upload_buf, .offset = 0 };
            SDL_GPUBufferRegion lz_dst = { .buffer = s_lz77_input_buffer,
                                           .offset = 0,
                                           .size = (Uint32)s_lz77_upload_offset };
            SDL_UploadToGPUBuffer(copy_pass, &lz_src, &lz_dst, false);

            // One-time upload of dctex_linear swizzle LUT
            if (!s_lz77_swizzle_uploaded) {
                extern s16* dctex_linear;
                SDL_GPUTransferBufferCreateInfo swiz_tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                                 .size = LZ77_SWIZZLE_SIZE };
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
                        SDL_GPUBufferRegion sdst = { .buffer = s_lz77_swizzle_buffer,
                                                     .offset = 0,
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
        }"""

if lz77_uploads in out:
    out = out.replace(
        lz77_uploads, "        LZ77_Upload(s_lz77_ctx, device, copy_pass);"
    )
else:
    print("Failed to find uploads")
    sys.exit(1)

# 6. RenderFrame dispatch
lz77_dispatch = """    // --- 1.5. Compute Pass (⚡ Opt6: LZ77 decode) ---
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
                current_cmd_buf, &(SDL_GPUStorageTextureReadWriteBinding) { .texture = texture_array }, 1, NULL, 0);
            if (comp) {
                SDL_BindGPUComputePipeline(comp, s_lz77_pipeline);
                SDL_GPUBuffer* lz77_ro_bufs[2] = { s_lz77_input_buffer, s_lz77_swizzle_buffer };
                SDL_BindGPUComputeStorageBuffers(comp, 0, lz77_ro_bufs, 2);
                SDL_PushGPUComputeUniformData(current_cmd_buf, 0, &s_lz77_uniforms, sizeof(s_lz77_uniforms));
                SDL_DispatchGPUCompute(comp, s_lz77_job_count, 1, 1);
                SDL_EndGPUComputePass(comp);
            }
        }
    }"""

if lz77_dispatch in out:
    # Notice we pass tex_array_layer to filter stale jobs
    out = out.replace(
        lz77_dispatch,
        "    // --- 1.5. Compute Pass (⚡ Opt6: LZ77 decode) ---\n"
        "    LZ77_Dispatch(s_lz77_ctx, current_cmd_buf, texture_array, tex_array_layer);",
    )
else:
    print("Failed to find dispatch")
    sys.exit(1)

# 7. Add include and context variable at top
includes = '#include "port/sdl/renderer/sdl_game_renderer_internal.h"\n'
new_includes = includes + '#include "port/sdl/renderer/sdl_game_renderer_gpu_lz77.h"\n'
out = out.replace(includes, new_includes, 1)

static_context = "static SDL_GPUDevice* device = NULL;\n"
new_static_context = static_context + "static LZ77Context* s_lz77_ctx = NULL;\n"
out = out.replace(static_context, new_static_context, 1)

# 8. API rewrites
lz77_apis = """// ⚡ Opt6: LZ77 GPU compute API
int SDLGameRendererGPU_LZ77Available(void) {
    return s_lz77_available ? 1 : 0;
}

int SDLGameRendererGPU_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size, int texture_handle,
                                   int palette_handle, u32 code, u32 tile_dim) {
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

    int layer = tex_array_layer[ti]; // 1D: keyed by texture only
    if (layer < 0) {
        if (tex_array_free_count <= 0)
            return 0;
        layer = tex_array_free[--tex_array_free_count];
        tex_array_layer[ti] = layer; // 1D
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
}"""

# Replace the APIs with a thinner wrapper
new_lz77_apis = """// ⚡ Opt6: LZ77 GPU compute API
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
}"""

if lz77_apis in out:
    out = out.replace(lz77_apis, new_lz77_apis)
else:
    print("Failed to find apis")
    sys.exit(1)

with open("src/port/sdl/renderer/sdl_game_renderer_gpu.c", "w", encoding="utf-8") as f:
    f.write(out)

print("Extraction script applied successfully.")
