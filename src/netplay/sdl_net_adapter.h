#ifndef SDL_NET_ADAPTER_H
#define SDL_NET_ADAPTER_H

#include "gekkonet.h"
#include <SDL3_net/SDL_net.h>

/// Create a GekkoNet adapter backed by an existing SDL3_Net datagram socket.
/// The socket must outlive the adapter.
GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock);

/// Destroy the adapter and release cached DNS entries.
void SDLNetAdapter_Destroy(void);

#endif
