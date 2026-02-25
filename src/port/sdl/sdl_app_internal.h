#ifndef SDL_APP_INTERNAL_H
#define SDL_APP_INTERNAL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// App State Toggles
void SDLApp_ToggleMenu();
void SDLApp_ToggleShaderMenu();
void SDLApp_ToggleModsMenu();
void SDLApp_ToggleStageConfigMenu();
void SDLApp_ToggleTrainingMenu();
void SDLApp_CycleScaleMode();
void SDLApp_ToggleFullscreen();
void SDLApp_HandleMouseMotion();
void SDLApp_SaveScreenshot();
void SDLApp_ToggleShaderMode();
void SDLApp_CyclePreset();
void SDLApp_ToggleBezel();
void SDLApp_ToggleFrameRateUncap();
void SDLApp_ToggleDebugHUD();

// Window Management
void SDLApp_HandleWindowResize(int w, int h);
void SDLApp_HandleWindowMove(int x, int y);

// Internal state accessors if needed
bool SDLApp_IsMenuVisible();

// GPU Resource Management Hooks
void SDLApp_ClearLibrashaderIntermediate();

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_INTERNAL_H
