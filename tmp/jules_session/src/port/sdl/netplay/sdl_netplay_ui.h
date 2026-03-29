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

#ifdef ENABLE_NETPLAY

void SDLNetplayUI_Init(void);
void SDLNetplayUI_Update(void);
void SDLNetplayUI_Shutdown(void);

void SDLNetplayUI_ProcessEvent(const SDL_Event* event);

void SDLNetplayUI_GetHUDText(char* buffer, size_t size);
int SDLNetplayUI_GetActiveToastCount(void);
void SDLNetplayUI_DrawNativeHUD(void);

void SDLNetplayUI_SetHUDVisible(bool visible);
bool SDLNetplayUI_IsHUDVisible(void);

void SDLNetplayUI_SetDiagnosticsVisible(bool visible);
bool SDLNetplayUI_IsDiagnosticsVisible(void);

void SDLNetplayUI_GetHistory(float* ping_hist, float* rb_hist, int* count);
void SDLNetplayUI_SetFPSHistory(const float* data, int count, float current_fps);

void SDLNetplayUI_SetNativeLobbyActive(bool active);
bool SDLNetplayUI_IsNativeLobbyActive(void);

const char* SDLNetplayUI_GetStatusMsg(void);
const char* SDLNetplayUI_GetRoomCode(void);
bool SDLNetplayUI_IsDiscovering(void);
bool SDLNetplayUI_IsReady(void);

void SDLNetplayUI_StartSearch(void);
void SDLNetplayUI_StopSearch(void);
bool SDLNetplayUI_IsSearching(void);

int SDLNetplayUI_GetOnlinePlayerCount(void);
const char* SDLNetplayUI_GetOnlinePlayerName(int index);
const char* SDLNetplayUI_GetOnlinePlayerRoomCode(int index);
const char* SDLNetplayUI_GetOnlinePlayerRegion(int index);
const char* SDLNetplayUI_GetOnlinePlayerCountry(int index);
const char* SDLNetplayUI_GetOnlinePlayerConnType(int index);
int SDLNetplayUI_GetOnlinePlayerPing(int index);
int SDLNetplayUI_GetOnlinePlayerFT(int index);
void SDLNetplayUI_ConnectToPlayer(int index);

bool SDLNetplayUI_HasPendingInvite(void);
const char* SDLNetplayUI_GetPendingInviteName(void);
const char* SDLNetplayUI_GetPendingInviteRegion(void);
int SDLNetplayUI_GetPendingInvitePing(void);
int SDLNetplayUI_GetPendingInviteFT(void);
void SDLNetplayUI_AcceptPendingInvite(void);
void SDLNetplayUI_DeclinePendingInvite(void);

bool SDLNetplayUI_HasOutgoingChallenge(void);
const char* SDLNetplayUI_GetOutgoingChallengeName(void);
int SDLNetplayUI_GetOutgoingChallengePing(void);
void SDLNetplayUI_CancelOutgoingChallenge(void);

const float* SDLNetplayUI_GetFPSHistory(int* out_count);
float SDLNetplayUI_GetCurrentFPS(void);

bool SDLNetplayUI_PlayerPassesFilters(const char* conn_type, int rtt_ms, const char* region);
void SDLNetplayUI_StartCasualMatchPunch(const char* opponent_room_code, const char* opponent_name,
                                        const char* opponent_player_id, bool we_are_p1);
void SDLNetplayUI_StartSpectatePunch(const char* host_room_code, const char* host_name, const char* host_player_id);
void SDLNetplayUI_ReportNaturalMatchEnd(void);
bool SDLNetplayUI_IsSessionComplete(void);
bool SDLNetplayUI_ConsumeSessionComplete(char* out_winner, size_t winner_size);

#else /* !ENABLE_NETPLAY — inline no-ops */

static inline void SDLNetplayUI_Init(void) {}
static inline void SDLNetplayUI_Update(void) {}
static inline void SDLNetplayUI_Shutdown(void) {}
static inline void SDLNetplayUI_ProcessEvent(const SDL_Event* e) { (void)e; }
static inline void SDLNetplayUI_GetHUDText(char* b, size_t s) { (void)b; (void)s; }
static inline int  SDLNetplayUI_GetActiveToastCount(void) { return 0; }
static inline void SDLNetplayUI_DrawNativeHUD(void) {}
static inline void SDLNetplayUI_SetHUDVisible(bool v) { (void)v; }
static inline bool SDLNetplayUI_IsHUDVisible(void) { return false; }
static inline void SDLNetplayUI_SetDiagnosticsVisible(bool v) { (void)v; }
static inline bool SDLNetplayUI_IsDiagnosticsVisible(void) { return false; }
static inline void SDLNetplayUI_GetHistory(float* p, float* r, int* c) { (void)p; (void)r; (void)c; }
static inline void SDLNetplayUI_SetFPSHistory(const float* d, int c, float f) { (void)d; (void)c; (void)f; }
static inline void SDLNetplayUI_SetNativeLobbyActive(bool a) { (void)a; }
static inline bool SDLNetplayUI_IsNativeLobbyActive(void) { return false; }
static inline const char* SDLNetplayUI_GetStatusMsg(void) { return ""; }
static inline const char* SDLNetplayUI_GetRoomCode(void) { return ""; }
static inline bool SDLNetplayUI_IsDiscovering(void) { return false; }
static inline bool SDLNetplayUI_IsReady(void) { return false; }
static inline void SDLNetplayUI_StartSearch(void) {}
static inline void SDLNetplayUI_StopSearch(void) {}
static inline bool SDLNetplayUI_IsSearching(void) { return false; }
static inline int  SDLNetplayUI_GetOnlinePlayerCount(void) { return 0; }
static inline const char* SDLNetplayUI_GetOnlinePlayerName(int i) { (void)i; return ""; }
static inline const char* SDLNetplayUI_GetOnlinePlayerRoomCode(int i) { (void)i; return ""; }
static inline const char* SDLNetplayUI_GetOnlinePlayerRegion(int i) { (void)i; return ""; }
static inline const char* SDLNetplayUI_GetOnlinePlayerCountry(int i) { (void)i; return ""; }
static inline const char* SDLNetplayUI_GetOnlinePlayerConnType(int i) { (void)i; return ""; }
static inline int  SDLNetplayUI_GetOnlinePlayerPing(int i) { (void)i; return 0; }
static inline int  SDLNetplayUI_GetOnlinePlayerFT(int i) { (void)i; return 0; }
static inline void SDLNetplayUI_ConnectToPlayer(int i) { (void)i; }
static inline bool SDLNetplayUI_HasPendingInvite(void) { return false; }
static inline const char* SDLNetplayUI_GetPendingInviteName(void) { return ""; }
static inline const char* SDLNetplayUI_GetPendingInviteRegion(void) { return ""; }
static inline int  SDLNetplayUI_GetPendingInvitePing(void) { return 0; }
static inline int  SDLNetplayUI_GetPendingInviteFT(void) { return 0; }
static inline void SDLNetplayUI_AcceptPendingInvite(void) {}
static inline void SDLNetplayUI_DeclinePendingInvite(void) {}
static inline bool SDLNetplayUI_HasOutgoingChallenge(void) { return false; }
static inline const char* SDLNetplayUI_GetOutgoingChallengeName(void) { return ""; }
static inline int  SDLNetplayUI_GetOutgoingChallengePing(void) { return 0; }
static inline void SDLNetplayUI_CancelOutgoingChallenge(void) {}
static inline const float* SDLNetplayUI_GetFPSHistory(int* c) { if(c) *c=0; return (const float*)0; }
static inline float SDLNetplayUI_GetCurrentFPS(void) { return 0.0f; }
static inline bool SDLNetplayUI_PlayerPassesFilters(const char* c, int r, const char* rg) {
    (void)c; (void)r; (void)rg; return false;
}
static inline void SDLNetplayUI_StartCasualMatchPunch(const char* rc, const char* n,
    const char* id, bool p1) { (void)rc; (void)n; (void)id; (void)p1; }
static inline void SDLNetplayUI_StartSpectatePunch(const char* rc, const char* n, const char* id) { (void)rc; (void)n; (void)id; }
static inline void SDLNetplayUI_ReportNaturalMatchEnd(void) {}
static inline bool SDLNetplayUI_IsSessionComplete(void) { return false; }
static inline bool SDLNetplayUI_ConsumeSessionComplete(char* w, size_t s) { (void)w; (void)s; return false; }

#endif /* ENABLE_NETPLAY */

#ifdef __cplusplus
}
#endif

#endif
