#ifndef SDL_APP_H
#define SDL_APP_H

#include <SDL3/SDL.h>
#include <glad/gl.h>

extern SDL_Window* window;

typedef enum RendererBackend {
    RENDERER_OPENGL,
    RENDERER_SDLGPU,
    RENDERER_SDL2D,
    RENDERER_SDL2D_CLASSIC
} RendererBackend;

// Returns true for any SDL2D variant (optimized or classic benchmark)
static inline bool is_sdl2d_backend(RendererBackend r) {
    return r == RENDERER_SDL2D || r == RENDERER_SDL2D_CLASSIC;
}

GLuint create_shader_program(const char* base_path, const char* vertex_path, const char* fragment_path);
int SDLApp_Init();
void SDLApp_Quit();

/// @brief Poll SDL events.
/// @return `true` if the main loop should continue running, `false` otherwise.
bool SDLApp_PollEvents();

void SDLApp_BeginFrame();
void SDLApp_EndFrame();
void SDLApp_Exit();
unsigned int SDLApp_GetPassthruShaderProgram();
unsigned int SDLApp_GetSceneShaderProgram();
unsigned int SDLApp_GetSceneArrayShaderProgram();

// CLI override setters (must be called before SDLApp_Init)
void SDLApp_SetWindowPosition(int x, int y);
void SDLApp_SetWindowSize(int width, int height);
void SDLApp_SetRenderer(RendererBackend backend);
RendererBackend SDLApp_GetRenderer(void);

#endif
