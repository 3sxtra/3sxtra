/**
 * @file sdl_game_renderer_gpu_lz77.h
 * @brief Extracted LZ77 GPU compute pipeline orchestration.
 */

#ifndef SDL_GAME_RENDERER_GPU_LZ77_H
#define SDL_GAME_RENDERER_GPU_LZ77_H

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct LZ77Context LZ77Context;

/** @brief Callback to load a shader file. */
typedef void* (*LoadShaderCodeFunc)(const char* filename, size_t* size);

/** @brief Allocate and initialize the LZ77 compute context. */
LZ77Context* LZ77_Create(SDL_GPUDevice* device, LoadShaderCodeFunc load_shader_cb);

/** @brief Release LZ77 resources and free the context. */
void LZ77_Destroy(LZ77Context* ctx, SDL_GPUDevice* device);

/** @brief Prepare the LZ77 context for a new frame. */
void LZ77_BeginFrame(LZ77Context* ctx);

/** @brief Check if the LZ77 pipeline was successfully initialized. */
int LZ77_IsAvailable(const LZ77Context* ctx);

/** @brief Enqueue a compressed tile for GPU decoding. */
int LZ77_Enqueue(LZ77Context* ctx, SDL_GPUDevice* device, const Uint8* compressed, Uint32 comp_size, Uint32 decomp_size,
                 int texture_index, int layer, Uint32 code, Uint32 tile_dim);

/** @brief Upload staged LZ77 data. Call within a GPU copy pass. */
void LZ77_Upload(LZ77Context* ctx, SDL_GPUDevice* device, SDL_GPUCopyPass* copy_pass);

/** @brief Dispatch the compute shader for all enqueued jobs. */
void LZ77_Dispatch(LZ77Context* ctx, SDL_GPUCommandBuffer* cmd_buf, SDL_GPUTexture* texture_array,
                   const int16_t* tex_array_layer);

#endif // SDL_GAME_RENDERER_GPU_LZ77_H
