// Use standalone configuration.h to avoid structs.h/Winsock typedef conflicts.
#include "discovery.h"
#include "configuration.h"
#include "identity.h"
#include "port/config/config.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h> // GetAdaptersAddresses — for per-interface broadcast
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#endif
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h> // getifaddrs — for per-interface broadcast
// IFF_UP / IFF_BROADCAST live in <net/if.h> on glibc but the header can
// conflict with <linux/if.h> on some toolchains.  Define fallbacks.
#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_BROADCAST
#define IFF_BROADCAST 0x2
#endif
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

    // Generate a cryptographically strong unique instance ID.
    // SDL_rand_bits() returns a full 32-bit random value via SDL's CSPRNG,
    // which is seeded independently per process — no address-XOR collision
    // risk even when two instances of the same binary run simultaneously on
    // the same machine (same image base, same static addresses).
    local_instance_id = SDL_rand_bits();
    if (local_instance_id == 0)
        local_instance_id = 1; // Avoid 0 (reserved as "no target")

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

    // Broadcast — send directed subnet broadcasts to each local interface.
    // 255.255.255.255 on Windows only hits the default-route interface, so
    // machines on other adapters (e.g. Pi4 on Ethernet while desktop default
    // is WiFi) never receive beacons. Directed broadcasts fix this.
    if (broadcast_sock >= 0 && (now - last_broadcast_ticks >= BROADCAST_INTERVAL_MS || last_broadcast_ticks == 0)) {
        char beacon_data[256];
        bool auto_now = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
        
        const char *room_code = rmlui_casual_lobby_get_room_code();
        if (room_code && room_code[0] != '\0') {
            auto_now = false;
        }

        // Include display name in beacon for casual lobby LAN detection.
        // Sanitize pipe characters to avoid breaking the beacon parser.
        const char* raw_name = Identity_GetDisplayName();
        char safe_name[32];
        snprintf(safe_name, sizeof(safe_name), "%s", raw_name ? raw_name : "");
        for (int i = 0; safe_name[i]; i++) {
            if (safe_name[i] == '|')
                safe_name[i] = '_';
        }

        snprintf(beacon_data,
                 sizeof(beacon_data),
                 "3SX_LOBBY|%u|%d|%d|%u|%hu|%s",
                 local_instance_id,
                 auto_now ? 1 : 0,
                 local_ready ? 1 : 0,
                 local_challenge_target,
                 configuration.netplay.port,
                 safe_name);
        int beacon_len = (int)strlen(beacon_data);

        struct sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(DISCOVERY_PORT);

        int sent_count = 0;

#ifdef _WIN32
        // Enumerate adapters and compute directed broadcast for each IPv4 unicast address
        ULONG buf_size = 15000;
        IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(buf_size);
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        ULONG ret = GetAdaptersAddresses(AF_INET, flags, NULL, addrs, &buf_size);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(addrs);
            addrs = (IP_ADAPTER_ADDRESSES*)malloc(buf_size);
            ret = GetAdaptersAddresses(AF_INET, flags, NULL, addrs, &buf_size);
        }
        if (ret == NO_ERROR) {
            for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
                if (a->OperStatus != IfOperStatusUp)
                    continue;
                for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next) {
                    struct sockaddr_in* sa = (struct sockaddr_in*)u->Address.lpSockaddr;
                    if (sa->sin_family != AF_INET)
                        continue;
                    // Skip loopback (127.x.x.x)
                    if ((ntohl(sa->sin_addr.s_addr) >> 24) == 127)
                        continue;
                    // Compute directed broadcast: addr | ~mask
                    ULONG prefix = u->OnLinkPrefixLength; // e.g. 24
                    if (prefix == 0 || prefix > 31)
                        continue;
                    uint32_t mask = htonl(0xFFFFFFFF << (32 - prefix));
                    uint32_t bcast = sa->sin_addr.s_addr | ~mask;
                    broadcast_addr.sin_addr.s_addr = bcast;
                    sendto(broadcast_sock,
                           beacon_data,
                           beacon_len,
                           0,
                           (struct sockaddr*)&broadcast_addr,
                           sizeof(broadcast_addr));
                    sent_count++;
                }
            }
        }
        free(addrs);
#else
        // POSIX: use getifaddrs to get each interface's broadcast address
        struct ifaddrs* ifap = NULL;
        if (getifaddrs(&ifap) == 0) {
            for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;
                if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_BROADCAST))
                    continue;
                if (!ifa->ifa_broadaddr)
                    continue;
                struct sockaddr_in* brd = (struct sockaddr_in*)ifa->ifa_broadaddr;
                broadcast_addr.sin_addr = brd->sin_addr;
                sendto(broadcast_sock,
                       beacon_data,
                       beacon_len,
                       0,
                       (struct sockaddr*)&broadcast_addr,
                       sizeof(broadcast_addr));
                sent_count++;
            }
            freeifaddrs(ifap);
        }
#endif

        // Fallback: if no interfaces were enumerated, use global broadcast
        if (sent_count == 0) {
            broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(
                broadcast_sock, beacon_data, beacon_len, 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        }

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
            unsigned short peer_port = 50000; // fallback for old beacons
            char peer_display_name[32] = { 0 };
            int fields = sscanf(buffer,
                                "3SX_LOBBY|%u|%d|%d|%u|%hu",
                                &peer_instance_id,
                                &peer_auto,
                                &peer_rdy,
                                &peer_challenge,
                                &peer_port);
            // Parse the 7th field (display_name) manually — sscanf %s
            // stops at whitespace which is fine, but manual extraction
            // is more robust for names with spaces in the future.
            if (fields >= 5) {
                // Find the 6th pipe to get the display name
                const char* p = buffer;
                int pipes = 0;
                while (*p && pipes < 6) {
                    if (*p == '|')
                        pipes++;
                    p++;
                }
                if (pipes == 6 && *p) {
                    snprintf(peer_display_name, sizeof(peer_display_name), "%s", p);
                    // Trim trailing whitespace/newlines
                    size_t dlen = strlen(peer_display_name);
                    while (dlen > 0 && (peer_display_name[dlen - 1] == '\n' || peer_display_name[dlen - 1] == '\r'))
                        peer_display_name[--dlen] = '\0';
                }
            }
            if (fields >= 1) {
                // Ignore our own broadcast
                if (peer_instance_id != local_instance_id) {
                    char ip_str[64];
                    SDL_strlcpy(ip_str, inet_ntoa(sender_addr.sin_addr), sizeof(ip_str));

                    bool found = false;
                    for (int i = 0; i < num_peers; i++) {
                        if (peers[i].instance_id == peer_instance_id) {
                            SDL_strlcpy(peers[i].ip, ip_str, sizeof(peers[i].ip)); // Update IP in case it changed
                            peers[i].port = peer_port;
                            snprintf(peers[i].name, sizeof(peers[i].name), "%s:%hu", ip_str, peer_port);
                            peers[i].last_seen_ticks = now;
                            peers[i].wants_auto_connect = (peer_auto == 1);
                            peers[i].peer_ready = (peer_rdy == 1);
                            peers[i].is_challenging_me = (peer_challenge == local_instance_id);
                            if (peer_display_name[0])
                                SDL_strlcpy(peers[i].display_name, peer_display_name, sizeof(peers[i].display_name));
                            found = true;
                            break;
                        }
                    }
                    if (!found && num_peers < MAX_PEERS) {
                        NetplayDiscoveredPeer* p = &peers[num_peers++];
                        SDL_strlcpy(p->ip, ip_str, sizeof(p->ip));
                        p->instance_id = peer_instance_id;
                        p->wants_auto_connect = (peer_auto == 1);
                        p->peer_ready = (peer_rdy == 1);
                        p->is_challenging_me = (peer_challenge == local_instance_id);
                        snprintf(p->name, sizeof(p->name), "%s:%hu", ip_str, peer_port);
                        snprintf(p->display_name, sizeof(p->display_name), "%s", peer_display_name);
                        p->port = peer_port;
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

uint32_t Discovery_GetChallengeTarget(void) {
    return local_challenge_target;
}

uint32_t Discovery_GetLocalInstanceID(void) {
    return local_instance_id;
}
