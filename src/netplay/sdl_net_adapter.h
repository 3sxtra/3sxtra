#ifndef SDL_NET_ADAPTER_H
#define SDL_NET_ADAPTER_H

#include "gekkonet.h"
#include <SDL3_net/SDL_net.h>

/// Create a GekkoNet adapter backed by an existing SDL3_Net datagram socket.
/// The socket must outlive the adapter.
GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock);

/// Register the expected remote peer address (e.g. "1.2.3.4:5678").
/// Enables cross-IP (IPv4↔IPv6) normalization: packets arriving on the
/// expected port from a different IP are rewritten to match this address.
void SDLNetAdapter_SetExpectedRemote(const char* addr_str);

/// Destroy the adapter and release cached DNS entries.
void SDLNetAdapter_Destroy(void);

/// Send a text chat message to the remote peer via P2P (out-of-band on the
/// same UDP socket). Messages use a 0x3C magic prefix so GekkoNet ignores them.
void SDLNetAdapter_SendChat(const char* text);

/// Poll for an incoming P2P chat message. Returns true if a message was
/// available and copies it into out_text (up to max_len bytes).
bool SDLNetAdapter_PollChat(char* out_text, int max_len);

#endif
