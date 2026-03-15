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

#include <SDL3_net/SDL_net.h>

// Platform headers for inet_ntop
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif


// STUN message types (RFC 5389)
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_MAGIC_COOKIE 0x2112A442

// STUN attribute types
#define STUN_ATTR_MAPPED_ADDRESS 0x0001
#define STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020

void Stun_EncodeEndpoint(const char* ip, uint16_t port, char* out_code) {
    if (!ip || !out_code)
        return;
    snprintf(out_code, 64, "%s|%u", ip, port);
}

bool Stun_DecodeEndpoint(const char* code, char* out_ip, uint16_t* out_port) {
    if (!code || !out_ip || !out_port)
        return false;
    
    char temp[64];
    SDL_strlcpy(temp, code, sizeof(temp));
    
    char* sep = strchr(temp, '|');
    if (!sep)
        return false;
        
    *sep = '\0';
    SDL_strlcpy(out_ip, temp, 64);
    *out_port = (uint16_t)atoi(sep + 1);
    return true;
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
static bool parse_binding_response(const uint8_t* buf, int len, const uint8_t* transaction_id, char* out_ip,
                                   int ip_buf_size, uint16_t* out_port) {
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
            uint16_t xport = ((uint16_t)buf[offset + 2] << 8) | buf[offset + 3];
            *out_port = SDL_Swap16BE(xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16));

            if (family == 0x01) {              // IPv4
                uint32_t xaddr = ((uint32_t)buf[offset + 4] << 24) | ((uint32_t)buf[offset + 5] << 16) |
                                 ((uint32_t)buf[offset + 6] << 8) | buf[offset + 7];
                uint32_t decoded_ip = SDL_Swap32BE(xaddr ^ STUN_MAGIC_COOKIE);
                uint8_t* b = (uint8_t*)&decoded_ip;
                snprintf(out_ip, ip_buf_size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
                return true;
            } else if (family == 0x02 && attr_len >= 20) { // IPv6
                uint8_t decoded_ipv6[16];
                // XOR first 4 bytes with magic cookie
                decoded_ipv6[0] = buf[offset + 4] ^ (uint8_t)(STUN_MAGIC_COOKIE >> 24);
                decoded_ipv6[1] = buf[offset + 5] ^ (uint8_t)(STUN_MAGIC_COOKIE >> 16);
                decoded_ipv6[2] = buf[offset + 6] ^ (uint8_t)(STUN_MAGIC_COOKIE >> 8);
                decoded_ipv6[3] = buf[offset + 7] ^ (uint8_t)(STUN_MAGIC_COOKIE);
                // XOR remaining 12 bytes with transaction ID
                for (int i = 0; i < 12; i++) {
                    decoded_ipv6[4 + i] = buf[offset + 8 + i] ^ transaction_id[i];
                }
                
                // Format directly via inet_ntop
#ifdef _WIN32
                // We use Windows-compatible inet_ntop mapping (requires ws2tcpip.h which is included)
                inet_ntop(AF_INET6, decoded_ipv6, out_ip, ip_buf_size);
#else
                inet_ntop(AF_INET6, decoded_ipv6, out_ip, ip_buf_size);
#endif
                return true;
            }
        }

        if (attr_type == STUN_ATTR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t family = buf[offset + 1];
            *out_port = SDL_Swap16BE(((uint16_t)buf[offset + 2] << 8) | buf[offset + 3]);
            
            if (family == 0x01) {
                uint32_t decoded_ip = SDL_Swap32BE(((uint32_t)buf[offset + 4] << 24) | ((uint32_t)buf[offset + 5] << 16) |
                                       ((uint32_t)buf[offset + 6] << 8) | buf[offset + 7]);
                uint8_t* b = (uint8_t*)&decoded_ip;
                snprintf(out_ip, ip_buf_size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
                return true;
            } else if (family == 0x02 && attr_len >= 20) {
                #ifdef _WIN32
                inet_ntop(AF_INET6, &buf[offset + 4], out_ip, ip_buf_size);
                #else
                inet_ntop(AF_INET6, &buf[offset + 4], out_ip, ip_buf_size);
                #endif
                return true;
            }
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
    result->socket = NULL;

    // Bind to NULL explicitly to allow dual-stack dual-stack sockets.
    NET_Address* bind_addr = NULL;
    NET_DatagramSocket* sock = NET_CreateDatagramSocket(bind_addr, local_port);
    if (!sock) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to create UDP socket: %s", SDL_GetError());
        return false;
    }

    // Resolve via SDL3_Net natively.
    char stun_host[] = "stun.l.google.com";
    NET_Address* stun_addr = NET_ResolveHostname(stun_host);
    if (!stun_addr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve %s via SDL3_Net: %s", stun_host, SDL_GetError());
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // Spin-wait for resolve (instant for numeric IPs, but API is async):
    int wait_attempts = 0;
    while (NET_GetAddressStatus(stun_addr) == NET_WAITING && wait_attempts < 100) {
        SDL_Delay(1);
        wait_attempts++;
    }

    if (NET_GetAddressStatus(stun_addr) != NET_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve %s", stun_host);
        NET_UnrefAddress(stun_addr);
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // Build and send STUN request
    uint8_t request[20];
    uint8_t transaction_id[12];
    build_binding_request(request, transaction_id);

    if (!NET_SendDatagram(sock, stun_addr, 19302, request, 20)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to send binding request: %s", SDL_GetError());
        NET_UnrefAddress(stun_addr);
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // Receive response (retry up to 3 times)
    NET_Datagram* dgram = NULL;
    for (int attempt = 0; attempt < 3 && !dgram; attempt++) {
        // Poll with timeout for response:
        for (int poll = 0; poll < 30 && !dgram; poll++) {
            NET_ReceiveDatagram(sock, &dgram);
            if (!dgram)
                SDL_Delay(100);
        }

        if (!dgram) {
            // Resend on timeout
            NET_SendDatagram(sock, stun_addr, 19302, request, 20);
        }
    }

    NET_UnrefAddress(stun_addr);

    if (!dgram) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: No response received");
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // Parse response
    char ip[64] = {0};
    uint16_t port = 0;
    if (!parse_binding_response((const uint8_t*)dgram->buf, dgram->buflen, transaction_id, ip, sizeof(ip), &port)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to parse binding response");
        NET_DestroyDatagram(dgram);
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // the mapped port is the actual port we use
    result->local_port = SDL_Swap16BE(port);

    SDL_strlcpy(result->public_ip, ip, sizeof(result->public_ip));
    result->public_port = port;
    result->socket = sock; // Keep open for hole punching!

    NET_DestroyDatagram(dgram);

    SDL_Log("STUN: Discovered public endpoint (local port %u)", result->local_port);

    return true;
}

bool Stun_HolePunch(StunResult* local, char* peer_ip, uint16_t* peer_port, int punch_duration_ms,
                    SDL_AtomicInt* cancel_flag) {
    if (!local || !local->socket || !peer_ip || !peer_port)
        return false;

    NET_DatagramSocket* sock = local->socket;

    // Punch packet — a small identifiable payload
    const char punch_msg[] = "3SX_PUNCH";
    const int punch_interval_ms = 200; // Send every 200ms

    NET_Address* peer = NET_ResolveHostname(peer_ip);
    if (!peer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve peer IP");
        return false;
    }

    int wait_attempts = 0;
    while (NET_GetAddressStatus(peer) == NET_WAITING && wait_attempts < 300) {
        SDL_Delay(10);
        wait_attempts++;
    }

    if (NET_GetAddressStatus(peer) != NET_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve peer IP");
        NET_UnrefAddress(peer);
        return false;
    }

    SDL_Log("STUN: Hole punching for %dms...", punch_duration_ms);

    uint32_t start = SDL_GetTicks();
    uint32_t last_send = 0;
    bool received_response = false;

    uint16_t local_peer_port = SDL_Swap16BE(*peer_port);

    while ((int)(SDL_GetTicks() - start) < punch_duration_ms) {
        // Check for cancellation
        if (cancel_flag && SDL_GetAtomicInt(cancel_flag)) {
            SDL_Log("STUN: Hole punch cancelled by caller");
            NET_UnrefAddress(peer);
            return false;
        }
        uint32_t now = SDL_GetTicks();

        // Send punch packet periodically
        if (now - last_send >= (uint32_t)punch_interval_ms || last_send == 0) {
            if (!NET_SendDatagram(sock, peer, local_peer_port, punch_msg, strlen(punch_msg))) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "STUN: Hole punch send failed: %s", SDL_GetError());
            }
            last_send = now;
        }

        // Try to receive from peer
        NET_Datagram* dgram = NULL;
        NET_ReceiveDatagram(sock, &dgram);

        if (dgram) {
            // Check if it's a punch from our expected peer.
            // NOTE: NET_GetAddressString returns a pointer to an internal
            // static buffer — must copy one result before the second call.
            char recv_addr[64];
            SDL_strlcpy(recv_addr, NET_GetAddressString(dgram->addr), sizeof(recv_addr));
            if (strcmp(recv_addr, NET_GetAddressString(peer)) == 0 && dgram->buflen == strlen(punch_msg) &&
                strncmp((char*)dgram->buf, punch_msg, dgram->buflen) == 0) {
                SDL_Log("STUN: Hole punch SUCCESS — received response from peer");
                received_response = true;

                // Update with actual received endpoint (fixes Symmetric NAT port/IP translation)
                *peer_port = SDL_Swap16BE(dgram->port);

                // Update peer_ip from received address (Symmetric NAT may change it)
                const char* received_ip = NET_GetAddressString(dgram->addr);
                SDL_strlcpy(peer_ip, received_ip, 64);

                // Send a few more punches to ensure the peer also receives ours
                for (int i = 0; i < 3; i++) {
                    NET_SendDatagram(sock, peer, local_peer_port, punch_msg, strlen(punch_msg));
                    SDL_Delay(50);
                }
                NET_DestroyDatagram(dgram);
                break;
            }
            NET_DestroyDatagram(dgram);
        } else {
            SDL_Delay(10); // Don't spin too hot if no data
        }
    }

    if (!received_response) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "STUN: Hole punch timed out after %dms. "
                    "Peer may be behind Symmetric NAT.",
                    punch_duration_ms);
    }

    NET_UnrefAddress(peer);

    return received_response;
}

void Stun_CloseSocket(StunResult* result) {
    if (result && result->socket != NULL) {
        NET_DestroyDatagramSocket(result->socket);
        result->socket = NULL;
    }
}
