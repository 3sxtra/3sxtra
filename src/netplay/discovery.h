#ifndef NETPLAY_DISCOVERY_H
#define NETPLAY_DISCOVERY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[32];
    char display_name[32]; // Human-readable display name from Identity module
    char player_id[64];    // Identity module player_id (for lobby<->LAN correlation)
    char ip[64];
    unsigned short port;
    uint32_t instance_id; // Unique ID for this peer instance
    bool wants_auto_connect;
    bool peer_ready; // Peer has seen us and is ready to connect
    bool is_challenging_me;
    int ft_value; // Peer's FT match mode (1=unranked, 2=FT2, etc.)
    uint32_t last_seen_ticks;
} NetplayDiscoveredPeer;

#ifdef ENABLE_NETPLAY

void Discovery_Init(bool auto_connect);
void Discovery_SetReady(bool ready);
void Discovery_SetChallengeTarget(uint32_t instance_id);
uint32_t Discovery_GetChallengeTarget(void);
void Discovery_Update();
void Discovery_Shutdown();
uint32_t Discovery_GetLocalInstanceID(void);

int Discovery_GetPeers(NetplayDiscoveredPeer* out_peers, int max_peers);

/// Dismiss a LAN challenger so the incoming challenge popup is suppressed.
/// The dismissal auto-clears if the peer stops challenging and re-challenges later.
void Discovery_DismissChallenger(uint32_t instance_id);

/// Returns the local configured netplay port (configuration.netplay.port).
uint16_t Discovery_GetLocalPort(void);

#else

static inline void Discovery_Init(bool auto_connect) {
    (void)auto_connect;
}
static inline void Discovery_SetReady(bool ready) {
    (void)ready;
}
static inline void Discovery_SetChallengeTarget(uint32_t instance_id) {
    (void)instance_id;
}
static inline uint32_t Discovery_GetChallengeTarget(void) {
    return 0;
}
static inline void Discovery_Update() {}
static inline void Discovery_Shutdown() {}
static inline uint32_t Discovery_GetLocalInstanceID(void) {
    return 0;
}
static inline int Discovery_GetPeers(NetplayDiscoveredPeer* out_peers, int max_peers) {
    (void)out_peers;
    (void)max_peers;
    return 0;
}
static inline void Discovery_DismissChallenger(uint32_t instance_id) {
    (void)instance_id;
}
static inline uint16_t Discovery_GetLocalPort(void) {
    return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
