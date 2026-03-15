#include "sdl_net_adapter.h"
#include <SDL3/SDL.h>

#define MAX_NETWORK_RESULTS 128
#define MAX_CACHED_PEERS 8  // Max unique peers (1v1 + spectators)

static NET_DatagramSocket* adapter_sock = NULL;
static GekkoNetAdapter adapter;
static GekkoNetResult* results[MAX_NETWORK_RESULTS];
static int result_count = 0;

// Per-peer address cache — avoids re-resolving DNS on every send.
// Supports multiple simultaneous peers (player + spectators).
typedef struct {
    char addr_key[64];        // "ip:port" string used as lookup key
    NET_Address* resolved;    // Cached NET_Address*
    Uint16 port;
} CachedPeer;

static CachedPeer cached_peers[MAX_CACHED_PEERS];
static int cached_peer_count = 0;

static CachedPeer* find_or_create_peer(const char* addr_str) {
    // Look up existing
    for (int i = 0; i < cached_peer_count; i++) {
        if (strcmp(cached_peers[i].addr_key, addr_str) == 0)
            return &cached_peers[i];
    }

    // Parse ip:port
    char ip[64];
    int port = 0;
    SDL_sscanf(addr_str, "%63[^:]:%d", ip, &port);

    // Evict oldest if full
    if (cached_peer_count >= MAX_CACHED_PEERS) {
        if (cached_peers[0].resolved)
            NET_UnrefAddress(cached_peers[0].resolved);
        SDL_memmove(&cached_peers[0], &cached_peers[1], sizeof(CachedPeer) * (MAX_CACHED_PEERS - 1));
        cached_peer_count--;
    }

    CachedPeer* p = &cached_peers[cached_peer_count++];
    SDL_strlcpy(p->addr_key, addr_str, sizeof(p->addr_key));
    p->resolved = NET_ResolveHostname(ip);
    p->port = (Uint16)port;
    return p;
}

static void send_data(GekkoNetAddress* addr, const char* data, int length) {
    if (!adapter_sock) return;

    CachedPeer* peer = find_or_create_peer((const char*)addr->data);
    if (!peer->resolved) return;

    switch (NET_GetAddressStatus(peer->resolved)) {
    case NET_SUCCESS:
        NET_SendDatagram(adapter_sock, peer->resolved, peer->port, data, length);
        break;
    case NET_FAILURE:
        NET_UnrefAddress(peer->resolved);
        peer->resolved = NULL;
        break;
    case NET_WAITING:
        break; // Still resolving — GekkoNet will retransmit
    }
}

static GekkoNetResult** receive_data(int* length) {
    result_count = 0;
    if (!adapter_sock) {
        *length = 0;
        return results;
    }

    NET_Datagram* dgram = NULL;
    while (result_count < MAX_NETWORK_RESULTS
           && NET_ReceiveDatagram(adapter_sock, &dgram)
           && dgram) {
        const char* ip_str = NET_GetAddressString(dgram->addr);
        char addr_str[64];
        SDL_snprintf(addr_str, sizeof(addr_str), "%s:%d", ip_str, (int)dgram->port);

        GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));
        size_t addr_len = SDL_strlen(addr_str);
        res->addr.data = SDL_malloc(addr_len + 1);
        SDL_strlcpy((char*)res->addr.data, addr_str, addr_len + 1);
        res->addr.size = (unsigned int)addr_len;

        res->data = SDL_malloc(dgram->buflen);
        SDL_memcpy(res->data, dgram->buf, dgram->buflen);
        res->data_len = (unsigned int)dgram->buflen;

        results[result_count++] = res;
        NET_DestroyDatagram(dgram);
        dgram = NULL;
    }

    *length = result_count;
    return results;
}

static void free_data(void* ptr) {
    SDL_free(ptr);
}

GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock) {
    // Guard against creating a second adapter without destroying the first
    if (adapter_sock != NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SDLNetAdapter: Creating adapter while one already exists — destroying previous");
        SDLNetAdapter_Destroy();
    }

    adapter_sock = sock;
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = free_data;
    return &adapter;
}

void SDLNetAdapter_Destroy(void) {
    adapter_sock = NULL;
    for (int i = 0; i < cached_peer_count; i++) {
        if (cached_peers[i].resolved) {
            NET_UnrefAddress(cached_peers[i].resolved);
            cached_peers[i].resolved = NULL;
        }
    }
    cached_peer_count = 0;
    SDL_memset(cached_peers, 0, sizeof(cached_peers));
}
