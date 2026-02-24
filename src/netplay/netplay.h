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

void Netplay_SetPlayerNumber(int player_num);
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

/// Pass a pre-punched STUN socket fd for GekkoNet to reuse.
/// This avoids creating a new socket (which would lose the NAT pinhole).
/// Set to -1 to fall back to the default ASIO adapter.
void Netplay_SetStunSocket(int fd);

#ifdef __cplusplus
}
#endif

#endif
