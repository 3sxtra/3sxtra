#ifndef SDL_APP_SHADER_CONFIG_H
#define SDL_APP_SHADER_CONFIG_H

#include "shaders/librashader_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void SDLAppShader_Init(const char* base_path);
void SDLAppShader_Shutdown();
void SDLAppShader_ProcessPendingLoad();

LibrashaderManager* SDLAppShader_GetManager();
bool SDLAppShader_IsLibretroMode();

// Actions
void SDLAppShader_ToggleMode();
void SDLAppShader_CyclePreset();
void SDLAppShader_LoadPreset(int index);

// Accessors for UI
int SDLAppShader_GetAvailableCount();
const char* SDLAppShader_GetPresetName(int index);
int SDLAppShader_GetCurrentIndex();
void SDLAppShader_SetCurrentIndex(int index);
void SDLAppShader_SetMode(bool libretro);

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_SHADER_CONFIG_H
