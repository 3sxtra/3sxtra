#ifndef NETPLAY_PING_PROBE_H
#define NETPLAY_PING_PROBE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the ping probe system.
/// socket_fd: an existing UDP socket (typically the STUN socket) to send/receive probes on.
/// The socket must already be bound and should be in non-blocking mode (or have a short timeout).
void PingProbe_Init(int socket_fd);

/// Shut down the ping probe system and clear all peer state.
void PingProbe_Shutdown(void);

/// Add or update a peer to probe. ip/port are in network byte order.
/// player_id is a unique identifier string (max 63 chars).
void PingProbe_AddPeer(uint32_t ip, uint16_t port, const char* player_id);

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

#ifdef __cplusplus
}
#endif

#endif
