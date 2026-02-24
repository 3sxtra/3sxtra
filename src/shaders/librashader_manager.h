#ifndef LIBRASHADER_MANAGER_H
#define LIBRASHADER_MANAGER_H

#include <SDL3/SDL.h>

typedef struct LibrashaderManager LibrashaderManager;

// Initialize the manager with a preset path.
// Returns NULL on failure.
LibrashaderManager* LibrashaderManager_Init(const char* preset_path);

// Render the pipeline (OpenGL).
// input_texture_ptr: Cast to GLuint for OpenGL.
void LibrashaderManager_Render(LibrashaderManager* manager, void* input_texture_ptr, int input_w, int input_h,
                               int viewport_x, int viewport_y, int viewport_w, int viewport_h);

// Render the pipeline (SDL_GPU).
// Two-stage approach matching GL backend:
//   1. Renders librashader to intermediate_texture at {0,0}
//   2. Blits the result to swapchain_texture at the centered letterbox position
void LibrashaderManager_Render_GPU_Wrapper(LibrashaderManager* manager, void* command_buffer, void* input_texture,
                                           void* intermediate_texture, void* swapchain_texture, int input_w,
                                           int input_h, int viewport_w, int viewport_h, int swapchain_w,
                                           int swapchain_h, int display_x, int display_y);

// Free resources.
void LibrashaderManager_Free(LibrashaderManager* manager);

#endif
