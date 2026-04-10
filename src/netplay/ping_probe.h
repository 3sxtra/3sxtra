#ifndef NETPLAY_PING_PROBE_H
#define NETPLAY_PING_PROBE_H

#include <stdbool.h>
#include <stdint.h>

struct NET_DatagramSocket; // Forward declaration — avoids including SDL3_net header

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_NETPLAY

/// Initialize the ping probe system.
/// socket: an existing SDL3_Net datagram socket (typically the STUN socket) to send/receive probes on.
void PingProbe_Init(struct NET_DatagramSocket* socket);

/// Shut down the ping probe system and clear all peer state.
void PingProbe_Shutdown(void);

/// Add or update a peer to probe.
/// player_id is a unique identifier string (max 63 chars).
void PingProbe_AddPeer(const char* ip, uint16_t port, const char* player_id);

/// Remove a peer by player_id.
void PingProbe_RemovePeer(const char* player_id);

/// Remove all peers.
void PingProbe_ClearPeers(void);

/// Send pending probes and receive any incoming pings/pongs.
/// Call this periodically from a background thread (e.g. lobby poll thread).
void PingProbe_Update(void);

/// Get the smoothed RTT for a peer in milliseconds.
/// Returns -1 if the peer is unknown or no measurement is available yet.
int PingProbe_GetRTT(const char* player_id);

/// Returns true if at least one pong has been received from this peer
/// and the peer has not timed out (5+ consecutive missed pongs).
bool PingProbe_IsReachable(const char* player_id);

#else

static inline void PingProbe_Init(struct NET_DatagramSocket* socket) { (void)socket; }
static inline void PingProbe_Shutdown(void) {}
static inline void PingProbe_AddPeer(const char* ip, uint16_t port, const char* player_id) { (void)ip; (void)port; (void)player_id; }
static inline void PingProbe_RemovePeer(const char* player_id) { (void)player_id; }
static inline void PingProbe_ClearPeers(void) {}
static inline void PingProbe_Update(void) {}
static inline int PingProbe_GetRTT(const char* player_id) { (void)player_id; return -1; }
static inline bool PingProbe_IsReachable(const char* player_id) { (void)player_id; return false; }

#endif


#ifdef __cplusplus
}
#endif

#endif
