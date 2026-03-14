/**
 * @file sdl_netplay_ui.h
 * @brief SDL netplay UI overlay API.
 */
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
const char* SDLNetplayUI_GetOnlinePlayerRegion(int index);
const char* SDLNetplayUI_GetOnlinePlayerCountry(int index);
const char* SDLNetplayUI_GetOnlinePlayerConnType(int index);
int SDLNetplayUI_GetOnlinePlayerPing(int index);
void SDLNetplayUI_ConnectToPlayer(int index);

// Pending internet invite (someone set connect_to = our room code)
bool SDLNetplayUI_HasPendingInvite(void);
const char* SDLNetplayUI_GetPendingInviteName(void);
const char* SDLNetplayUI_GetPendingInviteRegion(void);
int SDLNetplayUI_GetPendingInvitePing(void);
void SDLNetplayUI_AcceptPendingInvite(void);
void SDLNetplayUI_DeclinePendingInvite(void);

// Outgoing challenge (we initiated a connection)
bool SDLNetplayUI_HasOutgoingChallenge(void);
const char* SDLNetplayUI_GetOutgoingChallengeName(void);
int SDLNetplayUI_GetOutgoingChallengePing(void);
void SDLNetplayUI_CancelOutgoingChallenge(void);

// FPS data access (for RmlUi netplay UI module)
const float* SDLNetplayUI_GetFPSHistory(int* out_count);
float SDLNetplayUI_GetCurrentFPS(void);

// === Phase 6: Casual lobby bridge ===

/// Check if an opponent passes local connection filters (region lock, max ping, WiFi block).
/// conn_type: "wired", "wifi", or "unknown". rtt_ms: server RTT (-1 if unknown).
bool SDLNetplayUI_PlayerPassesFilters(const char* conn_type, int rtt_ms, const char* region);

/// Initiate P2P hole punch for a casual lobby match.
/// opponent_room_code is the STUN-encoded endpoint of the opponent.
/// opponent_name is for display in status messages.
/// opponent_player_id is the unique lobby server ID of the opponent.
/// we_are_p1: determines player number assignment.
void SDLNetplayUI_StartCasualMatchPunch(const char* opponent_room_code, const char* opponent_name, const char* opponent_player_id, bool we_are_p1);

#ifdef __cplusplus
}
#endif

#endif
