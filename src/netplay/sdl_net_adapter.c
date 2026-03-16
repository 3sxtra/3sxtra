#include "sdl_net_adapter.h"
#include <SDL3/SDL.h>
#include <string.h>

#define MAX_NETWORK_RESULTS 128
#define MAX_CACHED_PEERS 8 // Max unique peers (1v1 + spectators)

static NET_DatagramSocket* adapter_sock = NULL;
static GekkoNetAdapter adapter;
static GekkoNetResult* results[MAX_NETWORK_RESULTS];
static int result_count = 0;

// Expected remote address — used for cross-IP (IPv4↔IPv6) normalization.
// When a packet arrives on the expected port but from a different IP,
// we rewrite the source to match what GekkoNet was configured with.
static char expected_remote_addr[64] = {0}; // Full "ip:port" string
static Uint16 expected_remote_port = 0;
static bool cross_ip_logged = false; // Log only once per session

// Per-peer address cache — avoids re-resolving DNS on every send.
// Supports multiple simultaneous peers (player + spectators).
typedef struct {
    char addr_key[64];     // "ip:port" string used as lookup key
    NET_Address* resolved; // Cached NET_Address*
    Uint16 port;
} CachedPeer;

static CachedPeer cached_peers[MAX_CACHED_PEERS];
static int cached_peer_count = 0;

/**
 * @brief Strip "::ffff:" prefix from IPv4-mapped IPv6 addresses.
 *
 * On dual-stack sockets, incoming IPv4 packets arrive with source addresses
 * in IPv4-mapped IPv6 form (e.g. "::ffff:192.168.86.26"). GekkoNet compares
 * addresses as strings, so "::ffff:192.168.86.26:35413" != "192.168.86.26:35413".
 * Stripping the prefix normalizes to plain IPv4 for consistent matching.
 */
static const char* strip_ipv4_mapped_prefix(const char* ip) {
    if (SDL_strncmp(ip, "::ffff:", 7) == 0)
        return ip + 7;
    return ip;
}

/**
 * @brief Parse "ip:port" address string, handling both IPv4 and IPv6.
 *
 * Uses the last ':' as the port separator. This correctly parses:
 *   "192.168.86.26:35413"          → ip=192.168.86.26, port=35413
 *   "::ffff:192.168.86.26:35413"   → ip=::ffff:192.168.86.26, port=35413
 *   "[::1]:5678"                   → ip=::1, port=5678  (bracketed form)
 */
static void parse_addr_str(const char* addr_str, char* out_ip, int ip_size, int* out_port) {
    *out_port = 0;
    out_ip[0] = '\0';

    // Handle bracketed IPv6: [ip]:port
    if (addr_str[0] == '[') {
        const char* bracket_end = strchr(addr_str, ']');
        if (bracket_end) {
            int ip_len = (int)(bracket_end - addr_str - 1);
            if (ip_len >= ip_size) ip_len = ip_size - 1;
            SDL_memcpy(out_ip, addr_str + 1, ip_len);
            out_ip[ip_len] = '\0';
            if (bracket_end[1] == ':')
                *out_port = SDL_atoi(bracket_end + 2);
            return;
        }
    }

    // Use last ':' as port separator (works for both IPv4 and unbracketed IPv6)
    const char* last_colon = strrchr(addr_str, ':');
    if (!last_colon) {
        SDL_strlcpy(out_ip, addr_str, ip_size);
        return;
    }

    int ip_len = (int)(last_colon - addr_str);
    if (ip_len >= ip_size) ip_len = ip_size - 1;
    SDL_memcpy(out_ip, addr_str, ip_len);
    out_ip[ip_len] = '\0';
    *out_port = SDL_atoi(last_colon + 1);
}

static CachedPeer* find_or_create_peer(const char* addr_str) {
    // Look up existing
    for (int i = 0; i < cached_peer_count; i++) {
        if (strcmp(cached_peers[i].addr_key, addr_str) == 0)
            return &cached_peers[i];
    }

    // Parse ip:port (IPv4 and IPv6 safe)
    char ip[64];
    int port = 0;
    parse_addr_str(addr_str, ip, sizeof(ip), &port);

    // Normalize IPv4-mapped IPv6 for DNS resolution
    const char* resolve_ip = strip_ipv4_mapped_prefix(ip);

    // Evict oldest if full
    if (cached_peer_count >= MAX_CACHED_PEERS) {
        if (cached_peers[0].resolved)
            NET_UnrefAddress(cached_peers[0].resolved);
        SDL_memmove(&cached_peers[0], &cached_peers[1], sizeof(CachedPeer) * (MAX_CACHED_PEERS - 1));
        cached_peer_count--;
    }

    CachedPeer* p = &cached_peers[cached_peer_count++];
    SDL_strlcpy(p->addr_key, addr_str, sizeof(p->addr_key));
    p->resolved = NET_ResolveHostname(resolve_ip);
    p->port = (Uint16)port;
    return p;
}

static void send_data(GekkoNetAddress* addr, const char* data, int length) {
    if (!adapter_sock)
        return;

    // GekkoNet address data may not be null-terminated — copy with explicit size
    char addr_buf[64];
    unsigned int copy_len = addr->size < sizeof(addr_buf) - 1 ? addr->size : sizeof(addr_buf) - 1;
    SDL_memcpy(addr_buf, addr->data, copy_len);
    addr_buf[copy_len] = '\0';

    CachedPeer* peer = find_or_create_peer(addr_buf);
    if (!peer->resolved)
        return;

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
    while (result_count < MAX_NETWORK_RESULTS && NET_ReceiveDatagram(adapter_sock, &dgram) && dgram) {
        const char* raw_ip = NET_GetAddressString(dgram->addr);
        // Normalize IPv4-mapped IPv6 addresses (::ffff:x.x.x.x → x.x.x.x)
        // so GekkoNet can match incoming packets to the configured remote peer.
        const char* ip_str = strip_ipv4_mapped_prefix(raw_ip);
        char addr_str[64];
        SDL_snprintf(addr_str, sizeof(addr_str), "%s:%d", ip_str, (int)dgram->port);

        // Cross-IP normalization: if this packet arrived on the expected remote
        // port but from a different IP (IPv4↔IPv6 mismatch), rewrite the source
        // address to match what GekkoNet was configured with. This makes
        // GekkoNet's string-based address matching work across address families.
        const char* final_addr = addr_str;
        if (expected_remote_port != 0 &&
            dgram->port == expected_remote_port &&
            SDL_strcmp(addr_str, expected_remote_addr) != 0 &&
            expected_remote_addr[0] != '\0') {
            if (!cross_ip_logged) {
                SDL_Log("[NetAdapter] Cross-IP: rewriting %s → %s", addr_str, expected_remote_addr);
                cross_ip_logged = true;
            }
            final_addr = expected_remote_addr;
        }

        GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));
        size_t addr_len = SDL_strlen(final_addr);
        res->addr.data = SDL_malloc(addr_len + 1);
        SDL_strlcpy((char*)res->addr.data, final_addr, addr_len + 1);
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

void SDLNetAdapter_SetExpectedRemote(const char* addr_str) {
    if (addr_str && addr_str[0]) {
        SDL_strlcpy(expected_remote_addr, addr_str, sizeof(expected_remote_addr));
        char ip[64];
        int port = 0;
        parse_addr_str(addr_str, ip, sizeof(ip), &port);
        expected_remote_port = (Uint16)port;
        cross_ip_logged = false;
        SDL_Log("[NetAdapter] Expected remote: %s (port %u)", expected_remote_addr, expected_remote_port);
    } else {
        expected_remote_addr[0] = '\0';
        expected_remote_port = 0;
    }
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
    expected_remote_addr[0] = '\0';
    expected_remote_port = 0;
    cross_ip_logged = false;
}
