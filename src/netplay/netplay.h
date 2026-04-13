#ifndef NETPLAY_H
#define NETPLAY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NetworkStats {
    int delay;
    int ping;
    int rollback;
} NetworkStats;

typedef enum NetplaySessionState {
    NETPLAY_SESSION_IDLE,
    NETPLAY_SESSION_LOBBY,
    NETPLAY_SESSION_TRANSITIONING,
    NETPLAY_SESSION_CONNECTING,
    NETPLAY_SESSION_RUNNING,
    NETPLAY_SESSION_EXITING,
} NetplaySessionState;

// === 3SX-private extensions ===

typedef enum {
    NETPLAY_EVENT_NONE = 0,
    NETPLAY_EVENT_SYNCHRONIZING,
    NETPLAY_EVENT_CONNECTED,
    NETPLAY_EVENT_DISCONNECTED,
} NetplayEventType;

typedef struct {
    NetplayEventType type;
} NetplayEvent;

#ifdef ENABLE_NETPLAY

void Netplay_SetPlayerNumber(int player_num);
int Netplay_GetPlayerNumber(void);
void Netplay_SetRemoteIP(const char* ip);
void Netplay_SetLocalPort(unsigned short port);
void Netplay_SetRemotePort(unsigned short port);
void Netplay_EnterLobby();
void Netplay_Begin();
void Netplay_Run();
NetplaySessionState Netplay_GetSessionState();
void Netplay_HandleMenuExit();
void Netplay_GetNetworkStats(NetworkStats* stats);

bool Netplay_IsEnabled(void);
bool Netplay_PollEvent(NetplayEvent* out);

#ifndef PLATFORM_PS3
#include <SDL3_net/SDL_net.h>
#else
typedef void NET_DatagramSocket;
#endif

/// Pass a pre-punched STUN socket fd for GekkoNet to reuse.
/// This avoids creating a new socket (which would lose the NAT pinhole).
/// Set to NULL to fall back to the default ASIO adapter.
void Netplay_SetStunSocket(NET_DatagramSocket* socket);

/// Set/get the negotiated First-To value for the upcoming match.
/// The challenger sets this before Netplay_Begin(); the receiver uses the
/// value from the invite/beacon/room data. 0 = use local config default.
void Netplay_SetNegotiatedFT(int ft);
int Netplay_GetNegotiatedFT(void);

// For Netplay_GetPlayerHandle and BattleStartFrame:
int Netplay_GetPlayerHandle(void);
int Netplay_GetBattleStartFrame(void);

#else

static inline void Netplay_SetPlayerNumber(int p) {
    (void)p;
}
static inline int Netplay_GetPlayerNumber(void) {
    return 0;
}
static inline void Netplay_SetRemoteIP(const char* ip) {
    (void)ip;
}
static inline void Netplay_SetLocalPort(unsigned short p) {
    (void)p;
}
static inline void Netplay_SetRemotePort(unsigned short p) {
    (void)p;
}
static inline void Netplay_EnterLobby(void) {}
static inline void Netplay_Begin(void) {}
static inline void Netplay_Run(void) {}
static inline NetplaySessionState Netplay_GetSessionState(void) {
    return NETPLAY_SESSION_IDLE;
}
static inline void Netplay_HandleMenuExit(void) {}
static inline void Netplay_GetNetworkStats(NetworkStats* stats) {
    (void)stats;
}
static inline bool Netplay_IsEnabled(void) {
    return false;
}
static inline bool Netplay_PollEvent(NetplayEvent* out) {
    (void)out;
    return false;
}
static inline void Netplay_SetStunSocket(void* socket) {
    (void)socket;
}
static inline void Netplay_SetNegotiatedFT(int ft) {
    (void)ft;
}
static inline int Netplay_GetNegotiatedFT(void) {
    return 0;
}
static inline int Netplay_GetPlayerHandle(void) {
    return 0;
}
static inline int Netplay_GetBattleStartFrame(void) {
    return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
