/**
 * @file sdl_app_shader_config.h
 * @brief Shader preset discovery and cycling API.
 */
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

// ── Chain Management ───────────────────────────────────────────
// Append/prepend a preset's passes into the active chain.
void SDLAppShader_ChainAppend(int preset_index);
void SDLAppShader_ChainPrepend(int preset_index);

// Remove a single pass from the chain.
void SDLAppShader_ChainRemovePass(int pass_index);

// Move a pass within the chain (reorder).
void SDLAppShader_ChainMovePass(int from, int to);

// Clear the entire chain.
void SDLAppShader_ChainClear(void);

// Merge the chain preset, write to temp file, and reload the filter chain.
void SDLAppShader_ChainApply(void);

// Query chain state
int SDLAppShader_ChainGetPassCount(void);
const char* SDLAppShader_ChainGetPassShaderPath(int pass_index);
const char* SDLAppShader_ChainGetPassSourcePreset(int pass_index);

// Save the current chain as a new .slangp preset file.
bool SDLAppShader_ChainSaveAsPreset(const char* path);

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_SHADER_CONFIG_H
