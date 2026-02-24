#ifndef NETPLAY_DISCOVERY_H
#define NETPLAY_DISCOVERY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[32];
    char ip[64];
    unsigned short port;
    uint32_t instance_id; // Unique ID for this peer instance
    bool wants_auto_connect;
    bool peer_ready; // Peer has seen us and is ready to connect
    bool is_challenging_me;
    uint32_t last_seen_ticks;
} NetplayDiscoveredPeer;

void Discovery_Init(bool auto_connect);
void Discovery_SetReady(bool ready);
void Discovery_SetChallengeTarget(uint32_t instance_id);
int Discovery_GetChallengeTarget(void);
void Discovery_Update();
void Discovery_Shutdown();
uint32_t Discovery_GetLocalInstanceID(void);

int Discovery_GetPeers(NetplayDiscoveredPeer* out_peers, int max_peers);

#ifdef __cplusplus
}
#endif

#endif
