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

#endif
