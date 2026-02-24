#include "librashader_manager.h"
#include "port/sdl/sdl_app.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

// GLuint typedef for the GL backend dispatcher (avoids pulling in full GL headers)
typedef unsigned int GLuint;

// Forward declarations of backend-specific functions
LibrashaderManager* LibrashaderManager_Init_GL(const char* preset_path);
void LibrashaderManager_Render_GL(LibrashaderManager* manager,
                                  unsigned int input_texture, // GLuint
                                  int input_w, int input_h, int viewport_x, int viewport_y, int viewport_w,
                                  int viewport_h);
void LibrashaderManager_Free_GL(LibrashaderManager* manager);

LibrashaderManager* LibrashaderManager_Init_GPU(const char* preset_path);
void LibrashaderManager_Render_GPU(LibrashaderManager* manager, void* command_buffer, void* input_texture,
                                   void* intermediate_texture, void* swapchain_texture, int input_w, int input_h,
                                   int viewport_w, int viewport_h, int swapchain_w, int swapchain_h, int display_x,
                                   int display_y);
void LibrashaderManager_Free_GPU(LibrashaderManager* manager);

// Dispatcher Implementation

struct LibrashaderManager {
    void* backend_impl; // Opaque pointer to GL or GPU manager struct
    RendererBackend backend_type;
};

LibrashaderManager* LibrashaderManager_Init(const char* preset_path) {
    LibrashaderManager* wrapper = (LibrashaderManager*)calloc(1, sizeof(LibrashaderManager));
    if (!wrapper)
        return NULL;

    RendererBackend backend = SDLApp_GetRenderer();
    wrapper->backend_type = backend;

    if (backend == RENDERER_OPENGL) {
        wrapper->backend_impl = LibrashaderManager_Init_GL(preset_path);
    } else if (backend == RENDERER_SDLGPU) {
        wrapper->backend_impl = LibrashaderManager_Init_GPU(preset_path);
    }

    if (!wrapper->backend_impl) {
        free(wrapper);
        return NULL;
    }

    return wrapper;
}

// NOTE: This function signature was designed for GL (GLuint texture).
// For GPU, we need to pass SDL_GPUTexture pointers.
// We'll treat the 'input_texture' argument as a void* or union in the future,
// but for now, we'll cast it.
// Wait, the header defines input_texture as `GLuint`.
// I should update the header to be generic.

void LibrashaderManager_Render(LibrashaderManager* manager,
                               void* input_texture_ptr, // Changed from GLuint to void*
                               int input_w, int input_h, int viewport_x, int viewport_y, int viewport_w,
                               int viewport_h) {
    if (!manager || !manager->backend_impl)
        return;

    if (manager->backend_type == RENDERER_OPENGL) {
        // Cast void* to uintptr_t then to GLuint to suppress warnings
        GLuint tex = (GLuint)(uintptr_t)input_texture_ptr;
        LibrashaderManager_Render_GL((LibrashaderManager*)manager->backend_impl,
                                     tex,
                                     input_w,
                                     input_h,
                                     viewport_x,
                                     viewport_y,
                                     viewport_w,
                                     viewport_h);
    } else {
        // SDL_GPU path needs more arguments (command buffer, output texture).
        // This generic Render function is insufficient for SDL_GPU which requires
        // the command buffer to be passed in.
        // We will add a specific Render function for GPU in the header,
        // or varargs?
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "LibrashaderManager_Render called for GPU backend without CommandBuffer!");
    }
}

void LibrashaderManager_Render_GPU_Wrapper(LibrashaderManager* manager, void* command_buffer, void* input_texture,
                                           void* intermediate_texture, void* swapchain_texture, int input_w,
                                           int input_h, int viewport_w, int viewport_h, int swapchain_w,
                                           int swapchain_h, int display_x, int display_y) {
    if (!manager || !manager->backend_impl)
        return;
    if (manager->backend_type != RENDERER_SDLGPU)
        return;

    LibrashaderManager_Render_GPU((LibrashaderManager*)manager->backend_impl,
                                  command_buffer,
                                  input_texture,
                                  intermediate_texture,
                                  swapchain_texture,
                                  input_w,
                                  input_h,
                                  viewport_w,
                                  viewport_h,
                                  swapchain_w,
                                  swapchain_h,
                                  display_x,
                                  display_y);
}

void LibrashaderManager_Free(LibrashaderManager* manager) {
    if (!manager)
        return;

    if (manager->backend_type == RENDERER_OPENGL) {
        LibrashaderManager_Free_GL((LibrashaderManager*)manager->backend_impl);
    } else if (manager->backend_type == RENDERER_SDLGPU) {
        LibrashaderManager_Free_GPU((LibrashaderManager*)manager->backend_impl);
    }

    free(manager);
}
