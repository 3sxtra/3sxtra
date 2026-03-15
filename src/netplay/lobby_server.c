/**
 * @file lobby_server.c
 * @brief HTTP client for the 3SX lobby/matchmaking server.
 *
 * Communicates with the Node.js lobby server via HTTP/1.1 + HMAC-SHA256
 * request signing. Uses libcurl for HTTP requests and cJSON for JSON
 * serialization/deserialization.
 *
 * The SSE (Server-Sent Events) streaming client remains on raw sockets
 * for long-lived connections — curl would add complexity without benefit.
 *
 * HMAC implementation:
 *   - Windows: bcrypt.h (BCryptCreateHash / BCryptHashData / BCryptFinishHash)
 *   - Linux/macOS: embedded minimal SHA-256 (public domain)
 */
#ifndef _WIN32
#define _GNU_SOURCE // Must be before any includes for getaddrinfo/timeval
#endif
#include "lobby_server.h"
#include "identity.h"
#include "port/config/config.h"
#include <SDL3/SDL.h>
#include <curl/curl.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Platform includes (HMAC + SSE raw sockets only) --- */
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

static void compute_hmac(const char* payload, size_t payload_len, char* out_hex, size_t hex_size) {
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

    status = BCryptHashData(hHash, (PUCHAR)payload, (ULONG)payload_len, 0);
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
    hmac_sha256((const uint8_t*)server_key, strlen(server_key), (const uint8_t*)payload, payload_len, hash);
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

/* ======== HTTP client (libcurl) ======== */

#define HTTP_BUF_SIZE 16384

/* Dynamic buffer for curl write callback */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} CurlBuffer;

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    CurlBuffer* buf = (CurlBuffer*)userp;
    if (buf->size + total + 1 > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < buf->size + total + 1)
            new_cap = buf->size + total + 1;
        char* tmp = (char*)realloc(buf->data, new_cap);
        if (!tmp)
            return 0;
        buf->data = tmp;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

/**
 * Perform an HTTP request with HMAC signing via libcurl.
 * Returns the HTTP response body in out_buf (null-terminated).
 * Returns true on HTTP 2xx response.
 */
static bool http_request(const char* method, const char* path, const char* body, char* out_buf, size_t out_buf_size) {
    if (!configured)
        return false;

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    /* Build full URL */
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d%s", server_host, server_port, path);

    /* Generate timestamp and signature */
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)time(NULL));

    size_t body_len = body ? strlen(body) : 0;
    size_t payload_len = strlen(timestamp) + strlen(method) + strlen(path) + body_len;
    char* payload = (char*)malloc(payload_len + 1);
    snprintf(payload, payload_len + 1, "%s%s%s%s", timestamp, method, path, body ? body : "");

    char signature[66];
    compute_hmac(payload, payload_len, signature, sizeof(signature));
    free(payload);

    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    /* Headers */
    struct curl_slist* headers = NULL;
    char hdr_ts[64], hdr_sig[128];
    snprintf(hdr_ts, sizeof(hdr_ts), "X-Timestamp: %s", timestamp);
    snprintf(hdr_sig, sizeof(hdr_sig), "X-Signature: %s", signature);
    headers = curl_slist_append(headers, hdr_ts);
    headers = curl_slist_append(headers, hdr_sig);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Method + body */
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
    } else if (strcmp(method, "GET") == 0) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    }

    /* Response buffer */
    CurlBuffer resp = { .data = (char*)malloc(4096), .size = 0, .capacity = 4096 };
    if (resp.data)
        resp.data[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "LobbyServer: curl error for %s %s: %s",
                    method,
                    path,
                    curl_easy_strerror(res));
        free(resp.data);
        return false;
    }

    if (status < 200 || status >= 300) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: HTTP %ld for %s %s", status, method, path);
        if (status == 403 && resp.data) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "LobbyServer: 403 detail: %s", resp.data);
        }
    }

    /* Copy response body */
    if (resp.data && out_buf_size > 0) {
        SDL_strlcpy(out_buf, resp.data, out_buf_size);
    } else if (out_buf_size > 0) {
        out_buf[0] = '\0';
    }
    free(resp.data);

    return (status >= 200 && status < 300);
}

/* ======== JSON helpers (cJSON wrappers) ======== */

/** Copy a string field from a cJSON object. Returns false if key missing. */
static bool cjson_get_string(const cJSON* obj, const char* key, char* out, size_t out_size) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        SDL_strlcpy(out, item->valuestring, out_size);
        return true;
    }
    if (out_size > 0)
        out[0] = '\0';
    return false;
}

/** Get an integer field with default. */
static int cjson_get_int(const cJSON* obj, const char* key, int default_val) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item))
        return item->valueint;
    return default_val;
}

/** Get a double field with default. */
static double cjson_get_double(const cJSON* obj, const char* key, double default_val) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item))
        return item->valuedouble;
    return default_val;
}

/** Build a JSON body with player_id only: {"player_id":"..."} */
static char* json_body_pid(const char* player_id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", player_id);
    char* s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

/** Build a JSON body with player_id + room_code: {"player_id":"...","room_code":"..."} */
static char* json_body_pid_room(const char* player_id, const char* room_code) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "room_code", room_code);
    char* s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

/* ======== Public API ======== */

bool LobbyServer_UpdatePresence(const char* player_id, const char* display_name, const char* region,
                                const char* room_code, const char* connect_to, int rtt_ms, const char* connection_type,
                                int ft) {
    if (ft < 1)
        ft = 2;
    if (ft > 10)
        ft = 10;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "display_name", display_name);
    cJSON_AddStringToObject(root, "region", region ? region : "");
    cJSON_AddStringToObject(root, "room_code", room_code ? room_code : "");
    cJSON_AddStringToObject(root, "connect_to", connect_to ? connect_to : "");
    cJSON_AddNumberToObject(root, "rtt_ms", rtt_ms);
    cJSON_AddStringToObject(root, "connection_type", connection_type ? connection_type : "unknown");
    cJSON_AddNumberToObject(root, "ft", ft);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/presence", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_StartSearching(const char* player_id) {
    char* body = json_body_pid(player_id);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/searching/start", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_StopSearching(const char* player_id) {
    char* body = json_body_pid(player_id);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/searching/stop", body, response, sizeof(response));
    free(body);
    return ok;
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

    cJSON* root = cJSON_Parse(response);
    if (!root)
        return 0;

    int count = 0;
    const cJSON* players = cJSON_GetObjectItemCaseSensitive(root, "players");
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, players) {
        if (count >= max_players)
            break;
        LobbyPlayer* p = &out_players[count];
        memset(p, 0, sizeof(*p));
        cjson_get_string(item, "player_id", p->player_id, sizeof(p->player_id));
        cjson_get_string(item, "display_name", p->display_name, sizeof(p->display_name));
        cjson_get_string(item, "region", p->region, sizeof(p->region));
        cjson_get_string(item, "room_code", p->room_code, sizeof(p->room_code));
        cjson_get_string(item, "connect_to", p->connect_to, sizeof(p->connect_to));
        cjson_get_string(item, "status", p->status, sizeof(p->status));
        cjson_get_string(item, "country", p->country, sizeof(p->country));
        cjson_get_string(item, "connection_type", p->connection_type, sizeof(p->connection_type));
        p->rtt_ms = cjson_get_int(item, "rtt_ms", -1);
        p->ft = cjson_get_int(item, "ft", 2);
        if (strlen(p->player_id) > 0)
            count++;
    }

    cJSON_Delete(root);
    return count;
}

bool LobbyServer_Leave(const char* player_id) {
    char* body = json_body_pid(player_id);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/leave", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_DeclineInvite(const char* player_id, const char* declined_player_id) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "declined_player_id", declined_player_id);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/decline", body, response, sizeof(response));
    free(body);
    return ok;
}

/* === Room Discovery === */

int LobbyServer_ListRooms(RoomListItem* out_rooms, int max_rooms) {
    if (!configured || !out_rooms || max_rooms <= 0)
        return 0;

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", "/rooms/list", "", response, sizeof(response)))
        return 0;

    cJSON* root = cJSON_Parse(response);
    if (!root)
        return 0;

    int count = 0;
    const cJSON* rooms = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, rooms) {
        if (count >= max_rooms)
            break;
        RoomListItem* r = &out_rooms[count];
        memset(r, 0, sizeof(*r));
        cjson_get_string(item, "code", r->code, sizeof(r->code));
        cjson_get_string(item, "name", r->name, sizeof(r->name));
        r->player_count = cjson_get_int(item, "player_count", 0);
        r->ft = cjson_get_int(item, "ft", 1);
        if (strlen(r->code) > 0)
            count++;
    }

    cJSON_Delete(root);
    return count;
}

/* === Phase 2: Match Reporting === */

bool LobbyServer_ReportMatch(const MatchResult* result, int* out_match_id) {
    if (!configured || !result)
        return false;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", result->player_id);
    cJSON_AddStringToObject(root, "opponent_id", result->opponent_id);
    cJSON_AddStringToObject(root, "winner_id", result->winner_id);
    cJSON_AddNumberToObject(root, "player_char", result->player_char);
    cJSON_AddNumberToObject(root, "opponent_char", result->opponent_char);
    cJSON_AddNumberToObject(root, "rounds", result->rounds);
    cJSON_AddStringToObject(root, "source", result->source[0] ? result->source : "ranked");
    cJSON_AddNumberToObject(root, "ft", result->ft > 0 ? result->ft : 1);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/match_result", body, response, sizeof(response));
    free(body);

    if (ok) {
        SDL_Log("[LobbyServer] Match result reported: winner=%s", result->winner_id);
        if (out_match_id) {
            cJSON* resp_json = cJSON_Parse(response);
            *out_match_id = resp_json ? cjson_get_int(resp_json, "match_id", -1) : -1;
            cJSON_Delete(resp_json);
        }
    }
    return ok;
}

bool LobbyServer_UploadReplay(int match_id, const void* replay_data, size_t replay_size) {
    if (!configured || match_id < 0 || !replay_data || replay_size == 0)
        return false;

    char path[128];
    snprintf(path, sizeof(path), "/match_result/replay?match_id=%d", match_id);

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d%s", server_host, server_port, path);

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)time(NULL));

    /* HMAC payload = timestamp + "POST" + path + binary body */
    size_t header_len = strlen(timestamp) + 4 + strlen(path);
    size_t payload_len = header_len + replay_size;
    char* payload = (char*)malloc(payload_len);
    if (!payload) {
        curl_easy_cleanup(curl);
        return false;
    }
    snprintf(payload, header_len + 1, "%sPOST%s", timestamp, path);
    memcpy(payload + header_len, replay_data, replay_size);

    char signature[66];
    compute_hmac(payload, payload_len, signature, sizeof(signature));
    free(payload);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, replay_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)replay_size);

    struct curl_slist* headers = NULL;
    char hdr_ts[64], hdr_sig[128], hdr_pid[128];
    snprintf(hdr_ts, sizeof(hdr_ts), "X-Timestamp: %s", timestamp);
    snprintf(hdr_sig, sizeof(hdr_sig), "X-Signature: %s", signature);
    snprintf(hdr_pid, sizeof(hdr_pid), "X-Player-ID: %s", Identity_GetPlayerId());
    headers = curl_slist_append(headers, hdr_ts);
    headers = curl_slist_append(headers, hdr_sig);
    headers = curl_slist_append(headers, hdr_pid);
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION, "[LobbyServer] Replay upload curl error: %s", curl_easy_strerror(res));
        return false;
    }

    if (status >= 200 && status < 300) {
        SDL_Log("[LobbyServer] Replay uploaded successfully for match %d", match_id);
        return true;
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[LobbyServer] Replay upload failed: HTTP %ld", status);
        return false;
    }
}

bool LobbyServer_ReportDisconnect(const char* player_id, const char* opponent_id) {
    if (!configured || !player_id || !opponent_id)
        return false;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "opponent_id", opponent_id);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/match_disconnect", body, response, sizeof(response));
    free(body);
    if (ok) {
        SDL_Log("[LobbyServer] Disconnect reported: reporter=%s disconnecter=%s", player_id, opponent_id);
    }
    return ok;
}

bool LobbyServer_GetPlayerStats(const char* player_id, PlayerStats* out) {
    if (!configured || !player_id || !out)
        return false;

    memset(out, 0, sizeof(*out));

    char path[256];
    snprintf(path, sizeof(path), "/player/%s/stats", player_id);

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response))) {
        return false;
    }

    cJSON* root = cJSON_Parse(response);
    if (!root)
        return false;

    out->wins = cjson_get_int(root, "wins", 0);
    out->losses = cjson_get_int(root, "losses", 0);
    out->disconnects = cjson_get_int(root, "disconnects", 0);
    out->rating = (float)cjson_get_double(root, "rating", 1500.0);
    out->rd = (float)cjson_get_double(root, "rd", 350.0);
    cjson_get_string(root, "tier", out->tier, sizeof(out->tier));

    cJSON_Delete(root);
    return true;
}

/* === Phase 3: Leaderboards === */

/// Fetch a page of the leaderboard. Returns entry count (up to max_entries).
/// page is 0-indexed. *out_total receives the total player count (for pagination).
/// Returns -1 on error.
int LobbyServer_GetLeaderboard(LeaderboardEntry* out, int max_entries, int page, int* out_total) {
    if (!configured || !out || max_entries <= 0)
        return -1;

    char path[256];
    snprintf(path, sizeof(path), "/leaderboard?page=%d&limit=%d", page, max_entries);

    char response[HTTP_BUF_SIZE];
    if (!http_request("GET", path, "", response, sizeof(response))) {
        return -1;
    }

    cJSON* root = cJSON_Parse(response);
    if (!root)
        return -1;

    if (out_total) {
        *out_total = cjson_get_int(root, "total", 0);
    }

    int count = 0;
    const cJSON* players = cJSON_GetObjectItemCaseSensitive(root, "players");
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, players) {
        if (count >= max_entries)
            break;
        LeaderboardEntry* e = &out[count];
        memset(e, 0, sizeof(*e));
        e->rank = cjson_get_int(item, "rank", count + 1);
        cjson_get_string(item, "player_id", e->player_id, sizeof(e->player_id));
        cjson_get_string(item, "display_name", e->display_name, sizeof(e->display_name));
        e->wins = cjson_get_int(item, "wins", 0);
        e->losses = cjson_get_int(item, "losses", 0);
        e->disconnects = cjson_get_int(item, "disconnects", 0);
        e->rating = (float)cjson_get_double(item, "rating", 1500.0);
        cjson_get_string(item, "tier", e->tier, sizeof(e->tier));

        if (strlen(e->player_id) > 0)
            count++;
    }

    cJSON_Delete(root);
    return count;
}

// === Phase 5: Casual Lobbies (8-Player Rooms) ===

static void parse_room_json(const char* json_str, RoomState* out) {
    memset(out, 0, sizeof(*out));

    cJSON* root = cJSON_Parse(json_str);
    if (!root)
        return;

    cjson_get_string(root, "id", out->id, sizeof(out->id));
    cjson_get_string(root, "name", out->name, sizeof(out->name));
    cjson_get_string(root, "host", out->host, sizeof(out->host));
    out->ft = cjson_get_int(root, "ft", 1);

    /* Parse players array */
    const cJSON* players = cJSON_GetObjectItemCaseSensitive(root, "players");
    const cJSON* p_item = NULL;
    cJSON_ArrayForEach(p_item, players) {
        if (out->player_count >= MAX_ROOM_PLAYERS)
            break;
        RoomPlayer* rp = &out->players[out->player_count++];
        cjson_get_string(p_item, "player_id", rp->player_id, sizeof(rp->player_id));
        cjson_get_string(p_item, "display_name", rp->display_name, sizeof(rp->display_name));
        cjson_get_string(p_item, "region", rp->region, sizeof(rp->region));
        cjson_get_string(p_item, "country", rp->country, sizeof(rp->country));
    }

    /* Parse queue array: ["player_id_1", "player_id_2", ...] */
    const cJSON* queue = cJSON_GetObjectItemCaseSensitive(root, "queue");
    const cJSON* q_item = NULL;
    cJSON_ArrayForEach(q_item, queue) {
        if (out->queue_count >= MAX_ROOM_PLAYERS)
            break;
        if (cJSON_IsString(q_item) && q_item->valuestring) {
            SDL_strlcpy(out->queue[out->queue_count], q_item->valuestring, 64);
            out->queue_count++;
        }
    }

    /* Parse match object: {"p1":"id","p2":"id"} or null */
    const cJSON* match = cJSON_GetObjectItemCaseSensitive(root, "match");
    if (cJSON_IsObject(match)) {
        cjson_get_string(match, "p1", out->match_p1, sizeof(out->match_p1));
        cjson_get_string(match, "p2", out->match_p2, sizeof(out->match_p2));
        if (out->match_p1[0] && out->match_p2[0]) {
            out->match_active = 1;
        }
    }

    /* Parse chat array */
    const cJSON* chat = cJSON_GetObjectItemCaseSensitive(root, "chat");
    const cJSON* c_item = NULL;
    cJSON_ArrayForEach(c_item, chat) {
        if (out->chat_count >= MAX_CHAT_MESSAGES)
            break;
        ChatMessage* cm = &out->chat[out->chat_count++];
        cm->id = (uint64_t)cjson_get_int(c_item, "id", 0);
        cjson_get_string(c_item, "sender_id", cm->sender_id, sizeof(cm->sender_id));
        cjson_get_string(c_item, "sender_name", cm->sender_name, sizeof(cm->sender_name));
        cjson_get_string(c_item, "text", cm->text, sizeof(cm->text));
    }

    cJSON_Delete(root);
}

bool LobbyServer_CreateRoom(const char* name, int ft, RoomState* out_room) {
    if (!Identity_IsInitialized())
        return false;

    if (ft < 1)
        ft = 2;
    if (ft > 10)
        ft = 10;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", Identity_GetPlayerId());
    cJSON_AddStringToObject(root, "name", name ? name : "");
    cJSON_AddNumberToObject(root, "ft", ft);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/create", body, response, sizeof(response));
    free(body);

    if (!ok) {
        return false;
    }

    if (out_room) {
        // Response contains "room_code", so we parse it
        cJSON* res_json = cJSON_Parse(response);
        if (res_json) {
            cjson_get_string(res_json, "room_code", out_room->id, sizeof(out_room->id));
            cJSON_Delete(res_json);
        }

        SDL_strlcpy(out_room->name, name ? name : "", sizeof(out_room->name));
        SDL_strlcpy(out_room->host, Identity_GetPlayerId(), sizeof(out_room->host));
        out_room->player_count = 1;
        out_room->ft = ft;
        SDL_strlcpy(out_room->players[0].player_id, Identity_GetPlayerId(), sizeof(out_room->players[0].player_id));
        SDL_strlcpy(
            out_room->players[0].display_name, Identity_GetDisplayName(), sizeof(out_room->players[0].display_name));
    }
    return true;
}

bool LobbyServer_JoinRoom(const char* room_code, RoomState* out_room) {
    if (!Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/join", body, response, sizeof(response));
    free(body);

    if (ok && out_room) {
        parse_room_json(response, out_room);
    }
    return ok;
}

bool LobbyServer_LeaveRoom(const char* room_code) {
    if (!Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/leave", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_JoinQueue(const char* room_code) {
    if (!Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/queue/join", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_LeaveQueue(const char* room_code) {
    if (!Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/queue/leave", body, response, sizeof(response));
    free(body);
    return ok;
}

bool LobbyServer_SendChat(const char* room_code, const char* text) {
    if (!Identity_IsInitialized())
        return false;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "player_id", Identity_GetPlayerId());
    cJSON_AddStringToObject(root, "display_name", Identity_GetDisplayName());
    cJSON_AddStringToObject(root, "room_code", room_code);
    cJSON_AddStringToObject(root, "text", text);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/chat", body, response, sizeof(response));
    free(body);
    return ok;
}

// === GET /room/state (read-only, no side effects) ===

bool LobbyServer_GetRoomState(const char* room_code, RoomState* out) {
    if (!configured || !room_code || !out)
        return false;

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
    if (!configured || !room_code || !winner_id)
        return false;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "room_code", room_code);
    cJSON_AddStringToObject(root, "winner_id", winner_id);
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/end", body, response, sizeof(response));
    free(body);

    if (ok) {
        SDL_Log("[LobbyServer] Match end reported: room=%s winner=%s", room_code, winner_id);
    }
    return ok;
}

// === Phase 6: Match Accept/Decline ===

bool LobbyServer_AcceptMatch(const char* room_code) {
    if (!configured || !room_code || !Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/accept", body, response, sizeof(response));
    free(body);

    if (ok) {
        SDL_Log("[LobbyServer] Match accepted: room=%s", room_code);
    }
    return ok;
}

bool LobbyServer_DeclineMatch(const char* room_code) {
    if (!configured || !room_code || !Identity_IsInitialized())
        return false;

    char* body = json_body_pid_room(Identity_GetPlayerId(), room_code);
    char response[HTTP_BUF_SIZE];
    bool ok = http_request("POST", "/room/match/decline", body, response, sizeof(response));
    free(body);

    if (ok) {
        SDL_Log("[LobbyServer] Match declined: room=%s", room_code);
    }
    return ok;
}

/* ======== SSE raw socket connect ========
 * The SSE streaming client needs a long-lived TCP socket for recv() polling.
 * This is separate from the curl-based HTTP client used by all other APIs. */
static int sse_raw_connect(void) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", server_port);

    if (getaddrinfo(server_host, port_str, &hints, &res) != 0 || !res) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: DNS resolve failed");
        return -1;
    }

    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return -1;
    }

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: connect() failed");
        closesocket(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sock;
}

// === SSE Streaming Client ===

// SSE thread state — all access guarded by SDL atomics
static SDL_AtomicInt s_sse_running = { 0 }; // 1 = thread active
static SDL_AtomicInt s_sse_stop = { 0 };    // 1 = request stop
static SDL_Thread* s_sse_thread = NULL;
static SDL_AtomicInt s_sse_sock = { -1 }; // raw socket (for external close), atomic for thread safety
static char s_sse_room_code[16] = { 0 };

// SSE auto-reconnection state
static SDL_AtomicInt s_sse_reconnect_count = { 0 }; // consecutive failures
static uint32_t s_sse_last_disconnect_ms = 0;       // SDL_GetTicks() at last disconnect
#define SSE_MAX_RECONNECTS 5

// Lock-free ring buffer for SSE events. The background thread writes at
// s_sse_write_idx (mod SSE_RING_SIZE), and the main thread reads at
// s_sse_read_idx. Both are atomic — no mutex needed.
#define SSE_RING_SIZE 16
static SSEEvent s_sse_ring[SSE_RING_SIZE];
static SDL_AtomicInt s_sse_write_idx = { 0 };
static SDL_AtomicInt s_sse_read_idx = { 0 };

/**
 * Parse an SSE JSON event line like:
 *   data: {"type":"chat","data":{"id":123,"sender_id":"abc","sender_name":"Host","text":"hi"}}
 * Populates out_event based on the "type" field.
 */
static void sse_parse_event(const char* json_str, SSEEvent* out) {
    memset(out, 0, sizeof(*out));

    cJSON* root = cJSON_Parse(json_str);
    if (!root)
        return;

    char type_str[32] = { 0 };
    cjson_get_string(root, "type", type_str, sizeof(type_str));

    const cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (strcmp(type_str, "sync") == 0) {
        out->type = SSE_EVENT_SYNC;
        if (data) {
            char* data_str = cJSON_PrintUnformatted(data);
            if (data_str) {
                parse_room_json(data_str, &out->room);
                free(data_str);
            }
        }
    } else if (strcmp(type_str, "chat") == 0) {
        out->type = SSE_EVENT_CHAT;
        if (data) {
            cjson_get_string(data, "sender_id", out->chat_msg.sender_id, sizeof(out->chat_msg.sender_id));
            cjson_get_string(data, "sender_name", out->chat_msg.sender_name, sizeof(out->chat_msg.sender_name));
            cjson_get_string(data, "text", out->chat_msg.text, sizeof(out->chat_msg.text));
            out->chat_msg.id = (uint64_t)cjson_get_int(data, "id", 0);
        }
    } else if (strcmp(type_str, "join") == 0) {
        out->type = SSE_EVENT_JOIN;
        if (data) {
            cjson_get_string(data, "player_id", out->player_id, sizeof(out->player_id));
            cjson_get_string(data, "display_name", out->display_name, sizeof(out->display_name));
        }
    } else if (strcmp(type_str, "leave") == 0) {
        out->type = SSE_EVENT_LEAVE;
        if (data) {
            cjson_get_string(data, "player_id", out->player_id, sizeof(out->player_id));
        }
    } else if (strcmp(type_str, "queue_update") == 0) {
        out->type = SSE_EVENT_QUEUE_UPDATE;
    } else if (strcmp(type_str, "host_migrated") == 0) {
        out->type = SSE_EVENT_HOST_MIGRATED;
    } else if (strcmp(type_str, "match_start") == 0) {
        out->type = SSE_EVENT_MATCH_START;
    } else if (strcmp(type_str, "match_propose") == 0) {
        out->type = SSE_EVENT_MATCH_PROPOSE;
        if (data) {
            out->propose_ft = cjson_get_int(data, "ft", 1);
            const cJSON* p1 = cJSON_GetObjectItemCaseSensitive(data, "p1");
            if (p1) {
                cjson_get_string(p1, "id", out->propose_p1_id, sizeof(out->propose_p1_id));
                cjson_get_string(p1, "name", out->propose_p1_name, sizeof(out->propose_p1_name));
                cjson_get_string(p1, "connection_type", out->propose_p1_conn_type, sizeof(out->propose_p1_conn_type));
                out->propose_p1_rtt_ms = cjson_get_int(p1, "rtt_ms", -1);
                cjson_get_string(p1, "room_code", out->propose_p1_room_code, sizeof(out->propose_p1_room_code));
                cjson_get_string(p1, "region", out->propose_p1_region, sizeof(out->propose_p1_region));
            }
            const cJSON* p2 = cJSON_GetObjectItemCaseSensitive(data, "p2");
            if (p2) {
                cjson_get_string(p2, "id", out->propose_p2_id, sizeof(out->propose_p2_id));
                cjson_get_string(p2, "name", out->propose_p2_name, sizeof(out->propose_p2_name));
                cjson_get_string(p2, "connection_type", out->propose_p2_conn_type, sizeof(out->propose_p2_conn_type));
                out->propose_p2_rtt_ms = cjson_get_int(p2, "rtt_ms", -1);
                cjson_get_string(p2, "room_code", out->propose_p2_room_code, sizeof(out->propose_p2_room_code));
                cjson_get_string(p2, "region", out->propose_p2_region, sizeof(out->propose_p2_region));
            }
        }
    } else if (strcmp(type_str, "match_decline") == 0) {
        out->type = SSE_EVENT_MATCH_DECLINE;
        if (data) {
            cjson_get_string(data, "decliner_id", out->propose_decliner_id, sizeof(out->propose_decliner_id));
            cjson_get_string(data, "reason", out->propose_reason, sizeof(out->propose_reason));
            const cJSON* p1 = cJSON_GetObjectItemCaseSensitive(data, "p1");
            if (p1) {
                cjson_get_string(p1, "id", out->propose_p1_id, sizeof(out->propose_p1_id));
                cjson_get_string(p1, "name", out->propose_p1_name, sizeof(out->propose_p1_name));
            }
            const cJSON* p2 = cJSON_GetObjectItemCaseSensitive(data, "p2");
            if (p2) {
                cjson_get_string(p2, "id", out->propose_p2_id, sizeof(out->propose_p2_id));
                cjson_get_string(p2, "name", out->propose_p2_name, sizeof(out->propose_p2_name));
            }
        }
    } else if (strcmp(type_str, "match_end") == 0) {
        out->type = SSE_EVENT_MATCH_END;
        if (data) {
            cjson_get_string(data, "winner_id", out->match_winner_id, sizeof(out->match_winner_id));
            cjson_get_string(data, "loser_id", out->match_loser_id, sizeof(out->match_loser_id));
        }
    }

    cJSON_Delete(root);
}

#define SSE_BUF_SIZE 8192

static int sse_thread_fn(void* userdata) {
    (void)userdata;

    int sock = sse_raw_connect();
    if (sock < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: connect failed");
        SDL_SetAtomicInt(&s_sse_running, 0);
        return 1;
    }
    SDL_SetAtomicInt(&s_sse_sock, sock);

    // Build HTTP GET request for SSE (no auth — room code is the secret)
    char request[512];
    int req_len = snprintf(request,
                           sizeof(request),
                           "GET /room/events?room_code=%s HTTP/1.1\r\n"
                           "Host: %s:%d\r\n"
                           "Accept: text/event-stream\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n",
                           s_sse_room_code,
                           server_host,
                           server_port);

    if (send(sock, request, req_len, 0) < req_len) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSE: send failed");
        closesocket(sock);
        SDL_SetAtomicInt(&s_sse_sock, -1);
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
            if (WSAGetLastError() == WSAETIMEDOUT)
                continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
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
            if (!hdr_end)
                continue; // Need more data
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
            if (!data_prefix)
                break;

            char* line_end = strstr(data_prefix, "\n\n");
            if (!line_end)
                break; // Incomplete — wait for more data

            // Extract the JSON payload after "data: "
            char* json_start = data_prefix + 6;
            *line_end = '\0';

            // Parse and store the event
            SSEEvent evt;
            sse_parse_event(json_start, &evt);

            if (evt.type != SSE_EVENT_NONE) {
                // Write to ring buffer slot at write_idx, then advance.
                // Overflow check: if reader hasn't consumed fast enough, drop oldest.
                int wi = SDL_GetAtomicInt(&s_sse_write_idx);
                int ri = SDL_GetAtomicInt(&s_sse_read_idx);
                if (wi - ri >= SSE_RING_SIZE) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SSE: ring buffer overflow — dropping oldest event");
                    SDL_SetAtomicInt(&s_sse_read_idx, ri + 1);
                }
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
    SDL_SetAtomicInt(&s_sse_sock, -1);
    SDL_SetAtomicInt(&s_sse_running, 0);
    // Record disconnect time for reconnection backoff (only if not intentional)
    if (!SDL_GetAtomicInt(&s_sse_stop)) {
        s_sse_last_disconnect_ms = SDL_GetTicks();
        SDL_SetAtomicInt(&s_sse_reconnect_count, SDL_GetAtomicInt(&s_sse_reconnect_count) + 1);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SSE: unexpected disconnect from room %s (retry %d/%d)",
                    s_sse_room_code,
                    SDL_GetAtomicInt(&s_sse_reconnect_count),
                    SSE_MAX_RECONNECTS);
    } else {
        SDL_Log("SSE: disconnected from room %s", s_sse_room_code);
    }
    return 0;
}

bool LobbyServer_SSEConnect(const char* room_code) {
    if (!configured || !room_code || strlen(room_code) == 0)
        return false;

    // Disconnect any existing connection first
    if (SDL_GetAtomicInt(&s_sse_running)) {
        LobbyServer_SSEDisconnect();
    }

    snprintf(s_sse_room_code, sizeof(s_sse_room_code), "%s", room_code);

    SDL_SetAtomicInt(&s_sse_stop, 0);
    SDL_SetAtomicInt(&s_sse_running, 1);
    SDL_SetAtomicInt(&s_sse_write_idx, 0);
    SDL_SetAtomicInt(&s_sse_read_idx, 0);
    SDL_SetAtomicInt(&s_sse_reconnect_count, 0); // Reset reconnect counter
    s_sse_last_disconnect_ms = 0;
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
    // Mark as intentional disconnect — prevents auto-reconnection
    SDL_SetAtomicInt(&s_sse_stop, 1);
    s_sse_room_code[0] = '\0'; // Clear room code to signal "don't reconnect"

    if (!SDL_GetAtomicInt(&s_sse_running))
        return;

    // Force-close the socket to unblock recv() immediately
    {
        int sock = SDL_GetAtomicInt(&s_sse_sock);
        if (sock >= 0) {
            SDL_SetAtomicInt(&s_sse_sock, -1);
            closesocket(sock);
        }
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
    SDL_SetAtomicInt(&s_sse_reconnect_count, 0);
}

SSEEventType LobbyServer_SSEPoll(SSEEvent* out_event) {
    // Auto-reconnect: if thread died but room code is still set (not intentional disconnect)
    if (!SDL_GetAtomicInt(&s_sse_running) && s_sse_room_code[0] != '\0' && !SDL_GetAtomicInt(&s_sse_stop)) {
        int retries = SDL_GetAtomicInt(&s_sse_reconnect_count);
        if (retries < SSE_MAX_RECONNECTS) {
            // Exponential backoff: 2s, 4s, 8s, 16s, 30s
            uint32_t backoff_ms = (uint32_t)(2000 << retries);
            if (backoff_ms > 30000)
                backoff_ms = 30000;
            uint32_t elapsed = SDL_GetTicks() - s_sse_last_disconnect_ms;
            if (elapsed >= backoff_ms) {
                SDL_Log("SSE: auto-reconnecting to room %s (attempt %d/%d)",
                        s_sse_room_code,
                        retries + 1,
                        SSE_MAX_RECONNECTS);
                SDL_SetAtomicInt(&s_sse_stop, 0);
                SDL_SetAtomicInt(&s_sse_running, 1);
                s_sse_thread = SDL_CreateThread(sse_thread_fn, "SSERoomStream", NULL);
                if (s_sse_thread) {
                    SDL_DetachThread(s_sse_thread);
                } else {
                    SDL_SetAtomicInt(&s_sse_running, 0);
                }
            }
        }
        return SSE_EVENT_NONE;
    }

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

    // Successful event read — reset reconnect count (connection is healthy)
    if (SDL_GetAtomicInt(&s_sse_reconnect_count) > 0) {
        SDL_SetAtomicInt(&s_sse_reconnect_count, 0);
    }

    SDL_SetAtomicInt(&s_sse_read_idx, ri + 1);
    return type;
}

bool LobbyServer_SSEIsConnected(void) {
    return SDL_GetAtomicInt(&s_sse_running) != 0;
}
