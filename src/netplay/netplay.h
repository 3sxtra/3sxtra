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
    NETPLAY_SESSION_SPECTATING,
} NetplaySessionState;

void Netplay_SetPlayerNumber(int player_num);
int  Netplay_GetPlayerNumber(void);
void Netplay_SetRemoteIP(const char* ip);
void Netplay_SetLocalPort(unsigned short port);
void Netplay_SetRemotePort(unsigned short port);
void Netplay_EnterLobby();
void Netplay_Begin();
void Netplay_Run();
NetplaySessionState Netplay_GetSessionState();
void Netplay_HandleMenuExit();
void Netplay_GetNetworkStats(NetworkStats* stats);

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

bool Netplay_IsEnabled(void);
bool Netplay_PollEvent(NetplayEvent* out);

#include <SDL3_net/SDL_net.h>

/// Pass a pre-punched STUN socket fd for GekkoNet to reuse.
/// This avoids creating a new socket (which would lose the NAT pinhole).
/// Set to NULL to fall back to the default ASIO adapter.
void Netplay_SetStunSocket(NET_DatagramSocket* socket);

/// Set/get the negotiated First-To value for the upcoming match.
/// The challenger sets this before Netplay_Begin(); the receiver uses the
/// value from the invite/beacon/room data. 0 = use local config default.
void Netplay_SetNegotiatedFT(int ft);
int  Netplay_GetNegotiatedFT(void);

/// Begin a spectate-only session: connect to the active match host
/// and render the game without injecting local input.
void Netplay_BeginSpectate(const char* host_ip, unsigned short host_port);

/// Stop spectating and return to idle.
void Netplay_StopSpectate(void);

#ifdef __cplusplus
}
#endif

#endif
