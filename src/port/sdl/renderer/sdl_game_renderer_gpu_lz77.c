/**
 * @file sdl_game_renderer_gpu_lz77.c
 * @brief Implementation of the extracted LZ77 GPU compute pipeline.
 */

#include "port/sdl/renderer/sdl_game_renderer_gpu_lz77.h"
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ⚡ Opt6: LZ77 GPU compute decompression
#define LZ77_MAX_JOBS 64
#define LZ77_INPUT_SIZE (512 * 1024) // 512KB compressed data staging
#define LZ77_SWIZZLE_SIZE (1024 * 4) // 1024 uint entries

typedef struct {
    Uint32 src_offset, src_size, dst_size, dst_layer;
    Uint32 dst_x, dst_y, tile_dim, texture_index;
} LZ77Job;

struct LZ77Context {
    SDL_GPUComputePipeline* pipeline;
    SDL_GPUBuffer* input_buffer;
    SDL_GPUBuffer* swizzle_buffer;
    SDL_GPUTransferBuffer* upload_buf;
    Uint8* upload_ptr;
    size_t upload_offset;
    int job_count;
    bool available;
    bool swizzle_uploaded;

    struct {
        Uint32 job_count;
        Uint32 _pad1, _pad2, _pad3;
        LZ77Job jobs[LZ77_MAX_JOBS];
    } uniforms;
};

LZ77Context* LZ77_Create(SDL_GPUDevice* device, LoadShaderCodeFunc load_shader_cb) {
    LZ77Context* ctx = (LZ77Context*)SDL_calloc(1, sizeof(LZ77Context));
    if (!ctx)
        return NULL;

    char comp_path[1024];
    const char* bp = SDL_GetBasePath();
    snprintf(comp_path, sizeof(comp_path), "%sshaders/lz77_decode.comp.spv", bp ? bp : "");
    size_t comp_size_file;
    void* comp_code = load_shader_cb(comp_path, &comp_size_file);
    if (comp_code && comp_size_file > 0) {
        SDL_ShaderCross_SPIRV_Info spirv_info;
        SDL_zero(spirv_info);
        spirv_info.bytecode = (const Uint8*)comp_code;
        spirv_info.bytecode_size = comp_size_file;
        spirv_info.entrypoint = "main";
        spirv_info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
        SDL_ShaderCross_ComputePipelineMetadata* meta =
            SDL_ShaderCross_ReflectComputeSPIRV((const Uint8*)comp_code, comp_size_file, 0);
        if (meta) {
            ctx->pipeline = SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &spirv_info, meta, 0);
            SDL_free(meta);
        }
        SDL_free(comp_code);
    }

    if (ctx->pipeline) {
        SDL_GPUBufferCreateInfo input_info;
        SDL_zero(input_info);
        input_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
        input_info.size = LZ77_INPUT_SIZE;
        ctx->input_buffer = SDL_CreateGPUBuffer(device, &input_info);

        SDL_GPUBufferCreateInfo swiz_info;
        SDL_zero(swiz_info);
        swiz_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
        swiz_info.size = LZ77_SWIZZLE_SIZE;
        ctx->swizzle_buffer = SDL_CreateGPUBuffer(device, &swiz_info);

        SDL_GPUTransferBufferCreateInfo upload_info;
        SDL_zero(upload_info);
        upload_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        upload_info.size = LZ77_INPUT_SIZE;
        ctx->upload_buf = SDL_CreateGPUTransferBuffer(device, &upload_info);

        ctx->available = (ctx->input_buffer && ctx->swizzle_buffer && ctx->upload_buf);
        if (ctx->available) {
            SDL_Log("LZ77 GPU compute pipeline: ready");
        }
    }

    return ctx;
}

void LZ77_Destroy(LZ77Context* ctx, SDL_GPUDevice* device) {
    if (!ctx)
        return;
    if (ctx->pipeline)
        SDL_ReleaseGPUComputePipeline(device, ctx->pipeline);
    if (ctx->input_buffer)
        SDL_ReleaseGPUBuffer(device, ctx->input_buffer);
    if (ctx->swizzle_buffer)
        SDL_ReleaseGPUBuffer(device, ctx->swizzle_buffer);
    if (ctx->upload_buf)
        SDL_ReleaseGPUTransferBuffer(device, ctx->upload_buf);
    SDL_free(ctx);
}

void LZ77_BeginFrame(LZ77Context* ctx) {
    if (!ctx)
        return;
    ctx->job_count = 0;
    ctx->upload_offset = 0;
    ctx->upload_ptr = NULL; // ⚡ Deferred: mapped on first LZ77 submission
}

int LZ77_IsAvailable(const LZ77Context* ctx) {
    return (ctx && ctx->available) ? 1 : 0;
}

int LZ77_Enqueue(LZ77Context* ctx, SDL_GPUDevice* device, const Uint8* compressed, Uint32 comp_size, Uint32 decomp_size,
                 int texture_index, int layer, Uint32 code, Uint32 tile_dim) {
    if (!ctx || !ctx->available)
        return 0;

    // ⚡ Lazy map: only map the upload buffer on first LZ77 job this frame
    if (!ctx->upload_ptr) {
        ctx->upload_ptr = (Uint8*)SDL_MapGPUTransferBuffer(device, ctx->upload_buf, true);
        if (!ctx->upload_ptr)
            return 0;
    }

    if (ctx->job_count >= LZ77_MAX_JOBS)
        return 0;

    // Check staging space (align to 4 bytes for uint access in shader)
    size_t aligned_size = (comp_size + 3u) & ~3u;
    if (ctx->upload_offset + aligned_size > LZ77_INPUT_SIZE)
        return 0;

    // Compute destination pixel coordinates from tile code
    Uint32 dst_x, dst_y;
    if (tile_dim <= 16) {
        dst_x = (code & 0x0Fu) * 16u;
        dst_y = ((code >> 4u) & 0x0Fu) * 16u;
    } else {
        dst_x = (code & 7u) * 32u;
        dst_y = ((code >> 3u) & 7u) * 32u;
    }

    // Copy compressed data to staging buffer
    memcpy(ctx->upload_ptr + ctx->upload_offset, compressed, comp_size);

    // Record job descriptor
    LZ77Job* job = &ctx->uniforms.jobs[ctx->job_count++];
    job->src_offset = (Uint32)ctx->upload_offset;
    job->src_size = comp_size;
    job->dst_size = decomp_size;
    job->dst_layer = (Uint32)layer;
    job->dst_x = dst_x;
    job->dst_y = dst_y;
    job->tile_dim = tile_dim;
    job->texture_index = (Uint32)texture_index;

    ctx->upload_offset += aligned_size;
    return 1;
}

void LZ77_Upload(LZ77Context* ctx, SDL_GPUDevice* device, SDL_GPUCopyPass* copy_pass) {
    if (!ctx)
        return;

    // ⚡ Opt6: Upload compressed tile data for LZ77 compute
    if (ctx->available && ctx->job_count > 0 && ctx->upload_offset > 0) {
        SDL_UnmapGPUTransferBuffer(device, ctx->upload_buf);
        ctx->upload_ptr = NULL;

        SDL_GPUTransferBufferLocation lz_src = { .transfer_buffer = ctx->upload_buf, .offset = 0 };
        SDL_GPUBufferRegion lz_dst = { .buffer = ctx->input_buffer, .offset = 0, .size = (Uint32)ctx->upload_offset };
        SDL_UploadToGPUBuffer(copy_pass, &lz_src, &lz_dst, false);

        // One-time upload of dctex_linear swizzle LUT
        if (!ctx->swizzle_uploaded) {
            extern Sint16* dctex_linear;
            SDL_GPUTransferBufferCreateInfo swiz_tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                             .size = LZ77_SWIZZLE_SIZE };
            SDL_GPUTransferBuffer* swiz_tb = SDL_CreateGPUTransferBuffer(device, &swiz_tb_info);
            if (swiz_tb) {
                void* p = SDL_MapGPUTransferBuffer(device, swiz_tb, false);
                if (p) {
                    // Widen s16 entries to u32 for the GPU shader's uint swizzle[1024]
                    Uint32* dst = (Uint32*)p;
                    for (int i = 0; i < 1024; i++)
                        dst[i] = (Uint32)dctex_linear[i];
                    SDL_UnmapGPUTransferBuffer(device, swiz_tb);
                    SDL_GPUTransferBufferLocation ssrc = { .transfer_buffer = swiz_tb, .offset = 0 };
                    SDL_GPUBufferRegion sdst = { .buffer = ctx->swizzle_buffer,
                                                 .offset = 0,
                                                 .size = LZ77_SWIZZLE_SIZE };
                    SDL_UploadToGPUBuffer(copy_pass, &ssrc, &sdst, false);
                    ctx->swizzle_uploaded = true;
                }
                SDL_ReleaseGPUTransferBuffer(device, swiz_tb);
            }
        }
    } else if (ctx->upload_ptr) {
        SDL_UnmapGPUTransferBuffer(device, ctx->upload_buf);
        ctx->upload_ptr = NULL;
    }
}

void LZ77_Dispatch(LZ77Context* ctx, SDL_GPUCommandBuffer* cmd_buf, SDL_GPUTexture* texture_array,
                   const int16_t* tex_array_layer) {
    if (!ctx || !ctx->available || ctx->job_count <= 0)
        return;

    // Filter out stale jobs (if the texture was destroyed/layer freed after enqueue)
    int valid_jobs = 0;
    for (int i = 0; i < ctx->job_count; i++) {
        LZ77Job* job = &ctx->uniforms.jobs[i];
        if (tex_array_layer[job->texture_index] == (int)job->dst_layer) {
            ctx->uniforms.jobs[valid_jobs++] = *job;
        }
    }
    ctx->job_count = valid_jobs;

    if (ctx->job_count > 0) {
        ctx->uniforms.job_count = (Uint32)ctx->job_count;

        SDL_GPUComputePass* comp = SDL_BeginGPUComputePass(
            cmd_buf, &(SDL_GPUStorageTextureReadWriteBinding) { .texture = texture_array }, 1, NULL, 0);
        if (comp) {
            SDL_BindGPUComputePipeline(comp, ctx->pipeline);
            SDL_GPUBuffer* lz77_ro_bufs[2] = { ctx->input_buffer, ctx->swizzle_buffer };
            SDL_BindGPUComputeStorageBuffers(comp, 0, lz77_ro_bufs, 2);
            SDL_PushGPUComputeUniformData(cmd_buf, 0, &ctx->uniforms, sizeof(ctx->uniforms));
            SDL_DispatchGPUCompute(comp, ctx->job_count, 1, 1);
            SDL_EndGPUComputePass(comp);
        }
    }
}
