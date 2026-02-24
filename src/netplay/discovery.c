#include "discovery.h"
#include "port/config.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif

#define DISCOVERY_PORT 7999
#define BROADCAST_INTERVAL_MS 500
#define MAX_PEERS 16

static uint32_t local_instance_id = 0;
static bool local_auto_connect = false;
static bool local_ready = false;
static int broadcast_sock = -1;
static int listen_sock = -1;
static uint32_t last_broadcast_ticks = 0;
static uint32_t local_challenge_target = 0; // 0 = no target

static NetplayDiscoveredPeer peers[MAX_PEERS];
static int num_peers = 0;

static void set_nonblocking(int sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

void Discovery_Init(bool auto_connect) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // Generate a random unique instance ID for self-identification
    local_instance_id = SDL_GetTicks() ^ (uint32_t)(uintptr_t)&local_instance_id;
    // Mix in some randomness
    local_instance_id = local_instance_id * 2654435761u + SDL_GetPerformanceCounter();
    if (local_instance_id == 0)
        local_instance_id = 1; // Avoid 0 (used as "no target")

    local_auto_connect = auto_connect;
    local_ready = false;
    local_challenge_target = 0;
    num_peers = 0;

    // Setup broadcast socket
    broadcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_sock >= 0) {
        int broadcast_enable = 1;
        setsockopt(broadcast_sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast_enable, sizeof(broadcast_enable));
    }

    // Setup listen socket
    listen_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sock >= 0) {
        int reuse = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse));
#endif

        struct sockaddr_in listen_addr;
        memset(&listen_addr, 0, sizeof(listen_addr));
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_port = htons(DISCOVERY_PORT);
        listen_addr.sin_addr.s_addr = INADDR_ANY;
        bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr));

        set_nonblocking(listen_sock);
    }

    last_broadcast_ticks = 0;
}

void Discovery_Shutdown() {
    if (broadcast_sock >= 0) {
        closesocket(broadcast_sock);
        broadcast_sock = -1;
    }
    if (listen_sock >= 0) {
        closesocket(listen_sock);
        listen_sock = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void Discovery_Update() {
    uint32_t now = SDL_GetTicks();

    // Broadcast
    if (broadcast_sock >= 0 && (now - last_broadcast_ticks >= BROADCAST_INTERVAL_MS || last_broadcast_ticks == 0)) {
        struct sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(DISCOVERY_PORT);
        broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

        char beacon_data[256];
        bool auto_now = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
        snprintf(beacon_data,
                 sizeof(beacon_data),
                 "3SX_LOBBY|%u|%d|%d|%u",
                 local_instance_id,
                 auto_now ? 1 : 0,
                 local_ready ? 1 : 0,
                 local_challenge_target);

        sendto(broadcast_sock,
               beacon_data,
               strlen(beacon_data),
               0,
               (struct sockaddr*)&broadcast_addr,
               sizeof(broadcast_addr));

        last_broadcast_ticks = now;
    }

    // Listen — drain all queued packets (important for same-machine testing
    // where self-broadcasts can starve the peer's beacon)
    if (listen_sock >= 0) {
        for (int pkt = 0; pkt < 32; pkt++) {
            char buffer[256];
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);

            int bytes =
                recvfrom(listen_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);

            if (bytes <= 0)
                break; // No more packets

            buffer[bytes] = '\0';
            unsigned int peer_instance_id = 0;
            int peer_auto = 0;
            int peer_rdy = 0;
            unsigned int peer_challenge = 0;
            if (sscanf(buffer, "3SX_LOBBY|%u|%d|%d|%u", &peer_instance_id, &peer_auto, &peer_rdy, &peer_challenge) >=
                1) {
                // Ignore our own broadcast
                if (peer_instance_id != local_instance_id) {
                    char ip_str[64];
                    strcpy(ip_str, inet_ntoa(sender_addr.sin_addr));

                    bool found = false;
                    for (int i = 0; i < num_peers; i++) {
                        if (peers[i].instance_id == peer_instance_id) {
                            strcpy(peers[i].ip, ip_str); // Update IP in case it changed
                            peers[i].last_seen_ticks = now;
                            peers[i].wants_auto_connect = (peer_auto == 1);
                            peers[i].peer_ready = (peer_rdy == 1);
                            peers[i].is_challenging_me = (peer_challenge == local_instance_id);
                            found = true;
                            break;
                        }
                    }
                    if (!found && num_peers < MAX_PEERS) {
                        NetplayDiscoveredPeer* p = &peers[num_peers++];
                        strcpy(p->ip, ip_str);
                        p->instance_id = peer_instance_id;
                        p->wants_auto_connect = (peer_auto == 1);
                        p->peer_ready = (peer_rdy == 1);
                        p->is_challenging_me = (peer_challenge == local_instance_id);
                        snprintf(p->name, sizeof(p->name), "%s", ip_str);
                        p->port = 50000;
                        p->last_seen_ticks = now;
                    }
                }
            }
        }
    }

    // Clean up stale peers (> 15 seconds old)
    for (int i = 0; i < num_peers;) {
        if (now - peers[i].last_seen_ticks > 15000) {
            peers[i] = peers[num_peers - 1];
            num_peers--;
        } else {
            i++;
        }
    }
}

int Discovery_GetPeers(NetplayDiscoveredPeer* out_peers, int max_peers) {
    int count = num_peers < max_peers ? num_peers : max_peers;
    for (int i = 0; i < count; i++) {
        out_peers[i] = peers[i];
    }
    return count;
}

void Discovery_SetReady(bool ready) {
    local_ready = ready;
}

void Discovery_SetChallengeTarget(uint32_t instance_id) {
    local_challenge_target = instance_id;
}

int Discovery_GetChallengeTarget(void) {
    return (int)local_challenge_target;
}

uint32_t Discovery_GetLocalInstanceID(void) {
    return local_instance_id;
}
