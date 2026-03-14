/**
 * @file ping_probe.c
 * @brief P2P UDP ping probe for measuring true peer-to-peer RTT in the lobby.
 *
 * Sends lightweight UDP ping packets directly to lobby peers and measures
 * round-trip time. Replaces the inaccurate HTTP-based triangulated RTT estimate.
 *
 * Packet format (16 bytes):
 *   [0..7]   magic   "3SX_PING" or "3SX_PONG"
 *   [8..9]   seq     uint16 LE — sequence number
 *   [10..13] ts      uint32 LE — sender's SDL_GetTicks() at send time
 *   [14..15] pad     zeroes
 */
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include "ping_probe.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define PING_MAGIC      "3SX_PING"
#define PONG_MAGIC      "3SX_PONG"
#define MAGIC_LEN       8
#define PROBE_PKT_SIZE  16

#define MAX_PROBE_PEERS 16
#define PROBE_INTERVAL_MS 2000   /* Send one probe per peer every 2s */
#define MISS_TIMEOUT_COUNT 5     /* Mark unreachable after 5 misses (10s) */

#define SMOOTHING_ALPHA_NUM 1    /* α = 1/4 = 0.25 (integer math) */
#define SMOOTHING_ALPHA_DEN 4

/* ------------------------------------------------------------------ */
/* Per-peer state                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char     player_id[64];
    uint32_t ip;               /* Network byte order */
    uint16_t port;             /* Network byte order */
    bool     active;

    uint16_t next_seq;
    uint32_t last_send_ticks;  /* SDL_GetTicks() when last probe was sent */

    /* Measurement */
    int      smoothed_rtt;     /* Exponential average in ms, -1 = no data */
    int      consecutive_miss; /* Incremented each send, reset on pong */
    bool     ever_reached;     /* True once we get at least one pong */
} ProbePeer;

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

static int          s_socket_fd = -1;
static ProbePeer    s_peers[MAX_PROBE_PEERS];
static int          s_peer_count = 0;
static int          s_next_send_idx = 0; /* Round-robin index for staggered sends */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static ProbePeer* find_peer(const char* player_id) {
    for (int i = 0; i < s_peer_count; i++) {
        if (s_peers[i].active && strcmp(s_peers[i].player_id, player_id) == 0)
            return &s_peers[i];
    }
    return NULL;
}

static ProbePeer* find_peer_by_addr(uint32_t ip, uint16_t port) {
    for (int i = 0; i < s_peer_count; i++) {
        if (s_peers[i].active && s_peers[i].ip == ip && s_peers[i].port == port)
            return &s_peers[i];
    }
    /* Fallback: match by IP only (port may differ due to NAT remapping) */
    for (int i = 0; i < s_peer_count; i++) {
        if (s_peers[i].active && s_peers[i].ip == ip)
            return &s_peers[i];
    }
    return NULL;
}

static void build_probe(uint8_t* buf, const char* magic, uint16_t seq, uint32_t ts) {
    memcpy(buf, magic, MAGIC_LEN);
    memcpy(buf + 8, &seq, 2);
    memcpy(buf + 10, &ts, 4);
    buf[14] = 0;
    buf[15] = 0;
}

static void send_probe(ProbePeer* peer) {
    if (s_socket_fd < 0)
        return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = peer->ip;
    addr.sin_port = peer->port;

    uint32_t now = SDL_GetTicks();
    uint8_t pkt[PROBE_PKT_SIZE];
    build_probe(pkt, PING_MAGIC, peer->next_seq, now);

    sendto(s_socket_fd, (const char*)pkt, PROBE_PKT_SIZE, 0,
           (struct sockaddr*)&addr, sizeof(addr));

    peer->next_seq++;
    peer->last_send_ticks = now;
    peer->consecutive_miss++;
}

static void send_pong(const struct sockaddr_in* to, uint16_t seq, uint32_t ts) {
    if (s_socket_fd < 0)
        return;

    uint8_t pkt[PROBE_PKT_SIZE];
    build_probe(pkt, PONG_MAGIC, seq, ts);

    sendto(s_socket_fd, (const char*)pkt, PROBE_PKT_SIZE, 0,
           (const struct sockaddr*)to, sizeof(*to));
}

static void receive_probes(void) {
    if (s_socket_fd < 0)
        return;

    uint32_t now = SDL_GetTicks();

    /* Drain up to 64 packets per update to avoid starvation */
    for (int pkt_i = 0; pkt_i < 64; pkt_i++) {
        uint8_t buf[64];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);

        int bytes = recvfrom(s_socket_fd, (char*)buf, sizeof(buf), 0,
                             (struct sockaddr*)&from, &from_len);
        if (bytes <= 0)
            break;

        /* Only process our probe packets (16 bytes with known magic) */
        if (bytes < PROBE_PKT_SIZE)
            continue;

        if (memcmp(buf, PING_MAGIC, MAGIC_LEN) == 0) {
            /* Incoming ping from a peer — echo it back as pong */
            uint16_t seq;
            uint32_t ts;
            memcpy(&seq, buf + 8, 2);
            memcpy(&ts, buf + 10, 4);
            send_pong(&from, seq, ts);
        } else if (memcmp(buf, PONG_MAGIC, MAGIC_LEN) == 0) {
            /* Incoming pong — compute RTT */
            uint32_t ts;
            memcpy(&ts, buf + 10, 4);

            int rtt = (int)(now - ts);
            if (rtt < 0) rtt = 0;
            if (rtt > 9999) rtt = 9999;

            ProbePeer* peer = find_peer_by_addr(from.sin_addr.s_addr, from.sin_port);
            if (peer) {
                peer->consecutive_miss = 0;
                peer->ever_reached = true;

                if (peer->smoothed_rtt < 0) {
                    /* First sample — use as-is */
                    peer->smoothed_rtt = rtt;
                } else {
                    /* Exponential smoothing: avg = α*sample + (1-α)*avg */
                    peer->smoothed_rtt = (SMOOTHING_ALPHA_NUM * rtt +
                                          (SMOOTHING_ALPHA_DEN - SMOOTHING_ALPHA_NUM) * peer->smoothed_rtt)
                                         / SMOOTHING_ALPHA_DEN;
                }

                SDL_Log("[PingProbe] %s RTT=%dms (smoothed=%dms)",
                        peer->player_id, rtt, peer->smoothed_rtt);
            }
        }
        /* Silently ignore unrecognized packets (e.g. 3SX_PUNCH from hole punch) */
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void PingProbe_Init(int socket_fd) {
    s_socket_fd = socket_fd;
    s_peer_count = 0;
    s_next_send_idx = 0;
    memset(s_peers, 0, sizeof(s_peers));
    SDL_Log("[PingProbe] Initialized with socket fd=%d", socket_fd);
}

void PingProbe_Shutdown(void) {
    s_socket_fd = -1;
    s_peer_count = 0;
    s_next_send_idx = 0;
    memset(s_peers, 0, sizeof(s_peers));
    SDL_Log("[PingProbe] Shutdown");
}

void PingProbe_AddPeer(uint32_t ip, uint16_t port, const char* player_id) {
    if (!player_id || !player_id[0])
        return;

    /* Update existing peer if IP/port changed */
    ProbePeer* existing = find_peer(player_id);
    if (existing) {
        existing->ip = ip;
        existing->port = port;
        return;
    }

    /* Find a free slot */
    int slot = -1;
    for (int i = 0; i < MAX_PROBE_PEERS; i++) {
        if (!s_peers[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        /* All slots full — evict the least reachable peer */
        int worst = 0;
        for (int i = 1; i < MAX_PROBE_PEERS; i++) {
            if (s_peers[i].consecutive_miss > s_peers[worst].consecutive_miss)
                worst = i;
        }
        slot = worst;
    }

    ProbePeer* p = &s_peers[slot];
    memset(p, 0, sizeof(*p));
    snprintf(p->player_id, sizeof(p->player_id), "%s", player_id);
    p->ip = ip;
    p->port = port;
    p->active = true;
    p->smoothed_rtt = -1;
    p->consecutive_miss = 0;
    p->ever_reached = false;
    p->next_seq = 0;
    p->last_send_ticks = 0;

    if (slot >= s_peer_count)
        s_peer_count = slot + 1;

    char ip_str[32];
    uint8_t* b = (uint8_t*)&ip;
    snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    SDL_Log("[PingProbe] Added peer %s at %s:%u", player_id, ip_str, ntohs(port));
}

void PingProbe_RemovePeer(const char* player_id) {
    ProbePeer* p = find_peer(player_id);
    if (p) {
        p->active = false;
        SDL_Log("[PingProbe] Removed peer %s", player_id);
    }
}

void PingProbe_ClearPeers(void) {
    for (int i = 0; i < MAX_PROBE_PEERS; i++)
        s_peers[i].active = false;
    s_peer_count = 0;
    s_next_send_idx = 0;
}

void PingProbe_Update(void) {
    if (s_socket_fd < 0)
        return;

    /* 1. Receive all incoming probes/pongs */
    receive_probes();

    /* 2. Send probes — one peer per update call to stagger traffic */
    uint32_t now = SDL_GetTicks();
    if (s_peer_count == 0)
        return;

    /* Find the next active peer to probe */
    for (int attempts = 0; attempts < s_peer_count; attempts++) {
        if (s_next_send_idx >= s_peer_count)
            s_next_send_idx = 0;

        ProbePeer* p = &s_peers[s_next_send_idx];
        s_next_send_idx++;

        if (!p->active)
            continue;

        /* Only send if enough time has passed since last probe to this peer */
        if (now - p->last_send_ticks >= PROBE_INTERVAL_MS || p->last_send_ticks == 0) {
            send_probe(p);

            /* Log unreachable detection */
            if (p->consecutive_miss >= MISS_TIMEOUT_COUNT && p->consecutive_miss == MISS_TIMEOUT_COUNT) {
                SDL_Log("[PingProbe] %s unreachable (%d consecutive misses)",
                        p->player_id, p->consecutive_miss);
            }
            break; /* Only send one probe per update */
        }
    }
}

int PingProbe_GetRTT(const char* player_id) {
    ProbePeer* p = find_peer(player_id);
    if (!p)
        return -1;
    return p->smoothed_rtt;
}

bool PingProbe_IsReachable(const char* player_id) {
    ProbePeer* p = find_peer(player_id);
    if (!p)
        return false;
    return p->ever_reached && p->consecutive_miss < MISS_TIMEOUT_COUNT;
}
