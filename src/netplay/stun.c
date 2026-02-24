/**
 * @file stun.c
 * @brief Minimal STUN client (RFC 5389) and endpoint encoder/decoder.
 *
 * Performs a STUN Binding Request to discover the public IP:port,
 * and provides 8-character Base64-like encoding for sharing endpoints.
 */
#ifndef _WIN32
#define _GNU_SOURCE // Must be before any includes for getaddrinfo/timeval
#endif
#include "stun.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define closesocket close
#endif

// STUN message types (RFC 5389)
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN attribute types
#define STUN_ATTR_MAPPED_ADDRESS 0x0001
#define STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020

// XOR obfuscation key for room codes (lightweight, not crypto)
#define CODE_XOR_KEY 0xA7

// Base64url-safe alphabet (no +/= confusion)
static const char CODE_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static uint8_t decode_char(char c) {
    if (c >= 'A' && c <= 'Z')
        return (uint8_t)(c - 'A');
    if (c >= 'a' && c <= 'z')
        return (uint8_t)(c - 'a' + 26);
    if (c >= '0' && c <= '9')
        return (uint8_t)(c - '0' + 52);
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return 0xFF; // Invalid
}

void Stun_EncodeEndpoint(uint32_t ip, uint16_t port, char* out_code) {
    // Pack into 6 bytes: 4 bytes IP + 2 bytes port (all network byte order)
    uint8_t raw[6];
    memcpy(&raw[0], &ip, 4);
    memcpy(&raw[4], &port, 2);

    // XOR obfuscate
    for (int i = 0; i < 6; i++) {
        raw[i] ^= CODE_XOR_KEY;
    }

    // Encode 6 bytes (48 bits) into 8 base64 characters (6 bits each)
    out_code[0] = CODE_ALPHABET[(raw[0] >> 2) & 0x3F];
    out_code[1] = CODE_ALPHABET[((raw[0] & 0x03) << 4) | ((raw[1] >> 4) & 0x0F)];
    out_code[2] = CODE_ALPHABET[((raw[1] & 0x0F) << 2) | ((raw[2] >> 6) & 0x03)];
    out_code[3] = CODE_ALPHABET[raw[2] & 0x3F];
    out_code[4] = CODE_ALPHABET[(raw[3] >> 2) & 0x3F];
    out_code[5] = CODE_ALPHABET[((raw[3] & 0x03) << 4) | ((raw[4] >> 4) & 0x0F)];
    out_code[6] = CODE_ALPHABET[((raw[4] & 0x0F) << 2) | ((raw[5] >> 6) & 0x03)];
    out_code[7] = CODE_ALPHABET[raw[5] & 0x3F];
    out_code[8] = '\0';
}

bool Stun_DecodeEndpoint(const char* code, uint32_t* out_ip, uint16_t* out_port) {
    if (strlen(code) != 8)
        return false;

    uint8_t vals[8];
    for (int i = 0; i < 8; i++) {
        vals[i] = decode_char(code[i]);
        if (vals[i] == 0xFF)
            return false;
    }

    // Decode 8 base64 chars (48 bits) back to 6 bytes
    uint8_t raw[6];
    raw[0] = (uint8_t)((vals[0] << 2) | (vals[1] >> 4));
    raw[1] = (uint8_t)((vals[1] << 4) | (vals[2] >> 2));
    raw[2] = (uint8_t)((vals[2] << 6) | vals[3]);
    raw[3] = (uint8_t)((vals[4] << 2) | (vals[5] >> 4));
    raw[4] = (uint8_t)((vals[5] << 4) | (vals[6] >> 2));
    raw[5] = (uint8_t)((vals[6] << 6) | vals[7]);

    // XOR de-obfuscate
    for (int i = 0; i < 6; i++) {
        raw[i] ^= CODE_XOR_KEY;
    }

    memcpy(out_ip, &raw[0], 4);
    memcpy(out_port, &raw[4], 2);
    return true;
}

void Stun_FormatIP(uint32_t ip_net, char* buf, int buf_size) {
    uint8_t* b = (uint8_t*)&ip_net;
    snprintf(buf, buf_size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

// Build a 20-byte STUN Binding Request (RFC 5389 §6)
static void build_binding_request(uint8_t* buf, uint8_t* transaction_id) {
    // Type: Binding Request (0x0001)
    buf[0] = 0x00;
    buf[1] = 0x01;
    // Length: 0 (no attributes)
    buf[2] = 0x00;
    buf[3] = 0x00;
    // Magic Cookie
    buf[4] = 0x21;
    buf[5] = 0x12;
    buf[6] = 0xA4;
    buf[7] = 0x42;
    // Transaction ID (12 random bytes)
    for (int i = 0; i < 12; i++) {
        transaction_id[i] = (uint8_t)(SDL_rand(256));
        buf[8 + i] = transaction_id[i];
    }
}

// Parse STUN Binding Response for XOR-MAPPED-ADDRESS or MAPPED-ADDRESS
static bool parse_binding_response(const uint8_t* buf, int len, const uint8_t* transaction_id, uint32_t* out_ip,
                                   uint16_t* out_port) {
    if (len < 20)
        return false;

    // Check message type = Binding Success Response
    uint16_t msg_type = ((uint16_t)buf[0] << 8) | buf[1];
    if (msg_type != STUN_BINDING_RESPONSE)
        return false;

    uint16_t msg_len = ((uint16_t)buf[2] << 8) | buf[3];
    if (20 + msg_len > len)
        return false;

    // Verify magic cookie
    uint32_t cookie = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    if (cookie != STUN_MAGIC_COOKIE)
        return false;

    // Verify transaction ID
    if (memcmp(&buf[8], transaction_id, 12) != 0)
        return false;

    // Walk attributes
    int offset = 20;
    while (offset + 4 <= 20 + (int)msg_len) {
        uint16_t attr_type = ((uint16_t)buf[offset] << 8) | buf[offset + 1];
        uint16_t attr_len = ((uint16_t)buf[offset + 2] << 8) | buf[offset + 3];
        offset += 4;

        if (offset + attr_len > 20 + (int)msg_len)
            break;

        if (attr_type == STUN_ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8) {
            // Family at offset+1 (skip reserved byte)
            uint8_t family = buf[offset + 1];
            if (family != 0x01) {              // IPv4 only
                offset += (attr_len + 3) & ~3; // pad to 4-byte boundary
                continue;
            }
            uint16_t xport = ((uint16_t)buf[offset + 2] << 8) | buf[offset + 3];
            uint32_t xaddr = ((uint32_t)buf[offset + 4] << 24) | ((uint32_t)buf[offset + 5] << 16) |
                             ((uint32_t)buf[offset + 6] << 8) | buf[offset + 7];

            // XOR with magic cookie
            *out_port = htons(xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16));
            *out_ip = htonl(xaddr ^ STUN_MAGIC_COOKIE);
            return true;
        }

        if (attr_type == STUN_ATTR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t family = buf[offset + 1];
            if (family != 0x01) {
                offset += (attr_len + 3) & ~3;
                continue;
            }
            *out_port = htons(((uint16_t)buf[offset + 2] << 8) | buf[offset + 3]);
            *out_ip = htonl(((uint32_t)buf[offset + 4] << 24) | ((uint32_t)buf[offset + 5] << 16) |
                            ((uint32_t)buf[offset + 6] << 8) | buf[offset + 7]);
            return true;
        }

        // Advance to next attribute (padded to 4-byte boundary)
        offset += (attr_len + 3) & ~3;
    }

    return false;
}

bool Stun_Discover(StunResult* result, uint16_t local_port) {
    if (!result)
        return false;
    memset(result, 0, sizeof(*result));
    result->socket_fd = -1;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // Resolve stun.l.google.com
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo("stun.l.google.com", "19302", &hints, &res) != 0 || !res) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve stun.l.google.com");
        return false;
    }

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }

    // Bind to local port (important: this is the port we'll use for hole punching)
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to bind to port %u", local_port);
        closesocket(sock);
        freeaddrinfo(res);
        return false;
    }

    // Capture actual OS-assigned local port (important when local_port == 0)
    {
        struct sockaddr_in bound_addr;
        socklen_t bound_len = sizeof(bound_addr);
        if (getsockname(sock, (struct sockaddr*)&bound_addr, &bound_len) == 0) {
            result->local_port = ntohs(bound_addr.sin_port);
        } else {
            result->local_port = local_port;
        }
    }

    // Set receive timeout (3 seconds)
#ifdef _WIN32
    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // Build and send STUN request
    uint8_t request[20];
    uint8_t transaction_id[12];
    build_binding_request(request, transaction_id);

    int sent = sendto(sock, (const char*)request, 20, 0, res->ai_addr, (socklen_t)res->ai_addrlen);
    freeaddrinfo(res);

    if (sent != 20) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to send binding request");
        closesocket(sock);
        return false;
    }

    // Receive response (retry up to 3 times)
    uint8_t response[512];
    int received = -1;
    for (int attempt = 0; attempt < 3; attempt++) {
        received = recvfrom(sock, (char*)response, sizeof(response), 0, NULL, NULL);
        if (received > 0)
            break;

        // Resend on timeout
        // Re-resolve in case DNS fails? No, just resend.
        struct addrinfo* res2 = NULL;
        if (getaddrinfo("stun.l.google.com", "19302", &hints, &res2) == 0 && res2) {
            sendto(sock, (const char*)request, 20, 0, res2->ai_addr, (socklen_t)res2->ai_addrlen);
            freeaddrinfo(res2);
        }
    }

    if (received <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: No response received");
        closesocket(sock);
        return false;
    }

    // Parse response
    uint32_t ip = 0;
    uint16_t port = 0;
    if (!parse_binding_response(response, received, transaction_id, &ip, &port)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to parse binding response");
        closesocket(sock);
        return false;
    }

    result->public_ip = ip;
    result->public_port = port;
    result->socket_fd = sock; // Keep open for hole punching!

    char ip_str[32];
    Stun_FormatIP(ip, ip_str, sizeof(ip_str));
    SDL_Log("STUN: Discovered public endpoint %s:%u (local port %u)", ip_str, ntohs(port), result->local_port);

    return true;
}

void Stun_SetNonBlocking(StunResult* result) {
    if (!result || result->socket_fd < 0)
        return;
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(result->socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(result->socket_fd, F_GETFL, 0);
    fcntl(result->socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool Stun_HolePunch(StunResult* local, uint32_t peer_ip, uint16_t peer_port, int punch_duration_ms) {
    if (!local || local->socket_fd < 0)
        return false;

    int sock = local->socket_fd;

    // Build peer address
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = peer_port;      // Already network byte order
    peer_addr.sin_addr.s_addr = peer_ip; // Already network byte order

    // Punch packet — a small identifiable payload
    const char punch_msg[] = "3SX_PUNCH";
    const int punch_interval_ms = 200; // Send every 200ms

    // Set short receive timeout for polling
#ifdef _WIN32
    DWORD timeout = 200;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    char ip_str[32];
    Stun_FormatIP(peer_ip, ip_str, sizeof(ip_str));
    SDL_Log("STUN: Hole punching to %s:%u for %dms...", ip_str, ntohs(peer_port), punch_duration_ms);

    uint32_t start = SDL_GetTicks();
    uint32_t last_send = 0;
    bool received_response = false;

    while ((int)(SDL_GetTicks() - start) < punch_duration_ms) {
        uint32_t now = SDL_GetTicks();

        // Send punch packet periodically
        if (now - last_send >= (uint32_t)punch_interval_ms || last_send == 0) {
            sendto(sock, punch_msg, (int)strlen(punch_msg), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
            last_send = now;
        }

        // Try to receive from peer
        char recv_buf[64];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int bytes = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr*)&from_addr, &from_len);

        if (bytes > 0) {
            recv_buf[bytes] = '\0';
            // Check if it's a punch from our expected peer
            if (from_addr.sin_addr.s_addr == peer_ip && strcmp(recv_buf, punch_msg) == 0) {
                SDL_Log("STUN: Hole punch SUCCESS — received response from peer");
                received_response = true;
                // Send a few more punches to ensure the peer also receives ours
                for (int i = 0; i < 3; i++) {
                    sendto(sock, punch_msg, (int)strlen(punch_msg), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
                    SDL_Delay(50);
                }
                break;
            }
        }
    }

    if (!received_response) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "STUN: Hole punch timed out after %dms. "
                    "Peer may be behind Symmetric NAT.",
                    punch_duration_ms);
    }

    return received_response;
}

void Stun_CloseSocket(StunResult* result) {
    if (result && result->socket_fd >= 0) {
        closesocket(result->socket_fd);
        result->socket_fd = -1;
    }
}

// --- Socket helpers for GekkoNet adapter ---

int Stun_SocketSendTo(int fd, const char* dest_endpoint, const char* data, int length) {
    if (fd < 0 || !dest_endpoint)
        return -1;

    // Parse "ip:port" string
    char addr_copy[128];
    size_t len = strlen(dest_endpoint);
    if (len >= sizeof(addr_copy))
        return -1;
    memcpy(addr_copy, dest_endpoint, len + 1);

    char* colon = strrchr(addr_copy, ':');
    if (!colon)
        return -1;
    *colon = '\0';
    unsigned short port = (unsigned short)atoi(colon + 1);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, addr_copy, &dest.sin_addr);

    return sendto(fd, data, length, 0, (struct sockaddr*)&dest, sizeof(dest));
}

int Stun_SocketRecvFrom(int fd, char* buf, int buf_size, char* from_endpoint, int endpoint_size) {
    if (fd < 0)
        return -1;

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int n = recvfrom(fd, buf, buf_size, 0, (struct sockaddr*)&from, &from_len);
    if (n <= 0)
        return n;

    // Format sender as "ip:port"
    char ip_str[32];
    inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
    snprintf(from_endpoint, endpoint_size, "%s:%u", ip_str, ntohs(from.sin_port));

    return n;
}

void Stun_SocketClose(int fd) {
    if (fd >= 0) {
        closesocket(fd);
    }
}
