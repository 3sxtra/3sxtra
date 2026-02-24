#ifndef SDL_NETPLAY_UI_H
#define SDL_NETPLAY_UI_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void SDLNetplayUI_Init();
void SDLNetplayUI_Render(int window_width, int window_height);
void SDLNetplayUI_Shutdown();

void SDLNetplayUI_ProcessEvent(const SDL_Event* event);

// Helper for testing
void SDLNetplayUI_GetHUDText(char* buffer, size_t size);
int SDLNetplayUI_GetActiveToastCount();

void SDLNetplayUI_SetHUDVisible(bool visible);
bool SDLNetplayUI_IsHUDVisible();

void SDLNetplayUI_SetDiagnosticsVisible(bool visible);
bool SDLNetplayUI_IsDiagnosticsVisible();

// History access for testing
void SDLNetplayUI_GetHistory(float* ping_hist, float* rb_hist, int* count);

// FPS history data feed (called from sdl_app.c each frame)
void SDLNetplayUI_SetFPSHistory(const float* data, int count, float current_fps);

// === Native lobby bridge ===
// When native lobby is active, the ImGui lobby window is hidden but the
// underlying state machine (STUN, hole-punch, UPnP, server polling) keeps running.
void SDLNetplayUI_SetNativeLobbyActive(bool active);
bool SDLNetplayUI_IsNativeLobbyActive(void);

// Query lobby state machine
const char* SDLNetplayUI_GetStatusMsg(void);
const char* SDLNetplayUI_GetRoomCode(void);
bool SDLNetplayUI_IsDiscovering(void);
bool SDLNetplayUI_IsReady(void);

// Server search control
void SDLNetplayUI_StartSearch(void);
void SDLNetplayUI_StopSearch(void);
bool SDLNetplayUI_IsSearching(void);

// Online player list
int SDLNetplayUI_GetOnlinePlayerCount(void);
const char* SDLNetplayUI_GetOnlinePlayerName(int index);
const char* SDLNetplayUI_GetOnlinePlayerRoomCode(int index);
void SDLNetplayUI_ConnectToPlayer(int index);

// Pending internet invite (someone set connect_to = our room code)
bool SDLNetplayUI_HasPendingInvite(void);
const char* SDLNetplayUI_GetPendingInviteName(void);
void SDLNetplayUI_AcceptPendingInvite(void);

#ifdef __cplusplus
}
#endif

#endif
