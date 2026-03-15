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

// Platform headers for getaddrinfo with AF_INET hint (force IPv4 resolution).
// SDL3_Net's NET_ResolveHostname returns IPv6 first on dual-stack systems,
// but the netplay stack uses uint32_t IPs (IPv4 only).
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

// Resolve a hostname to an IPv4 address string (e.g. "142.250.189.127").
// Returns true on success, writes result to ipv4_buf.
static bool resolve_hostname_ipv4(const char* hostname, char* ipv4_buf, int buf_size) {
    struct addrinfo hints = { 0 }, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0 || !res)
        return false;
    struct sockaddr_in* sin = (struct sockaddr_in*)res->ai_addr;
    inet_ntop(AF_INET, &sin->sin_addr, ipv4_buf, buf_size);
    freeaddrinfo(res);
    return true;
}

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
// TODO: Add IPv6 support (family=0x02) — requires changing IP representation
//       across the entire netplay stack from uint32_t to a dual-stack type.
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
            *out_port = SDL_Swap16BE(xport ^ (uint16_t)(STUN_MAGIC_COOKIE >> 16));
            *out_ip = SDL_Swap32BE(xaddr ^ STUN_MAGIC_COOKIE);
            return true;
        }

        if (attr_type == STUN_ATTR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t family = buf[offset + 1];
            if (family != 0x01) {
                offset += (attr_len + 3) & ~3;
                continue;
            }
            *out_port = SDL_Swap16BE(((uint16_t)buf[offset + 2] << 8) | buf[offset + 3]);
            *out_ip = SDL_Swap32BE(((uint32_t)buf[offset + 4] << 24) | ((uint32_t)buf[offset + 5] << 16) |
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
    result->socket = NULL;

    // Bind to 0.0.0.0 explicitly to force IPv4.
    // NULL creates a dual-stack socket on Windows, which causes STUN servers
    // to return IPv6 XOR-MAPPED-ADDRESS that our parser doesn't handle.
    NET_Address* bind_addr = NET_ResolveHostname("0.0.0.0");
    if (bind_addr) {
        // Wait for resolve (instant for numeric addresses, but API is async)
        int wait = 0;
        while (NET_GetAddressStatus(bind_addr) == NET_WAITING && wait < 100) {
            SDL_Delay(1);
            wait++;
        }
    }
    NET_DatagramSocket* sock = NET_CreateDatagramSocket(bind_addr, local_port);
    if (bind_addr)
        NET_UnrefAddress(bind_addr);
    if (!sock) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to create UDP socket: %s", SDL_GetError());
        return false;
    }

    // Force IPv4 resolution for the STUN server.
    // SDL3_Net's NET_ResolveHostname returns IPv6 on dual-stack systems,
    // but our STUN parser and netplay stack only handle IPv4.
    char stun_ipv4[64];
    if (!resolve_hostname_ipv4("stun.l.google.com", stun_ipv4, sizeof(stun_ipv4))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve stun.l.google.com to IPv4");
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    NET_Address* stun_addr = NET_ResolveHostname(stun_ipv4);
    if (!stun_addr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve %s via SDL3_Net: %s", stun_ipv4, SDL_GetError());
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to resolve %s", stun_ipv4);
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
    uint32_t ip = 0;
    uint16_t port = 0;
    if (!parse_binding_response((const uint8_t*)dgram->buf, dgram->buflen, transaction_id, &ip, &port)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "STUN: Failed to parse binding response");
        NET_DestroyDatagram(dgram);
        NET_DestroyDatagramSocket(sock);
        return false;
    }

    // the mapped port is the actual port we use
    result->local_port = SDL_Swap16BE(port);

    result->public_ip = ip;
    result->public_port = port;
    result->socket = sock; // Keep open for hole punching!

    NET_DestroyDatagram(dgram);

    SDL_Log("STUN: Discovered public endpoint (local port %u)", result->local_port);

    return true;
}

bool Stun_HolePunch(StunResult* local, uint32_t* peer_ip, uint16_t* peer_port, int punch_duration_ms,
                    SDL_AtomicInt* cancel_flag) {
    if (!local || !local->socket || !peer_ip || !peer_port)
        return false;

    NET_DatagramSocket* sock = local->socket;

    // Punch packet — a small identifiable payload
    const char punch_msg[] = "3SX_PUNCH";
    const int punch_interval_ms = 200; // Send every 200ms

    // Convert peer_ip/peer_port back to string to resolve with SDL3_net
    char peer_ip_str[32];
    Stun_FormatIP(*peer_ip, peer_ip_str, sizeof(peer_ip_str));

    NET_Address* peer = NET_ResolveHostname(peer_ip_str);
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
                uint8_t octets[4];
                if (SDL_sscanf(received_ip, "%hhu.%hhu.%hhu.%hhu", &octets[0], &octets[1], &octets[2], &octets[3]) ==
                    4) {
                    uint32_t new_ip;
                    memcpy(&new_ip, octets, 4);
                    *peer_ip = new_ip;
                }

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
