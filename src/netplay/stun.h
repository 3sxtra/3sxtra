#ifndef NETPLAY_STUN_H
#define NETPLAY_STUN_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Result of a STUN binding request
typedef struct {
    uint32_t public_ip;   // Network byte order
    uint16_t public_port; // Network byte order
    uint16_t local_port;  // Host byte order — actual OS-bound port (may differ from public_port)
    int socket_fd;        // The socket used for STUN (reuse for hole punching)
} StunResult;

/// Perform a STUN Binding Request (RFC 5389).
/// Uses stun.l.google.com:19302.
/// Returns true on success and fills `result`.
/// The socket in result->socket_fd is left open for hole punching.
bool Stun_Discover(StunResult* result, uint16_t local_port);

/// Close the STUN socket when done
void Stun_CloseSocket(StunResult* result);

/// Encode a 4-byte IP + 2-byte port into an 8-character room code.
/// out_code must be at least 9 bytes (8 chars + null terminator).
void Stun_EncodeEndpoint(uint32_t ip, uint16_t port, char* out_code);

/// Decode an 8-character room code back into IP + port.
/// Returns true on success.
bool Stun_DecodeEndpoint(const char* code, uint32_t* out_ip, uint16_t* out_port);

/// Format an IP (network byte order) into dotted string.
void Stun_FormatIP(uint32_t ip_net, char* buf, int buf_size);

/// Perform UDP hole punching: send punch packets to peer's public endpoint
/// using the STUN socket. Both peers must call this simultaneously.
/// Returns true if a response was received from the peer (hole is open).
/// punch_duration_ms: how long to keep punching (e.g. 5000ms).
/// cancel_flag: optional atomic flag; if non-NULL and set to non-zero, punch exits early.
bool Stun_HolePunch(StunResult* local, uint32_t peer_ip, uint16_t peer_port, int punch_duration_ms,
                    SDL_AtomicInt* cancel_flag);

/// Set the STUN socket to non-blocking mode (for use after hole punch succeeds)
void Stun_SetNonBlocking(StunResult* result);

/// --- Socket helpers for GekkoNet adapter (avoids winsock2.h in netplay.c) ---

/// Send data via a raw socket to "ip:port". Returns bytes sent or -1.
int Stun_SocketSendTo(int fd, const char* dest_endpoint, const char* data, int length);

/// Receive data from a raw socket. Writes sender as "ip:port" into from_endpoint.
/// Returns bytes received, 0 if nothing available, or -1 on error.
int Stun_SocketRecvFrom(int fd, char* buf, int buf_size, char* from_endpoint, int endpoint_size);

/// Close a raw socket fd.
void Stun_SocketClose(int fd);

#ifdef __cplusplus
}
#endif

#endif
