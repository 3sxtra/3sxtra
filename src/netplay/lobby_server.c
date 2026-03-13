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
#include "port/config/config.h"
#include "identity.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// clang-format off
#ifdef _WIN32
#include <winsock2.h>  // Must precede bcrypt.h — provides LONG/ULONG via windows.h
#include <ws2tcpip.h>
#include <bcrypt.h>
// clang-format on
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
// NOTE: The HMAC key is used as raw ASCII bytes (not base64-decoded).
// The server does the same, so signing matches. This is intentional.
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
    SDL_Log("LobbyServer: Configured (port %d)", server_port);
}

bool LobbyServer_IsConfigured(void) {
    return configured;
}

/* ======== SHA-256 (shared portable implementation) ======== */

/* On non-Windows, use the shared portable SHA-256 for both hashing and HMAC.
 * On Windows, BCrypt handles HMAC but we still include the header for types. */
#include "sha256.h"

/* ======== HMAC computation (cross-platform) ======== */

static void compute_hmac(const char* payload, char* out_hex, size_t hex_size) {
    uint8_t hash[32];

#ifdef _WIN32
    /* Windows: use BCrypt HMAC */
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    ULONG cbHashObject = 0, cbResult = 0;
    uint8_t* pbHashObject = NULL;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status < 0) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCryptOpenAlgorithmProvider failed: 0x%lx", status);
        goto hmac_done;
    }

    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cbHashObject, sizeof(ULONG), &cbResult, 0);
    if (status < 0) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCryptGetProperty failed: 0x%lx", status);
        goto hmac_done;
    }

    pbHashObject = (uint8_t*)malloc(cbHashObject);
    if (!pbHashObject) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCrypt malloc failed");
        goto hmac_done;
    }

    status =
        BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, (PUCHAR)server_key, (ULONG)strlen(server_key), 0);
    if (status < 0) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCryptCreateHash failed: 0x%lx", status);
        goto hmac_done;
    }

    status = BCryptHashData(hHash, (PUCHAR)payload, (ULONG)strlen(payload), 0);
    if (status < 0) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCryptHashData failed: 0x%lx", status);
        goto hmac_done;
    }

    status = BCryptFinishHash(hHash, hash, 32, 0);
    if (status < 0) {
        memset(hash, 0, 32);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BCryptFinishHash failed: 0x%lx", status);
        goto hmac_done;
    }

hmac_done:
    if (hHash)
        BCryptDestroyHash(hHash);
    free(pbHashObject);
    if (hAlg)
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

    // NOTE: getaddrinfo() is a blocking syscall with no portable timeout control.
    // If the DNS server is unresponsive, this can block for 30+ seconds.
    // This function is only called from background threads, so the main thread
    // is not affected, but stalled threads may accumulate until DNS resolves.
    if (getaddrinfo(server_host, port_str, &hints, &res) != 0 || !res) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: DNS resolve failed");
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: connect() failed");
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

    if (status < 200 || status >= 300) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: HTTP %d for %s %s", status, method, path);
        // Surface 403 reason (e.g. stale timestamp / clock drift) for debugging
        if (status == 403) {
            const char* reason = strstr(response, "\r\n\r\n");
            if (reason) {
                reason += 4;
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: 403 detail: %s", reason);
            }
        }
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
    /* Find the closing quote, skipping escaped quotes */
    const char* end = p;
    while (*end) {
        if (*end == '"' && (end == p || *(end - 1) != '\\'))
            break;
        end++;
    }
    if (!*end)
        return false;
    size_t len = (size_t)(end - p);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

/* Extract an integer value for a key like "key":123 */
static int json_get_int(const char* json, const char* key, int default_val) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* p = strstr(json, pattern);
    if (!p)
        return default_val;
    p += strlen(pattern);
    /* Skip whitespace */
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '-' || (*p >= '0' && *p <= '9'))
        return atoi(p);
    return default_val;
}

/* ======== Public API ======== */

bool LobbyServer_UpdatePresence(const char* player_id, const char* display_name, const char* region,
                                const char* room_code, const char* connect_to, int rtt_ms,
                                const char* connection_type) {
    char esc_pid[128], esc_name[64], esc_region[16], esc_code[32], esc_ct[32], esc_conn[16];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));
    json_escape_string(display_name, esc_name, sizeof(esc_name));
    json_escape_string(region ? region : "", esc_region, sizeof(esc_region));
    json_escape_string(room_code ? room_code : "", esc_code, sizeof(esc_code));
    json_escape_string(connect_to ? connect_to : "", esc_ct, sizeof(esc_ct));
    json_escape_string(connection_type ? connection_type : "unknown", esc_conn, sizeof(esc_conn));

    char body[512];
    snprintf(body,
             sizeof(body),
             "{\"player_id\":\"%s\",\"display_name\":\"%s\",\"region\":\"%s\",\"room_code\":\"%s\",\"connect_to\":\""
             "%s\",\"rtt_ms\":%d,\"connection_type\":\"%s\"}",
             esc_pid,
             esc_name,
             esc_region,
             esc_code,
             esc_ct,
             rtt_ms,
             esc_conn);

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
        json_get_string(obj, "status", p->status, sizeof(p->status));
        json_get_string(obj, "country", p->country, sizeof(p->country));
        json_get_string(obj, "connection_type", p->connection_type, sizeof(p->connection_type));
        p->rtt_ms = json_get_int(obj, "rtt_ms", -1);

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

bool LobbyServer_DeclineInvite(const char* player_id, const char* declined_player_id) {
    char esc_pid[128], esc_dpid[128];
    json_escape_string(player_id, esc_pid, sizeof(esc_pid));
    json_escape_string(declined_player_id, esc_dpid, sizeof(esc_dpid));

    char body[256];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"declined_player_id\":\"%s\"}", esc_pid, esc_dpid);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/decline", body, response, sizeof(response));
}

/* === Phase 2: Match Reporting === */

bool LobbyServer_ReportMatch(const MatchResult* result) {
    if (!configured || !result) return false;

    char esc_pid[128], esc_oid[128], esc_wid[128];
    json_escape_string(result->player_id, esc_pid, sizeof(esc_pid));
    json_escape_string(result->opponent_id, esc_oid, sizeof(esc_oid));
    json_escape_string(result->winner_id, esc_wid, sizeof(esc_wid));

    char body[512];
    snprintf(body, sizeof(body),
             "{\"player_id\":\"%s\",\"opponent_id\":\"%s\",\"winner_id\":\"%s\","
             "\"player_char\":%d,\"opponent_char\":%d,\"rounds\":%d}",
             esc_pid, esc_oid, esc_wid,
             result->player_char, result->opponent_char, result->rounds);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/match_result", body, response, sizeof(response));
    if (ok) {
        SDL_Log("[LobbyServer] Match result reported: winner=%s", result->winner_id);
    }
    return ok;
}

bool LobbyServer_GetPlayerStats(const char* player_id, PlayerStats* out) {
    if (!configured || !player_id || !out) return false;

    memset(out, 0, sizeof(*out));

    char path[256];
    snprintf(path, sizeof(path), "/player/%s/stats", player_id);

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response))) {
        return false;
    }

    /* Extract the HTTP body (after blank line) */
    const char* body = strstr(response, "\r\n\r\n");
    if (!body) return false;
    body += 4;

    out->wins = json_get_int(body, "wins", 0);
    out->losses = json_get_int(body, "losses", 0);
    out->disconnects = json_get_int(body, "disconnects", 0);

    /* Parse rating/rd as floats from JSON */
    const char* rating_str = strstr(body, "\"rating\":");
    if (rating_str) {
        out->rating = (float)strtod(rating_str + 9, NULL);
    } else {
        out->rating = 1500.0f;
    }

    const char* rd_str = strstr(body, "\"rd\":");
    if (rd_str) {
        out->rd = (float)strtod(rd_str + 5, NULL);
    } else {
        out->rd = 350.0f;
    }

    json_get_string(body, "tier", out->tier, sizeof(out->tier));

    return true;
}

/* === Phase 3: Leaderboards === */

/// Fetch a page of the leaderboard. Returns entry count (up to max_entries).
/// page is 0-indexed. *out_total receives the total player count (for pagination).
/// Returns -1 on error.
int LobbyServer_GetLeaderboard(LeaderboardEntry* out, int max_entries, int page, int* out_total) {
    if (!configured || !out || max_entries <= 0) return -1;

    char path[256];
    snprintf(path, sizeof(path), "/leaderboard?page=%d&limit=%d", page, max_entries);

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response))) {
        return -1;
    }

    /* Extract body */
    const char* body = strstr(response, "\r\n\r\n");
    if (!body) return -1;
    body += 4;

    /* Parse total player count */
    if (out_total) {
        *out_total = json_get_int(body, "total", 0);
    }

    /* Find "players":[ array */
    const char* cursor = strstr(body, "\"players\":[");
    if (!cursor) return 0;
    cursor += 11; /* skip past "players":[ */

    int count = 0;
    while (count < max_entries) {
        const char* obj_start = strchr(cursor, '{');
        if (!obj_start) break;
        const char* obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        size_t obj_len = (size_t)(obj_end - obj_start + 1);
        char obj[512];
        if (obj_len >= sizeof(obj)) obj_len = sizeof(obj) - 1;
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        LeaderboardEntry* e = &out[count];
        memset(e, 0, sizeof(*e));
        e->rank = json_get_int(obj, "rank", count + 1);
        json_get_string(obj, "player_id", e->player_id, sizeof(e->player_id));
        json_get_string(obj, "display_name", e->display_name, sizeof(e->display_name));
        e->wins = json_get_int(obj, "wins", 0);
        e->losses = json_get_int(obj, "losses", 0);

        const char* rating_str = strstr(obj, "\"rating\":");
        if (rating_str) {
            e->rating = (float)strtod(rating_str + 9, NULL);
        } else {
            e->rating = 1500.0f;
        }

        json_get_string(obj, "tier", e->tier, sizeof(e->tier));

        if (strlen(e->player_id) > 0)
            count++;

        cursor = obj_end + 1;
    }

    return count;
}


// === Phase 5: Casual Lobbies (8-Player Rooms) ===

static void parse_room_json(const char* json, RoomState* out) {
    memset(out, 0, sizeof(*out));

    json_get_string(json, "id", out->id, sizeof(out->id));
    json_get_string(json, "name", out->name, sizeof(out->name));
    json_get_string(json, "host", out->host, sizeof(out->host));

    // Simple arrays are parsed loosely to avoid full JSON AST parsing overhead.
    // In a real prod client we'd link cJSON or Jansson, but we stick to strstr here
    // for consistency with the rest of this zero-dependency file.
    
    // Parse players array: [{"player_id":"...","display_name":"...","region":"..."},...]
    const char* p_start = strstr(json, "\"players\":[");
    if (p_start) {
        const char* p_end = strchr(p_start, ']');
        if (p_end) {
            const char* cur = p_start;
            while (cur < p_end && out->player_count < MAX_ROOM_PLAYERS) {
                cur = strstr(cur, "{\"player_id\"");
                if (!cur || cur > p_end) break;
                
                char p_obj[256];
                const char* obj_end = strchr(cur, '}');
                if (!obj_end) break;
                
                size_t len = obj_end - cur + 1;
                if (len >= sizeof(p_obj)) len = sizeof(p_obj) - 1;
                memcpy(p_obj, cur, len);
                p_obj[len] = '\0';
                
                RoomPlayer* rp = &out->players[out->player_count++];
                json_get_string(p_obj, "player_id", rp->player_id, sizeof(rp->player_id));
                json_get_string(p_obj, "display_name", rp->display_name, sizeof(rp->display_name));
                json_get_string(p_obj, "region", rp->region, sizeof(rp->region));
                
                cur = obj_end;
            }
        }
    }

    // Parse queue array: ["player_id_1", "player_id_2", ...]
    const char* q_start = strstr(json, "\"queue\":[");
    if (q_start) {
        q_start += 9; // skip '"queue":['
        const char* q_end = strchr(q_start, ']');
        if (q_end) {
            const char* cur = q_start;
            while (cur < q_end && out->queue_count < MAX_ROOM_PLAYERS) {
                const char* quote1 = strchr(cur, '"');
                if (!quote1 || quote1 >= q_end) break;
                const char* quote2 = strchr(quote1 + 1, '"');
                if (!quote2 || quote2 >= q_end) break;
                
                size_t id_len = quote2 - quote1 - 1;
                if (id_len >= 64) id_len = 63;
                memcpy(out->queue[out->queue_count], quote1 + 1, id_len);
                out->queue[out->queue_count][id_len] = '\0';
                out->queue_count++;
                
                cur = quote2 + 1;
            }
        }
    }

    // Parse match object: {"p1":"id","p2":"id","state":"playing"} or null
    const char* m_start = strstr(json, "\"match\":{");
    if (m_start) {
        m_start += 8; // skip '"match":'
        json_get_string(m_start, "p1", out->match_p1, sizeof(out->match_p1));
        json_get_string(m_start, "p2", out->match_p2, sizeof(out->match_p2));
        if (out->match_p1[0] && out->match_p2[0]) {
            out->match_active = 1;
        }
    }

    // Parse chat array: [{"id":123,"sender_id":"...","sender_name":"...","text":"..."},...]
    const char* c_start = strstr(json, "\"chat\":[");
    if (c_start) {
        c_start += 8; // skip '"chat":['
        const char* c_end = NULL;
        // Find the closing ']' — must handle nested braces within chat objects
        int depth = 1;
        const char* scan = c_start;
        while (*scan && depth > 0) {
            if (*scan == '[') depth++;
            else if (*scan == ']') depth--;
            if (depth > 0) scan++;
        }
        c_end = scan;

        const char* cur = c_start;
        while (cur < c_end && out->chat_count < MAX_CHAT_MESSAGES) {
            const char* obj_start = strchr(cur, '{');
            if (!obj_start || obj_start >= c_end) break;
            const char* obj_end = strchr(obj_start, '}');
            if (!obj_end || obj_end >= c_end) break;

            char c_obj[512];
            size_t len = obj_end - obj_start + 1;
            if (len >= sizeof(c_obj)) len = sizeof(c_obj) - 1;
            memcpy(c_obj, obj_start, len);
            c_obj[len] = '\0';

            ChatMessage* cm = &out->chat[out->chat_count++];
            cm->id = (uint64_t)json_get_int(c_obj, "id", 0);
            json_get_string(c_obj, "sender_id", cm->sender_id, sizeof(cm->sender_id));
            json_get_string(c_obj, "sender_name", cm->sender_name, sizeof(cm->sender_name));
            json_get_string(c_obj, "text", cm->text, sizeof(cm->text));

            cur = obj_end + 1;
        }
    }
}

bool LobbyServer_CreateRoom(const char* name, RoomState* out_room) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_name[64];
    json_escape_string(name ? name : "", esc_name, sizeof(esc_name));

    char body[256];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"name\":\"%s\"}", 
             Identity_GetPlayerId(), esc_name);

    char response[HTTP_BUF_SIZE];
    if (!http_request("POST", "/room/create", body, response, sizeof(response))) {
        return false;
    }

    if (out_room) {
        // Response contains "room_code", so we do a partial parse just to get the code,
        // then the client usually joins or fetches full state.
        json_get_string(response, "room_code", out_room->id, sizeof(out_room->id));
        strncpy(out_room->name, name ? name : "", sizeof(out_room->name) - 1);
        strncpy(out_room->host, Identity_GetPlayerId(), sizeof(out_room->host) - 1);
        out_room->player_count = 1;
        strncpy(out_room->players[0].player_id, Identity_GetPlayerId(), sizeof(out_room->players[0].player_id) - 1);
        strncpy(out_room->players[0].display_name, Identity_GetDisplayName(), sizeof(out_room->players[0].display_name) - 1);
    }
    return true;
}

bool LobbyServer_JoinRoom(const char* room_code, RoomState* out_room) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[256];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"display_name\":\"%s\",\"room_code\":\"%s\"}", 
             Identity_GetPlayerId(), Identity_GetDisplayName(), esc_code);

    char response[HTTP_BUF_SIZE];
    if (!http_request("POST", "/room/join", body, response, sizeof(response))) {
        return false;
    }

    if (out_room) {
        parse_room_json(response, out_room);
    }
    return true;
}

bool LobbyServer_LeaveRoom(const char* room_code) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"room_code\":\"%s\"}", 
             Identity_GetPlayerId(), esc_code);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/room/leave", body, response, sizeof(response));
}

bool LobbyServer_JoinQueue(const char* room_code) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"room_code\":\"%s\"}", 
             Identity_GetPlayerId(), esc_code);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/room/queue/join", body, response, sizeof(response));
}

bool LobbyServer_LeaveQueue(const char* room_code) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[128];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"room_code\":\"%s\"}", 
             Identity_GetPlayerId(), esc_code);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/room/queue/leave", body, response, sizeof(response));
}

bool LobbyServer_SendChat(const char* room_code, const char* text) {
    if (!Identity_IsInitialized()) return false;
    
    char esc_code[16], esc_text[256];
    json_escape_string(room_code, esc_code, sizeof(esc_code));
    json_escape_string(text, esc_text, sizeof(esc_text));

    char body[512];
    snprintf(body, sizeof(body), "{\"player_id\":\"%s\",\"display_name\":\"%s\",\"room_code\":\"%s\",\"text\":\"%s\"}", 
             Identity_GetPlayerId(), Identity_GetDisplayName(), esc_code, esc_text);

    char response[HTTP_BUF_SIZE];
    return http_request("POST", "/room/chat", body, response, sizeof(response));
}

// === GET /room/state (read-only, no side effects) ===

bool LobbyServer_GetRoomState(const char* room_code, RoomState* out) {
    if (!configured || !room_code || !out) return false;

    char path[128];
    snprintf(path, sizeof(path), "/room/state?room_code=%s", room_code);

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response))) {
        return false;
    }

    parse_room_json(response, out);
    return true;
}

bool LobbyServer_ReportMatchEnd(const char* room_code, const char* winner_id) {
    if (!configured || !room_code || !winner_id) return false;

    char esc_code[16], esc_wid[128];
    json_escape_string(room_code, esc_code, sizeof(esc_code));
    json_escape_string(winner_id, esc_wid, sizeof(esc_wid));

    char body[256];
    snprintf(body, sizeof(body), "{\"room_code\":\"%s\",\"winner_id\":\"%s\"}",
             esc_code, esc_wid);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/end", body, response, sizeof(response));
    if (ok) {
        SDL_Log("[LobbyServer] Match end reported: room=%s winner=%s", room_code, winner_id);
    }
    return ok;
}

// === Phase 6: Match Accept/Decline ===

bool LobbyServer_AcceptMatch(const char* room_code) {
    if (!configured || !room_code || !Identity_IsInitialized()) return false;

    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[256];
    snprintf(body, sizeof(body), "{\"room_code\":\"%s\",\"player_id\":\"%s\"}",
             esc_code, Identity_GetPlayerId());

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/accept", body, response, sizeof(response));
    if (ok) {
        SDL_Log("[LobbyServer] Match accepted: room=%s", room_code);
    }
    return ok;
}

bool LobbyServer_DeclineMatch(const char* room_code) {
    if (!configured || !room_code || !Identity_IsInitialized()) return false;

    char esc_code[16];
    json_escape_string(room_code, esc_code, sizeof(esc_code));

    char body[256];
    snprintf(body, sizeof(body), "{\"room_code\":\"%s\",\"player_id\":\"%s\"}",
             esc_code, Identity_GetPlayerId());

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/decline", body, response, sizeof(response));
    if (ok) {
        SDL_Log("[LobbyServer] Match declined: room=%s", room_code);
    }
    return ok;
}

// === SSE Streaming Client ===

// SSE thread state — all access guarded by SDL atomics
static SDL_AtomicInt s_sse_running = { 0 };     // 1 = thread active
static SDL_AtomicInt s_sse_stop    = { 0 };     // 1 = request stop
static SDL_Thread*   s_sse_thread  = NULL;
static int           s_sse_sock    = -1;         // raw socket (for external close)
static char          s_sse_room_code[16] = { 0 };

// Lock-free ring buffer for SSE events. The background thread writes at
// s_sse_write_idx (mod SSE_RING_SIZE), and the main thread reads at
// s_sse_read_idx. Both are atomic — no mutex needed.
#define SSE_RING_SIZE 16
static SSEEvent s_sse_ring[SSE_RING_SIZE];
static SDL_AtomicInt s_sse_write_idx = { 0 };
static SDL_AtomicInt s_sse_read_idx  = { 0 };

/**
 * Parse an SSE JSON event line like:
 *   data: {"type":"chat","data":{"id":123,"sender_id":"abc","sender_name":"Host","text":"hi"}}
 * Populates out_event based on the "type" field.
 */
static void sse_parse_event(const char* json, SSEEvent* out) {
    memset(out, 0, sizeof(*out));

    char type_str[32];
    if (!json_get_string(json, "type", type_str, sizeof(type_str))) {
        return;
    }

    if (strcmp(type_str, "sync") == 0) {
        out->type = SSE_EVENT_SYNC;
        // The "data" field contains the full room state
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7; // skip '"data":', pointing at the '{' of the nested object
            parse_room_json(data_start, &out->room);
        }
    } else if (strcmp(type_str, "chat") == 0) {
        out->type = SSE_EVENT_CHAT;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7; // skip '"data":', pointing at '{'
            json_get_string(data_start, "sender_id", out->chat_msg.sender_id, sizeof(out->chat_msg.sender_id));
            json_get_string(data_start, "sender_name", out->chat_msg.sender_name, sizeof(out->chat_msg.sender_name));
            json_get_string(data_start, "text", out->chat_msg.text, sizeof(out->chat_msg.text));
            out->chat_msg.id = (uint64_t)json_get_int(data_start, "id", 0);
        }
    } else if (strcmp(type_str, "join") == 0) {
        out->type = SSE_EVENT_JOIN;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7; // skip '"data":', pointing at '{'
            json_get_string(data_start, "player_id", out->player_id, sizeof(out->player_id));
            json_get_string(data_start, "display_name", out->display_name, sizeof(out->display_name));
        }
    } else if (strcmp(type_str, "leave") == 0) {
        out->type = SSE_EVENT_LEAVE;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7; // skip '"data":', pointing at '{'
            json_get_string(data_start, "player_id", out->player_id, sizeof(out->player_id));
        }
    } else if (strcmp(type_str, "queue_update") == 0) {
        out->type = SSE_EVENT_QUEUE_UPDATE;
    } else if (strcmp(type_str, "host_migrated") == 0) {
        out->type = SSE_EVENT_HOST_MIGRATED;
    } else if (strcmp(type_str, "match_start") == 0) {
        out->type = SSE_EVENT_MATCH_START;
        /* match_start contains full room state via sync after broadcastRoomEvent.
           The RmlUI layer will re-fetch room state for structural changes. */
    } else if (strcmp(type_str, "match_propose") == 0) {
        out->type = SSE_EVENT_MATCH_PROPOSE;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7;
            /* Parse p1 object */
            const char* p1_start = strstr(data_start, "\"p1\":{");
            if (p1_start) {
                p1_start += 5;
                json_get_string(p1_start, "id", out->propose_p1_id, sizeof(out->propose_p1_id));
                json_get_string(p1_start, "name", out->propose_p1_name, sizeof(out->propose_p1_name));
                json_get_string(p1_start, "connection_type", out->propose_p1_conn_type, sizeof(out->propose_p1_conn_type));
                out->propose_p1_rtt_ms = json_get_int(p1_start, "rtt_ms", -1);
                json_get_string(p1_start, "room_code", out->propose_p1_room_code, sizeof(out->propose_p1_room_code));
                json_get_string(p1_start, "region", out->propose_p1_region, sizeof(out->propose_p1_region));
            }
            /* Parse p2 object */
            const char* p2_start = strstr(data_start, "\"p2\":{");
            if (p2_start) {
                p2_start += 5;
                json_get_string(p2_start, "id", out->propose_p2_id, sizeof(out->propose_p2_id));
                json_get_string(p2_start, "name", out->propose_p2_name, sizeof(out->propose_p2_name));
                json_get_string(p2_start, "connection_type", out->propose_p2_conn_type, sizeof(out->propose_p2_conn_type));
                out->propose_p2_rtt_ms = json_get_int(p2_start, "rtt_ms", -1);
                json_get_string(p2_start, "room_code", out->propose_p2_room_code, sizeof(out->propose_p2_room_code));
                json_get_string(p2_start, "region", out->propose_p2_region, sizeof(out->propose_p2_region));
            }
        }
    } else if (strcmp(type_str, "match_decline") == 0) {
        out->type = SSE_EVENT_MATCH_DECLINE;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7;
            json_get_string(data_start, "decliner_id", out->propose_decliner_id, sizeof(out->propose_decliner_id));
            json_get_string(data_start, "reason", out->propose_reason, sizeof(out->propose_reason));
            /* Parse p1/p2 info */
            const char* p1_start = strstr(data_start, "\"p1\":{");
            if (p1_start) {
                p1_start += 5;
                json_get_string(p1_start, "id", out->propose_p1_id, sizeof(out->propose_p1_id));
                json_get_string(p1_start, "name", out->propose_p1_name, sizeof(out->propose_p1_name));
            }
            const char* p2_start = strstr(data_start, "\"p2\":{");
            if (p2_start) {
                p2_start += 5;
                json_get_string(p2_start, "id", out->propose_p2_id, sizeof(out->propose_p2_id));
                json_get_string(p2_start, "name", out->propose_p2_name, sizeof(out->propose_p2_name));
            }
        }
    } else if (strcmp(type_str, "match_end") == 0) {
        out->type = SSE_EVENT_MATCH_END;
        const char* data_start = strstr(json, "\"data\":{");
        if (data_start) {
            data_start += 7;
            json_get_string(data_start, "winner_id", out->match_winner_id, sizeof(out->match_winner_id));
            json_get_string(data_start, "loser_id", out->match_loser_id, sizeof(out->match_loser_id));
        }
    }
}

#define SSE_BUF_SIZE 8192

static int sse_thread_fn(void* userdata) {
    (void)userdata;

    int sock = http_connect();
    if (sock < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: connect failed");
        SDL_SetAtomicInt(&s_sse_running, 0);
        return 1;
    }
    s_sse_sock = sock;

    // Build HTTP GET request for SSE (no auth — room code is the secret)
    char request[512];
    int req_len = snprintf(request, sizeof(request),
        "GET /room/events?room_code=%s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Accept: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        s_sse_room_code, server_host, server_port);

    if (send(sock, request, req_len, 0) < req_len) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: send failed");
        closesocket(sock);
        s_sse_sock = -1;
        SDL_SetAtomicInt(&s_sse_running, 0);
        return 1;
    }

    SDL_Log("SSE: connected to room %s", s_sse_room_code);

    // Read the HTTP response headers first, then process SSE data lines
    char buf[SSE_BUF_SIZE];
    int buf_len = 0;
    bool headers_done = false;

    // Set a shorter recv timeout so we can check the stop flag periodically
#ifdef _WIN32
    DWORD sse_timeout = 1000; // 1 second
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&sse_timeout, sizeof(sse_timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    while (!SDL_GetAtomicInt(&s_sse_stop)) {
        int n = recv(sock, buf + buf_len, (int)(sizeof(buf) - 1 - buf_len), 0);
        if (n < 0) {
            // Timeout — just check stop flag and retry
#ifdef _WIN32
            if (WSAGetLastError() == WSAETIMEDOUT) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: recv error");
            break;
        }
        if (n == 0) {
            SDL_Log("SSE: server closed connection");
            break;
        }

        buf_len += n;
        buf[buf_len] = '\0';

        // Skip HTTP headers on first receive
        if (!headers_done) {
            char* hdr_end = strstr(buf, "\r\n\r\n");
            if (!hdr_end) continue; // Need more data
            hdr_end += 4;
            int remaining = buf_len - (int)(hdr_end - buf);
            if (remaining > 0) {
                memmove(buf, hdr_end, remaining);
            }
            buf_len = remaining;
            buf[buf_len] = '\0';
            headers_done = true;
        }

        // Process complete "data: {...}\n\n" lines
        char* line_start = buf;
        while (1) {
            // SSE format: "data: {json}\n\n"
            char* data_prefix = strstr(line_start, "data: ");
            if (!data_prefix) break;

            char* line_end = strstr(data_prefix, "\n\n");
            if (!line_end) break; // Incomplete — wait for more data

            // Extract the JSON payload after "data: "
            char* json_start = data_prefix + 6;
            *line_end = '\0';

            // Parse and store the event
            SSEEvent evt;
            sse_parse_event(json_start, &evt);

            if (evt.type != SSE_EVENT_NONE) {
                // Write to ring buffer slot at write_idx, then advance
                int wi = SDL_GetAtomicInt(&s_sse_write_idx);
                memcpy(&s_sse_ring[wi % SSE_RING_SIZE], &evt, sizeof(SSEEvent));
                SDL_SetAtomicInt(&s_sse_write_idx, wi + 1);
            }

            line_start = line_end + 2; // Skip past \n\n
        }

        // Move unprocessed data to front of buffer
        int remaining = buf_len - (int)(line_start - buf);
        if (remaining > 0 && line_start != buf) {
            memmove(buf, line_start, remaining);
        }
        buf_len = remaining;
    }

    closesocket(sock);
    s_sse_sock = -1;
    SDL_SetAtomicInt(&s_sse_running, 0);
    SDL_Log("SSE: disconnected from room %s", s_sse_room_code);
    return 0;
}

bool LobbyServer_SSEConnect(const char* room_code) {
    if (!configured || !room_code || strlen(room_code) == 0) return false;

    // Disconnect any existing connection first
    if (SDL_GetAtomicInt(&s_sse_running)) {
        LobbyServer_SSEDisconnect();
    }

    snprintf(s_sse_room_code, sizeof(s_sse_room_code), "%s", room_code);

    SDL_SetAtomicInt(&s_sse_stop, 0);
    SDL_SetAtomicInt(&s_sse_running, 1);
    SDL_SetAtomicInt(&s_sse_write_idx, 0);
    SDL_SetAtomicInt(&s_sse_read_idx, 0);
    memset(s_sse_ring, 0, sizeof(s_sse_ring));

    s_sse_thread = SDL_CreateThread(sse_thread_fn, "SSERoomStream", NULL);
    if (!s_sse_thread) {
        SDL_SetAtomicInt(&s_sse_running, 0);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: failed to create thread");
        return false;
    }
    SDL_DetachThread(s_sse_thread);
    return true;
}

void LobbyServer_SSEDisconnect(void) {
    if (!SDL_GetAtomicInt(&s_sse_running)) return;

    SDL_SetAtomicInt(&s_sse_stop, 1);

    // Force-close the socket to unblock recv() immediately
    if (s_sse_sock >= 0) {
        closesocket(s_sse_sock);
        s_sse_sock = -1;
    }

    // Wait for thread to finish (up to 2 seconds)
    int wait_ms = 0;
    while (SDL_GetAtomicInt(&s_sse_running) && wait_ms < 2000) {
        SDL_Delay(10);
        wait_ms += 10;
    }

    s_sse_thread = NULL;
    SDL_SetAtomicInt(&s_sse_write_idx, 0);
    SDL_SetAtomicInt(&s_sse_read_idx, 0);
}

SSEEventType LobbyServer_SSEPoll(SSEEvent* out_event) {
    int ri = SDL_GetAtomicInt(&s_sse_read_idx);
    int wi = SDL_GetAtomicInt(&s_sse_write_idx);
    if (ri >= wi) {
        return SSE_EVENT_NONE; // Ring empty
    }

    // If writer has lapped us (more than SSE_RING_SIZE events behind),
    // skip to the oldest available slot to avoid reading stale data.
    if (wi - ri > SSE_RING_SIZE) {
        ri = wi - SSE_RING_SIZE;
    }

    SSEEvent* slot = &s_sse_ring[ri % SSE_RING_SIZE];
    SSEEventType type = slot->type;

    if (out_event) {
        memcpy(out_event, slot, sizeof(SSEEvent));
    }

    SDL_SetAtomicInt(&s_sse_read_idx, ri + 1);
    return type;
}

bool LobbyServer_SSEIsConnected(void) {
    return SDL_GetAtomicInt(&s_sse_running) != 0;
}
