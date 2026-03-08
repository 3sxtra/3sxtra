#include "netplay/netplay.h"
#include <string.h>

static NetworkStats mock_stats = {0};
static NetplayEvent mock_event_queue[16];
static int mock_queue_head = 0;
static int mock_queue_tail = 0;

void MockNetplay_SetStats(int delay, int ping, int rollback) {
    mock_stats.delay = delay;
    mock_stats.ping = ping;
    mock_stats.rollback = rollback;
}

void MockNetplay_PushEvent(NetplayEventType type) {
    int next = (mock_queue_tail + 1) % 16;
    if (next != mock_queue_head) {
        mock_event_queue[mock_queue_tail].type = type;
        mock_queue_tail = next;
    }
}

void Netplay_GetNetworkStats(NetworkStats* stats) {
    if (stats) {
        *stats = mock_stats;
    }
}

bool Netplay_PollEvent(NetplayEvent* event) {
    if (mock_queue_head != mock_queue_tail) {
        if (event) {
            *event = mock_event_queue[mock_queue_head];
        }
        mock_queue_head = (mock_queue_head + 1) % 16;
        return true;
    }
    return false;
}

// Stubs for other functions potentially used by UI
bool Netplay_IsEnabled() {
    return true;
}

NetplaySessionState Netplay_GetSessionState() {
    return NETPLAY_SESSION_RUNNING;
}

void Netplay_SetEnabled(bool enabled) {}
void Netplay_SetPlayer(int player) {}
void Netplay_Begin() {}
void Netplay_Run() {}

bool LobbyServer_IsConfigured() { return false; }
const char* Config_GetString(const char* key, const char* default_val) { return default_val; }
void Netplay_SetRemoteIP(const char* ip) {}
void Netplay_SetRemotePort(unsigned short port) {}
void Netplay_SetLocalPort(unsigned short port) {}
void Netplay_SetStunSocket(int fd) {}
void Netplay_SetPlayerNumber(int player) {}
bool Upnp_AddMapping(int port) { return false; }
bool Upnp_RemoveMapping(int port) { return false; }
void LobbyServer_UpdatePresence(const char* game, const char* status, const char* state, const char* connection_info) {}
void LobbyServer_StartSearching(const char* game, int max_results) {}
void LobbyServer_StopSearching() {}
void LobbyServer_Leave() {}
bool LobbyServer_GetSearching() { return false; }