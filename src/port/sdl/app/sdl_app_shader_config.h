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

#ifdef ENABLE_LIBRASHADER
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

// ── Runtime Parameter API ──────────────────────────────────────
// Query and modify shader parameters on the active filter chain.
int SDLAppShader_GetParamCount(void);
const char* SDLAppShader_GetParamName(int index);
const char* SDLAppShader_GetParamDesc(int index);
float SDLAppShader_GetParamValue(int index);
float SDLAppShader_GetParamInitial(int index);
float SDLAppShader_GetParamMin(int index);
float SDLAppShader_GetParamMax(int index);
float SDLAppShader_GetParamStep(int index);
void SDLAppShader_SetParamValue(int index, float value);
void SDLAppShader_ResetParam(int index);
#else
static inline void SDLAppShader_Init(const char* base_path) {}
static inline void SDLAppShader_Shutdown() {}
static inline void SDLAppShader_ProcessPendingLoad() {}

static inline LibrashaderManager* SDLAppShader_GetManager() { return NULL; }
static inline bool SDLAppShader_IsLibretroMode() { return false; }

static inline void SDLAppShader_ToggleMode() {}
static inline void SDLAppShader_CyclePreset() {}
static inline void SDLAppShader_LoadPreset(int index) {}

static inline int SDLAppShader_GetAvailableCount() { return 0; }
static inline const char* SDLAppShader_GetPresetName(int index) { return NULL; }
static inline int SDLAppShader_GetCurrentIndex() { return 0; }
static inline void SDLAppShader_SetCurrentIndex(int index) {}
static inline void SDLAppShader_SetMode(bool libretro) {}

static inline void SDLAppShader_ChainAppend(int preset_index) {}
static inline void SDLAppShader_ChainPrepend(int preset_index) {}
static inline void SDLAppShader_ChainRemovePass(int pass_index) {}
static inline void SDLAppShader_ChainMovePass(int from, int to) {}
static inline void SDLAppShader_ChainClear(void) {}
static inline void SDLAppShader_ChainApply(void) {}

static inline int SDLAppShader_ChainGetPassCount(void) { return 0; }
static inline const char* SDLAppShader_ChainGetPassShaderPath(int pass_index) { return NULL; }
static inline const char* SDLAppShader_ChainGetPassSourcePreset(int pass_index) { return NULL; }
static inline bool SDLAppShader_ChainSaveAsPreset(const char* path) { return false; }

static inline int SDLAppShader_GetParamCount(void) { return 0; }
static inline const char* SDLAppShader_GetParamName(int index) { return NULL; }
static inline const char* SDLAppShader_GetParamDesc(int index) { return NULL; }
static inline float SDLAppShader_GetParamValue(int index) { return 0.0f; }
static inline float SDLAppShader_GetParamInitial(int index) { return 0.0f; }
static inline float SDLAppShader_GetParamMin(int index) { return 0.0f; }
static inline float SDLAppShader_GetParamMax(int index) { return 0.0f; }
static inline float SDLAppShader_GetParamStep(int index) { return 0.0f; }
static inline void SDLAppShader_SetParamValue(int index, float value) {}
static inline void SDLAppShader_ResetParam(int index) {}
#endif

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_SHADER_CONFIG_H
