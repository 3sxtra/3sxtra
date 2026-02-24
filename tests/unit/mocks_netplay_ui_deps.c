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