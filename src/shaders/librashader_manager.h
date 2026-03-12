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

// ── Parameter API ─────────────────────────────────────────────
// Query the number of runtime parameters available.
int LibrashaderManager_GetParamCount(LibrashaderManager* manager);

// Get metadata for a parameter by index.
// Returns false if index is out of range.  out_ pointers may be NULL.
bool LibrashaderManager_GetParamInfo(LibrashaderManager* manager, int index, const char** out_name,
                                     const char** out_desc, float* out_value, float* out_initial, float* out_min,
                                     float* out_max, float* out_step);

// Set a parameter value by name (applied live to the filter chain).
bool LibrashaderManager_SetParam(LibrashaderManager* manager, const char* name, float value);

// Get the current runtime value of a parameter by name.
bool LibrashaderManager_GetParam(LibrashaderManager* manager, const char* name, float* out_value);

#endif
