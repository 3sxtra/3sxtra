/**
 * @file lobby_server.c
 * @brief HTTP client for the 3SX lobby/matchmaking server.
 *
 * Communicates with the Node.js lobby server via HTTP/1.1 + HMAC-SHA256
 * request signing. Uses raw sockets — no libcurl dependency.
 *
 * HMAC implementation:
 *   - Windows: bcrypt.h (BCryptCreateHash / BCryptHashData / BCryptFinishHash)
 *   - Linux/macOS: embedded minimal SHA-256 (public domain)
 */
#ifndef _WIN32
#define _GNU_SOURCE // Must be before any includes for getaddrinfo/timeval
#endif
#include "lobby_server.h"
#include "port/config.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#ifndef BCRYPT_SHA256_ALGORITHM
#define BCRYPT_SHA256_ALGORITHM L"SHA256"
#endif
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define closesocket close
#endif

/* ======== Configuration ======== */

static char server_host[256] = { 0 };
static int server_port = 8080;
static char server_key[256] = { 0 };
static bool configured = false;

// Baked-in defaults (used if config.ini values are missing or empty)
#define DEFAULT_LOBBY_URL "http://152.67.75.184:3000"
#define DEFAULT_LOBBY_KEY "zqv0R11DN5DI8ZdRDhRmXzexQ2ciExSKXBvZSfXG0Z8="

void LobbyServer_Init(void) {
    const char* url_override = Config_GetString(CFG_KEY_LOBBY_SERVER_URL);
    const char* key_override = Config_GetString(CFG_KEY_LOBBY_SERVER_KEY);

    const char* url = (url_override && strlen(url_override) > 0) ? url_override : DEFAULT_LOBBY_URL;
    const char* key = (key_override && strlen(key_override) > 0) ? key_override : DEFAULT_LOBBY_KEY;

    configured = false;
    memset(server_host, 0, sizeof(server_host));
    server_port = 80; // default to 80 if no port specified
    memset(server_key, 0, sizeof(server_key));

    if (!url || !key || strlen(url) == 0 || strlen(key) == 0) {
        SDL_Log("LobbyServer: Not configured (missing URL or key)");
        return;
    }

    /* Parse URL: "http://host:port" */
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0)
        p += 7;

    const char* colon = strchr(p, ':');
    if (colon) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= sizeof(server_host))
            host_len = sizeof(server_host) - 1;
        memcpy(server_host, p, host_len);
        server_host[host_len] = '\0';
        server_port = atoi(colon + 1);
    } else {
        snprintf(server_host, sizeof(server_host), "%s", p);
        /* Strip trailing slash */
        size_t len = strlen(server_host);
        if (len > 0 && server_host[len - 1] == '/')
            server_host[len - 1] = '\0';
    }

    snprintf(server_key, sizeof(server_key), "%s", key);
    configured = true;
    SDL_Log("LobbyServer: Configured for %s:%d", server_host, server_port);
}

bool LobbyServer_IsConfigured(void) {
    return configured;
}

/* ======== Embedded SHA-256 (non-Windows) ======== */

#ifndef _WIN32

/* Minimal SHA-256 implementation — public domain.
 * Based on the reference by Brad Conte (B-Con). */

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define SHA_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA_EP0(x) (SHA_ROTR(x, 2) ^ SHA_ROTR(x, 13) ^ SHA_ROTR(x, 22))
#define SHA_EP1(x) (SHA_ROTR(x, 6) ^ SHA_ROTR(x, 11) ^ SHA_ROTR(x, 25))
#define SHA_SIG0(x) (SHA_ROTR(x, 7) ^ SHA_ROTR(x, 18) ^ ((x) >> 3))
#define SHA_SIG1(x) (SHA_ROTR(x, 17) ^ SHA_ROTR(x, 19) ^ ((x) >> 10))

static void sha256_transform(SHA256_CTX* ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    int i;
    for (i = 0; i < 16; ++i)
        m[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) | ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    for (; i < 64; ++i)
        m[i] = SHA_SIG1(m[i - 2]) + m[i - 7] + SHA_SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + SHA_EP1(e) + SHA_CH(e, f, g) + sha256_k[i] + m[i];
        t2 = SHA_EP0(a) + SHA_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX* ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX* ctx, uint8_t hash[32]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 8; ++i) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* HMAC-SHA256 */
static void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    uint8_t k_pad[64];
    SHA256_CTX ctx;
    uint8_t temp_hash[32];

    /* If key > 64 bytes, hash it first */
    uint8_t key_hash[32];
    if (key_len > 64) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, key_hash);
        key = key_hash;
        key_len = 32;
    }

    /* Inner pad */
    memset(k_pad, 0x36, 64);
    for (size_t i = 0; i < key_len; i++)
        k_pad[i] ^= key[i];

    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, 64);
    sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, temp_hash);

    /* Outer pad */
    memset(k_pad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++)
        k_pad[i] ^= key[i];

    sha256_init(&ctx);
    sha256_update(&ctx, k_pad, 64);
    sha256_update(&ctx, temp_hash, 32);
    sha256_final(&ctx, out);
}

#endif /* !_WIN32 */

/* ======== HMAC computation (cross-platform) ======== */

static void compute_hmac(const char* payload, char* out_hex, size_t hex_size) {
    uint8_t hash[32];

#ifdef _WIN32
    /* Windows: use BCrypt HMAC */
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    ULONG cbHashObject = 0, cbResult = 0;
    uint8_t* pbHashObject = NULL;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cbHashObject, sizeof(ULONG), &cbResult, 0);
    pbHashObject = (uint8_t*)malloc(cbHashObject);
    BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, (PUCHAR)server_key, (ULONG)strlen(server_key), 0);
    BCryptHashData(hHash, (PUCHAR)payload, (ULONG)strlen(payload), 0);
    BCryptFinishHash(hHash, hash, 32, 0);
    BCryptDestroyHash(hHash);
    free(pbHashObject);
    BCryptCloseAlgorithmProvider(hAlg, 0);
#else
    hmac_sha256((const uint8_t*)server_key, strlen(server_key), (const uint8_t*)payload, strlen(payload), hash);
#endif

    /* Convert to hex */
    for (int i = 0; i < 32 && (size_t)(i * 2 + 2) < hex_size; i++) {
        snprintf(out_hex + i * 2, 3, "%02x", hash[i]);
    }
    if (hex_size > 64)
        out_hex[64] = '\0';
    else if (hex_size > 0)
        out_hex[hex_size - 1] = '\0';
}

/* ======== HTTP client ======== */

#define HTTP_BUF_SIZE 4096

/* Open a TCP connection to server_host:server_port */
static int http_connect(void) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", server_port);

    if (getaddrinfo(server_host, port_str, &hints, &res) != 0 || !res) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: DNS resolve failed for %s", server_host);
        return -1;
    }

    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return -1;
    }

    /* Set socket timeout (5 seconds) */
#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (connect(sock, res->ai_addr, (socklen_t)res->ai_addrlen) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: connect() failed to %s:%d", server_host, server_port);
        closesocket(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sock;
}

/**
 * Perform an HTTP request with HMAC signing.
 * Returns the HTTP response body in out_buf (null-terminated).
 * Returns true on HTTP 2xx response.
 */
static bool http_request(const char* method, const char* path, const char* body, char* out_buf, size_t out_buf_size) {
    if (!configured)
        return false;

    int sock = http_connect();
    if (sock < 0)
        return false;

    /* Generate timestamp and signature */
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)time(NULL));

    /* payload for HMAC = timestamp + method + path + body */
    size_t payload_len = strlen(timestamp) + strlen(method) + strlen(path) + strlen(body);
    char* payload = (char*)malloc(payload_len + 1);
    snprintf(payload, payload_len + 1, "%s%s%s%s", timestamp, method, path, body);

    char signature[66];
    compute_hmac(payload, signature, sizeof(signature));
    free(payload);

    /* Build HTTP request */
    char request[HTTP_BUF_SIZE];
    int body_len = (int)strlen(body);

    int req_len = snprintf(request,
                           sizeof(request),
                           "%s %s HTTP/1.1\r\n"
                           "Host: %s:%d\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "X-Timestamp: %s\r\n"
                           "X-Signature: %s\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           method,
                           path,
                           server_host,
                           server_port,
                           body_len,
                           timestamp,
                           signature,
                           body);

    /* Send */
    int sent = send(sock, request, req_len, 0);
    if (sent < req_len) {
        closesocket(sock);
        return false;
    }

    /* Receive response */
    char response[HTTP_BUF_SIZE];
    int total = 0;
    while (total < (int)sizeof(response) - 1) {
        int n = recv(sock, response + total, (int)sizeof(response) - 1 - total, 0);
        if (n <= 0)
            break;
        total += n;
    }
    response[total] = '\0';
    closesocket(sock);

    /* Parse HTTP status code */
    int status = 0;
    if (strncmp(response, "HTTP/1.1 ", 9) == 0 || strncmp(response, "HTTP/1.0 ", 9) == 0) {
        status = atoi(response + 9);
    }

    /* Extract body (after \r\n\r\n) */
    const char* body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        snprintf(out_buf, out_buf_size, "%s", body_start);
    } else {
        out_buf[0] = '\0';
    }

    return (status >= 200 && status < 300);
}

/* ======== JSON helpers ======== */

/**
 * Escape a string for safe embedding in a JSON value.
 * Handles \", \\, and control characters (< 0x20) as \uXXXX.
 * Writes at most out_size-1 characters + null terminator.
 */
static void json_escape_string(const char* src, char* out, size_t out_size) {
    if (!src || out_size == 0)
        return;
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < out_size; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= out_size)
                break;
            out[j++] = '\\';
            out[j++] = c;
        } else if ((unsigned char)c < 0x20) {
            if (j + 6 >= out_size)
                break;
            j += snprintf(out + j, out_size - j, "\\u%04x", (unsigned char)c);
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

/* Extract a string value for a key like "key":"value" — writes into out (max out_size-1 chars) */
static bool json_get_string(const char* json, const char* key, char* out, size_t out_size) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* p = strstr(json, pattern);
    if (!p)
        return false;
    p += strlen(pattern);
    const char* end = strchr(p, '"');
    if (!end)
        return false;
    size_t len = (size_t)(end - p);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

/* ======== Public API ======== */

bool LobbyServer_UpdatePresence(const char* player_id, const char* display_name, const char* region,
                                const char* room_code, const char* connect_to) {
    char esc_pid[128], esc_name[64], esc_region[16], esc_code[32], esc_ct[32];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));
    json_escape_string(display_name, esc_name, sizeof(esc_name));
    json_escape_string(region ? region : "", esc_region, sizeof(esc_region));
    json_escape_string(room_code ? room_code : "", esc_code, sizeof(esc_code));
    json_escape_string(connect_to ? connect_to : "", esc_ct, sizeof(esc_ct));

    char body[512];
    snprintf(
        body,
        sizeof(body),
        "{\"player_id\":\"%s\",\"display_name\":\"%s\",\"region\":\"%s\",\"room_code\":\"%s\",\"connect_to\":\"%s\"}",
        esc_pid,
        esc_name,
        esc_region,
        esc_code,
        esc_ct);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/presence", body, response, sizeof(response));
}

bool LobbyServer_StartSearching(const char* player_id) {
    char esc_pid[128];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\"}", esc_pid);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/searching/start", body, response, sizeof(response));
}

bool LobbyServer_StopSearching(const char* player_id) {
    char esc_pid[128];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\"}", esc_pid);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/searching/stop", body, response, sizeof(response));
}

int LobbyServer_GetSearching(LobbyPlayer* out_players, int max_players, const char* region_filter) {
    char path[128];
    if (region_filter && strlen(region_filter) > 0) {
        snprintf(path, sizeof(path), "/searching?region=%s", region_filter);
    } else {
        snprintf(path, sizeof(path), "/searching");
    }

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response)))
        return 0;

    /* Parse JSON array of players from {"players":[...]} */
    int count = 0;
    const char* cursor = strstr(response, "\"players\":[");
    if (!cursor)
        return 0;
    cursor += 11; /* skip past "players":[ */

    while (count < max_players) {
        /* Find next object */
        const char* obj_start = strchr(cursor, '{');
        if (!obj_start)
            break;
        const char* obj_end = strchr(obj_start, '}');
        if (!obj_end)
            break;

        /* Extract fields from this object */
        size_t obj_len = (size_t)(obj_end - obj_start + 1);
        char obj[512];
        if (obj_len >= sizeof(obj))
            obj_len = sizeof(obj) - 1;
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        LobbyPlayer* p = &out_players[count];
        memset(p, 0, sizeof(*p));
        json_get_string(obj, "player_id", p->player_id, sizeof(p->player_id));
        json_get_string(obj, "display_name", p->display_name, sizeof(p->display_name));
        json_get_string(obj, "region", p->region, sizeof(p->region));
        json_get_string(obj, "room_code", p->room_code, sizeof(p->room_code));
        json_get_string(obj, "connect_to", p->connect_to, sizeof(p->connect_to));

        if (strlen(p->player_id) > 0)
            count++;

        cursor = obj_end + 1;
    }

    return count;
}

bool LobbyServer_Leave(const char* player_id) {
    char esc_pid[128];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\"}", esc_pid);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/leave", body, response, sizeof(response));
}
