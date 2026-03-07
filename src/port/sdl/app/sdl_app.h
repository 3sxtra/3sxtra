#ifndef SDL_APP_H
#define SDL_APP_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RendererBackend { RENDERER_OPENGL, RENDERER_SDLGPU, RENDERER_SDL2D } RendererBackend;

int SDLApp_Init();
void SDLApp_Quit();
bool SDLApp_PollEvents();
void SDLApp_BeginFrame();
void SDLApp_EndFrame();
void SDLApp_Exit();

// VSync Control (independent of frame pacer)
void SDLApp_SetVSync(bool enabled);
bool SDLApp_IsVSyncEnabled();

// Frame Rate Uncap (decoupled rendering)
void SDLApp_PresentOnly(void);
Uint64 SDLApp_GetTargetFrameTimeNS(void);
bool SDLApp_IsFrameRateUncapped(void);

unsigned int SDLApp_GetPassthruShaderProgram();
unsigned int SDLApp_GetSceneShaderProgram();
unsigned int SDLApp_GetSceneArrayShaderProgram();

int SDLApp_GetScaleMode();
void SDLApp_SetScaleMode(int mode);
const char* SDLApp_GetScaleModeName(int mode);

// Shader Mode (Internal vs Libretro)
bool SDLApp_GetShaderModeLibretro();
void SDLApp_SetShaderModeLibretro(bool libretro);

// Preset Management
int SDLApp_GetCurrentPresetIndex();
void SDLApp_SetCurrentPresetIndex(int index);
int SDLApp_GetAvailablePresetCount();
const char* SDLApp_GetPresetName(int index);
void SDLApp_LoadPreset(int index);

// CLI / Config Accessors
void SDLApp_SetWindowPosition(int x, int y);
void SDLApp_SetWindowSize(int width, int height);

void SDLApp_SetRenderer(RendererBackend backend);
RendererBackend SDLApp_GetRenderer(void);

// GPU Accessors
SDL_GPUDevice* SDLApp_GetGPUDevice(void);
SDL_Renderer* SDLApp_GetSDLRenderer(void);
SDL_Window* SDLApp_GetWindow(void);

// Utils
SDL_FRect get_letterbox_rect(int win_w, int win_h);
unsigned int create_shader_program(const char* base_path, const char* vertex_path, const char* fragment_path);

#ifdef __cplusplus
}
#endif

#endif
