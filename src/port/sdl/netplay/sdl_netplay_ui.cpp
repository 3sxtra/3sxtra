/**
 * @file sdl_netplay_ui.cpp
 * @brief Netplay HUD, diagnostics overlay, and toast notification system.
 *
 * Renders ping/rollback history graphs, connection status indicators,
 * and timed toast messages using ImGui during netplay sessions.
 */
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "netplay/netplay.h"
#include <SDL3/SDL.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
// Windows headers define macros that collide with game engine struct field names
#ifdef s_addr
#undef s_addr
#endif
#ifdef cmb2
#undef cmb2
#endif
#ifdef cmb3
#undef cmb3
#endif
#else
#include <arpa/inet.h>
#endif
#include "netplay/discovery.h"
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "netplay/net_detect.h"
#include "netplay/ping_probe.h"
#include "netplay/stun.h"
#include "netplay/upnp.h"
#include "port/config/config.h"

// Master RmlUi toggle + per-component toggles
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

// Game engine globals for match result reporting
#include "sf33rd/Source/Game/engine/workuser.h"

static bool hud_visible = true;
static bool diagnostics_visible = false;
static uint64_t session_start_ticks = 0;

// Lobby state
static StunResult stun_result = { 0 };
static char my_room_code[16] = { 0 };

static char lobby_status_msg[128] = { 0 };

// Server browser state
static bool lobby_server_registered = false;
static bool lobby_server_searching = false;
static LobbyPlayer lobby_server_players[16];
static SDL_AtomicInt lobby_server_player_count = { 0 };
static uint32_t lobby_server_last_poll = 0;
static char lobby_my_player_id[64] = { 0 };
static char current_opponent_id[64] = { 0 }; // tracked explicitly to fix room rotation reporting
static int lobby_my_rtt_ms = -1;            // Our measured RTT to the lobby server (still sent to server for presence)
static bool ping_probe_initialized = false; // True once PingProbe_Init has been called
static const char* my_connection_type = "unknown"; // Detected once at lobby entry
#define LOBBY_POLL_INTERVAL_MS 2000

// Match reporting state
static NetplaySessionState last_session_state = NETPLAY_SESSION_IDLE;
static bool match_result_reported = false;
static SDL_AtomicInt async_match_report_active = { 0 };

#include "port/save/native_save.h"
#include "sf33rd/Source/Game/system/work_sys.h"

// NativeReplayHeader must match the struct in native_save.c exactly
typedef struct {
    uint32_t magic;     // 0x33535852 = "3SXR"
    uint32_t version;   // 1
    uint32_t data_size; // sizeof(_REPLAY_W)
    uint32_t reserved;
} AsyncNativeReplayHeader;

#define ASYNC_NATIVE_REPLAY_MAGIC 0x33535852
#define ASYNC_NATIVE_REPLAY_VERSION 1

// Async data: carries both the match result and a snapshot of the replay
typedef struct {
    MatchResult result;
    void* replay_snapshot;  // malloc'd NativeReplayHeader + _REPLAY_W data
    size_t replay_size;     // total size of the snapshot (header + data)
} AsyncMatchReportData;

static int async_match_report_fn(void* userdata) {
    AsyncMatchReportData* data = (AsyncMatchReportData*)userdata;
    int match_id = -1;
    bool ok = LobbyServer_ReportMatch(&data->result, &match_id);

    if (ok) {
        SDL_Log("[NetplayUI] Match reported successfully (match_id=%d)", match_id);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[NetplayUI] Match report FAILED");
    }

    // Upload the in-memory replay snapshot if the match was recorded
    if (ok && match_id >= 0 && data->replay_snapshot && data->replay_size > 0) {
        bool upload_ok = LobbyServer_UploadReplay(match_id, data->replay_snapshot, data->replay_size);
        if (upload_ok) {
            SDL_Log("[NetplayUI] Replay uploaded for match %d (%zu bytes)", match_id, data->replay_size);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[NetplayUI] Replay upload FAILED for match %d", match_id);
        }
    }

    free(data->replay_snapshot);
    free(data);
    SDL_SetAtomicInt(&async_match_report_active, 0);
    return 0;
}

static void AsyncReportMatch(const char* my_id, const char* opponent_id, const char* winner_id, int my_char,
                             int opp_char, int rounds, const char* source, int ft) {
    if (!LobbyServer_IsConfigured() || !my_id || !my_id[0])
        return;
    if (SDL_GetAtomicInt(&async_match_report_active) != 0)
        return;
    SDL_SetAtomicInt(&async_match_report_active, 1);

    AsyncMatchReportData* data = (AsyncMatchReportData*)malloc(sizeof(AsyncMatchReportData));
    if (!data) {
        SDL_SetAtomicInt(&async_match_report_active, 0);
        return;
    }
    memset(data, 0, sizeof(*data));

    // Fill match result
    MatchResult* r = &data->result;
    // Truncation is intentional — MatchResult fields are fixed-size buffers.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
    snprintf(r->player_id, sizeof(r->player_id), "%s", my_id);
    snprintf(r->opponent_id, sizeof(r->opponent_id), "%s", opponent_id);
    snprintf(r->winner_id, sizeof(r->winner_id), "%s", winner_id);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    r->player_char = my_char;
    r->opponent_char = opp_char;
    r->rounds = rounds;
    snprintf(r->source, sizeof(r->source), "%s", source ? source : "ranked");
    r->ft = ft > 0 ? ft : 1;

    // Snapshot Replay_w from memory now (before the user can overwrite it).
    // This avoids the race condition of reading from disk before the replay is saved.
    size_t snapshot_size = sizeof(AsyncNativeReplayHeader) + sizeof(_REPLAY_W);
    void* snapshot = malloc(snapshot_size);
    if (snapshot) {
        AsyncNativeReplayHeader hdr = {
            ASYNC_NATIVE_REPLAY_MAGIC, ASYNC_NATIVE_REPLAY_VERSION, (uint32_t)sizeof(_REPLAY_W), 0
        };
        memcpy(snapshot, &hdr, sizeof(hdr));
        memcpy((uint8_t*)snapshot + sizeof(hdr), &Replay_w, sizeof(_REPLAY_W));
        data->replay_snapshot = snapshot;
        data->replay_size = snapshot_size;
    }

    SDL_Thread* t = SDL_CreateThread(async_match_report_fn, "AsyncMatchReport", data);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(data->replay_snapshot);
        free(data);
        SDL_SetAtomicInt(&async_match_report_active, 0);
    }
}

// --- Async disconnect reporting ---
static SDL_AtomicInt async_disconnect_active = { 0 };

struct DisconnectData {
    char player_id[64];
    char opponent_id[64];
};

static int SDLCALL async_disconnect_fn(void* userdata) {
    DisconnectData* d = (DisconnectData*)userdata;
    LobbyServer_ReportDisconnect(d->player_id, d->opponent_id);
    free(d);
    SDL_SetAtomicInt(&async_disconnect_active, 0);
    return 0;
}

static void AsyncReportDisconnect(const char* my_id, const char* opponent_id) {
    if (!LobbyServer_IsConfigured() || !my_id || !my_id[0])
        return;
    if (SDL_GetAtomicInt(&async_disconnect_active) != 0)
        return;
    SDL_SetAtomicInt(&async_disconnect_active, 1);

    DisconnectData* d = (DisconnectData*)malloc(sizeof(DisconnectData));
    snprintf(d->player_id, sizeof(d->player_id), "%s", my_id);
    snprintf(d->opponent_id, sizeof(d->opponent_id), "%s", opponent_id);

    SDL_Thread* t = SDL_CreateThread(async_disconnect_fn, "AsyncDisconnect", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&async_disconnect_active, 0);
    }
}

// Anti-spam: local cooldown for declined players
#define MAX_DECLINED_PLAYERS 16
#define DEFAULT_INVITE_COOLDOWN_MS 30000
static struct {
    char player_id[64];
    uint32_t cooldown_until; // SDL_GetTicks() value
} declined_players[MAX_DECLINED_PLAYERS] = {};
static int declined_player_count = 0;

static void add_declined_player(const char* pid) {
    int cooldown_ms = Config_GetInt(CFG_KEY_NETPLAY_INVITE_COOLDOWN);
    if (cooldown_ms <= 0)
        cooldown_ms = DEFAULT_INVITE_COOLDOWN_MS;
    else
        cooldown_ms *= 1000; // config is in seconds

    // Check if already in list and update
    for (int i = 0; i < declined_player_count; i++) {
        if (strcmp(declined_players[i].player_id, pid) == 0) {
            declined_players[i].cooldown_until = SDL_GetTicks() + cooldown_ms;
            return;
        }
    }
    // Add new entry (wrap around if full)
    int idx = declined_player_count < MAX_DECLINED_PLAYERS ? declined_player_count++ : 0;
    snprintf(declined_players[idx].player_id, sizeof(declined_players[idx].player_id), "%s", pid);
    declined_players[idx].cooldown_until = SDL_GetTicks() + cooldown_ms;
}

static bool is_player_declined(const char* pid) {
    uint32_t now = SDL_GetTicks();
    for (int i = 0; i < declined_player_count; i++) {
        if (strcmp(declined_players[i].player_id, pid) == 0) {
            if (now < declined_players[i].cooldown_until)
                return true;
            // Expired — remove by swapping with last
            declined_players[i] = declined_players[--declined_player_count];
            return false;
        }
    }
    return false;
}

// Helper: check if player passes local filter criteria
static bool player_passes_filters(const LobbyPlayer* p) {
    // Region lock
    if (Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK)) {
        const char* my_region = Config_GetString(CFG_KEY_LOBBY_REGION);
        if (my_region && my_region[0] && p->region[0]) {
            if (strcmp(my_region, p->region) != 0)
                return false;
        }
    }
    // Max ping — use true P2P RTT from ping probe if available
    int max_ping = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
    if (max_ping > 0) {
        int p2p_rtt = PingProbe_GetRTT(p->player_id);
        if (p2p_rtt < 0 && p->rtt_ms > 0) {
            // Fallback: triangulated estimate if no direct measurement yet
            p2p_rtt = lobby_my_rtt_ms + p->rtt_ms;
        }
        if (p2p_rtt > 0 && p2p_rtt > max_ping)
            return false;
    }
    // Block WiFi
    if (Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI)) {
        if (strcmp(p->connection_type, "wifi") == 0)
            return false;
    }
    return true;
}

// Async state machine
enum LobbyAsyncState {
    LOBBY_ASYNC_IDLE = 0,
    LOBBY_ASYNC_DISCOVERING, // STUN thread running
    LOBBY_ASYNC_READY,       // STUN done, waiting for user
    LOBBY_ASYNC_PUNCHING,    // Hole punch thread running
    LOBBY_ASYNC_PUNCH_DONE,  // Hole punch finished
    LOBBY_ASYNC_STUN_FAILED, // STUN failed
    LOBBY_ASYNC_UPNP_TRYING, // UPnP fallback thread running
    LOBBY_ASYNC_UPNP_DONE,   // UPnP finished
    LOBBY_ASYNC_WAIT_PEER,   // Initiator waiting for receiver to accept
};

static SDL_AtomicInt lobby_async_state = { LOBBY_ASYNC_IDLE };
static SDL_Thread* lobby_thread = NULL;
static SDL_AtomicInt lobby_thread_result = { 0 }; // 1=success, 0=fail
static uint32_t lobby_punch_peer_ip = 0;
static uint16_t lobby_punch_peer_port = 0;
static char lobby_punch_peer_ip_str[32] = { 0 };
static char lobby_punch_peer_name[32] = { 0 }; // Display name of peer being punched
static UpnpMapping lobby_upnp_mapping = {};
static bool native_lobby_active = false;

// Pending internet invite state (for native lobby indication)
static bool lobby_has_pending_invite = false;
static char lobby_pending_invite_name[32] = { 0 };
static uint32_t lobby_pending_invite_ip = 0;
static uint16_t lobby_pending_invite_port = 0;
static char lobby_pending_invite_room[16] = { 0 };
static char lobby_pending_invite_region[8] = { 0 };
static uint32_t lobby_pending_invite_time = 0; // Timestamp when invite was detected (for expiry)
static int lobby_pending_invite_ping = -1;       // ms, -1 = unknown
static int lobby_pending_invite_ft = 2;           // Challenger's FT mode
static SDL_AtomicInt lobby_punch_cancel = { 0 }; // Set to 1 to cancel in-progress hole punch
static bool lobby_we_are_initiator = false;      // true = we clicked Connect, false = they invited us
static char lobby_connect_to_intent[16] = { 0 }; // Current connect_to value preserved across heartbeats

// Wait-for-peer state: initiator waits for receiver to accept before Netplay_Begin
#define LOBBY_WAIT_PEER_TIMEOUT_MS 30000
static uint32_t lobby_wait_peer_start = 0;

// Forward declarations for functions used by lobby_poll_server
static void lobby_start_punch(uint32_t peer_ip, uint16_t peer_port);

// --- Async Lobby API ---
typedef struct {
    char player_id[128];
    char display_name[64];
    char region[8];
    char room_code[32];
    char connect_to[32];
    char connection_type[8];
    int rtt_ms;
    int ft;
} AsyncPresenceData;

static SDL_AtomicInt async_presence_active = { 0 };

static int SDLCALL async_presence_fn(void* data) {
    AsyncPresenceData* d = (AsyncPresenceData*)data;
    LobbyServer_UpdatePresence(
        d->player_id, d->display_name, d->region, d->room_code, d->connect_to, d->rtt_ms, d->connection_type, d->ft);
    free(d);
    SDL_SetAtomicInt(&async_presence_active, 0);
    return 0;
}

static void AsyncUpdatePresence(const char* pid, const char* disp, const char* rc, const char* ct) {
    if (!LobbyServer_IsConfigured() || !pid || !pid[0])
        return;
    if (SDL_GetAtomicInt(&async_presence_active) != 0)
        return; // Previous request still in-flight; skip to avoid thread accumulation
    SDL_SetAtomicInt(&async_presence_active, 1);
    AsyncPresenceData* d = (AsyncPresenceData*)malloc(sizeof(AsyncPresenceData));
    memset(d, 0, sizeof(*d));
    snprintf(d->player_id, sizeof(d->player_id), "%s", pid);
    if (disp)
        snprintf(d->display_name, sizeof(d->display_name), "%s", disp);
    const char* region = Config_GetString(CFG_KEY_LOBBY_REGION);
    if (region && region[0])
        snprintf(d->region, sizeof(d->region), "%s", region);
    if (rc)
        snprintf(d->room_code, sizeof(d->room_code), "%s", rc);
    if (ct)
        snprintf(d->connect_to, sizeof(d->connect_to), "%s", ct);
    snprintf(d->connection_type, sizeof(d->connection_type), "%s", my_connection_type);
    d->rtt_ms = lobby_my_rtt_ms;
    d->ft = Config_GetInt(CFG_KEY_NETPLAY_FT);
    SDL_Thread* t = SDL_CreateThread(async_presence_fn, "AsyncPresence", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&async_presence_active, 0);
    }
}

typedef struct {
    char player_id[128];
    int action;
} AsyncActionData;

static SDL_AtomicInt async_action_active = { 0 };

static int SDLCALL async_action_fn(void* data) {
    AsyncActionData* d = (AsyncActionData*)data;
    if (d->action == 1)
        LobbyServer_StartSearching(d->player_id);
    else if (d->action == 2)
        LobbyServer_StopSearching(d->player_id);
    else if (d->action == 3)
        LobbyServer_Leave(d->player_id);
    free(d);
    SDL_SetAtomicInt(&async_action_active, 0);
    return 0;
}

static void AsyncLobbyAction(const char* pid, int action) {
    if (!LobbyServer_IsConfigured() || !pid || !pid[0])
        return;
    if (SDL_GetAtomicInt(&async_action_active) != 0)
        return; // Previous action still in-flight; skip to avoid thread accumulation
    SDL_SetAtomicInt(&async_action_active, 1);
    AsyncActionData* d = (AsyncActionData*)malloc(sizeof(AsyncActionData));
    snprintf(d->player_id, sizeof(d->player_id), "%s", pid);
    d->action = action;
    SDL_Thread* t = SDL_CreateThread(async_action_fn, "AsyncAction", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&async_action_active, 0);
    }
}

static SDL_AtomicInt lobby_poll_active = { 0 };

static int SDLCALL lobby_poll_thread_fn(void* data) {
    (void)data;
    LobbyPlayer temp_players[16]; // Stack-local — static here was a data race risk

    // Measure HTTP RTT to the lobby server (still used for server-side presence)
    uint32_t t0 = SDL_GetTicks();
    int count = LobbyServer_GetSearching(temp_players, 16, NULL);
    uint32_t t1 = SDL_GetTicks();
    lobby_my_rtt_ms = (int)(t1 - t0);

    SDL_MemoryBarrierRelease();
    memcpy(lobby_server_players, temp_players, sizeof(temp_players));
    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&lobby_server_player_count, count);

    // Sync peers into PingProbe and send/receive probes
    if (ping_probe_initialized) {
        // Add/update peers from the player list
        for (int i = 0; i < count; i++) {
            // Skip ourselves
            if (strcmp(temp_players[i].player_id, lobby_my_player_id) == 0)
                continue;
            // Decode their STUN endpoint from room_code
            uint32_t peer_ip = 0;
            uint16_t peer_port = 0;
            if (temp_players[i].room_code[0] && Stun_DecodeEndpoint(temp_players[i].room_code, &peer_ip, &peer_port)) {
                // Skip probing peers on the same LAN (same public IP) — unreachable via
                // public endpoint due to no hairpin NAT, causes false "unreachable" warnings.
                if (stun_result.public_ip != 0 && peer_ip == stun_result.public_ip)
                    continue;
                PingProbe_AddPeer(peer_ip, peer_port, temp_players[i].player_id);
            }
        }
        PingProbe_Update();
    }

    SDL_SetAtomicInt(&lobby_poll_active, 0);
    return 0;
}

// (Ping probe removed — we now measure RTT from the eager hole punch instead)

// Server-polling helper — runs every frame while in lobby, independent of ImGui.
// Polls the lobby server for player list AND checks for incoming connect_to invites.
static void lobby_poll_server(void) {
    // Must be registered with a valid room code to poll
    if (!lobby_server_registered || !my_room_code[0])
        return;

    // Always poll when registered — we need to detect incoming invites even when not searching
    bool lobby_auto = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);

    uint32_t now = SDL_GetTicks();
    if (now - lobby_server_last_poll >= LOBBY_POLL_INTERVAL_MS || lobby_server_last_poll == 0) {
        if (SDL_GetAtomicInt(&lobby_poll_active) == 0) {
            SDL_SetAtomicInt(&lobby_poll_active, 1);

            // Keep our presence alive (heartbeat every poll cycle to avoid stale eviction)
            // IMPORTANT: preserve connect_to if we have an active connection intent
            {
                const char* display = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                if (!display || !display[0])
                    display = my_room_code;
                AsyncUpdatePresence(lobby_my_player_id, display, my_room_code, lobby_connect_to_intent);
            }

            SDL_Thread* t = SDL_CreateThread(lobby_poll_thread_fn, "LobbyPoll", NULL);
            if (t)
                SDL_DetachThread(t);
            else
                SDL_SetAtomicInt(&lobby_poll_active, 0);

            lobby_server_last_poll = now;
        }
    }

    // Check if another player wants to connect to us (works even when not searching)
    // Skip if we're already mid-connection (punching, UPnP, etc.)
    int cur_state = SDL_GetAtomicInt(&lobby_async_state);
    if (cur_state != LOBBY_ASYNC_READY) {
        // Don't clear pending invite here — it persists until user accepts/declines
        return;
    }
    bool found_invite = false;
    int player_count = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < player_count; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (lobby_server_players[i].connect_to[0] && strcmp(lobby_server_players[i].connect_to, my_room_code) == 0) {

            // Anti-spam: check if this player is on our local declined list
            if (is_player_declined(lobby_server_players[i].player_id)) {
                SDL_Log("[lobby] Ignoring invite from %s (declined cooldown active)",
                        lobby_server_players[i].display_name);
                continue;
            }

            uint32_t peer_ip = 0;
            uint16_t peer_port = 0;
            if (!Stun_DecodeEndpoint(lobby_server_players[i].room_code, &peer_ip, &peer_port))
                break;

            // Auto-decline if player fails filter criteria (region lock, max ping, WiFi block)
            if (!player_passes_filters(&lobby_server_players[i])) {
                // Build a reason string for the status message
                const char* reason = "filter";
                if (Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK)) {
                    const char* my_region = Config_GetString(CFG_KEY_LOBBY_REGION);
                    if (my_region && my_region[0] && lobby_server_players[i].region[0] &&
                        strcmp(my_region, lobby_server_players[i].region) != 0) {
                        reason = "region mismatch";
                    }
                }
                if (Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI) &&
                    strcmp(lobby_server_players[i].connection_type, "wifi") == 0) {
                    reason = "WiFi blocked";
                }
                int max_ping = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
                if (max_ping > 0) {
                    int est = PingProbe_GetRTT(lobby_server_players[i].player_id);
                    if (est < 0 && lobby_server_players[i].rtt_ms > 0)
                        est = lobby_my_rtt_ms + lobby_server_players[i].rtt_ms;
                    if (est > 0 && est > max_ping)
                        reason = "high ping";
                }
                SDL_Log("[lobby] Auto-declined %s (%s)", lobby_server_players[i].display_name, reason);
                snprintf(lobby_status_msg,
                         sizeof(lobby_status_msg),
                         "Declined %s (%s)",
                         lobby_server_players[i].display_name,
                         reason);
                // Report decline to server for rate limiting
                LobbyServer_DeclineInvite(lobby_my_player_id, lobby_server_players[i].player_id);
                add_declined_player(lobby_server_players[i].player_id);
                continue;
            }

            // Always update pending invite state for native lobby indication
            found_invite = true;
            lobby_has_pending_invite = true;
            lobby_pending_invite_time = now;
            snprintf(lobby_pending_invite_name,
                     sizeof(lobby_pending_invite_name),
                     "%s",
                     lobby_server_players[i].display_name);
            snprintf(current_opponent_id, sizeof(current_opponent_id), "%s", lobby_server_players[i].player_id);
            lobby_pending_invite_ip = peer_ip;
            lobby_pending_invite_port = peer_port;
            snprintf(
                lobby_pending_invite_room, sizeof(lobby_pending_invite_room), "%s", lobby_server_players[i].room_code);
            snprintf(
                lobby_pending_invite_region, sizeof(lobby_pending_invite_region), "%s", lobby_server_players[i].region);
            lobby_pending_invite_ft = lobby_server_players[i].ft > 0 ? lobby_server_players[i].ft : 2;

            // Use true P2P RTT from ping probe if available
            int p2p_rtt = PingProbe_GetRTT(lobby_server_players[i].player_id);
            if (p2p_rtt >= 0) {
                lobby_pending_invite_ping = p2p_rtt;
            } else {
                // Fallback: triangulated estimate
                int their_rtt = lobby_server_players[i].rtt_ms;
                if (lobby_my_rtt_ms > 0 && their_rtt > 0)
                    lobby_pending_invite_ping = lobby_my_rtt_ms + their_rtt;
                else
                    lobby_pending_invite_ping = -1;
            }

            if (lobby_auto) {
                snprintf(lobby_status_msg,
                         sizeof(lobby_status_msg),
                         "Auto-accepting %s...",
                         lobby_server_players[i].display_name);
                snprintf(
                    lobby_punch_peer_name, sizeof(lobby_punch_peer_name), "%s", lobby_server_players[i].display_name);
                const char* d2 = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                if (!d2 || !d2[0])
                    d2 = my_room_code;
                AsyncUpdatePresence(lobby_my_player_id, d2, my_room_code, lobby_server_players[i].room_code);
                lobby_has_pending_invite = false; // Consumed
                lobby_we_are_initiator = false;   // They invited us, we auto-accepted
                lobby_start_punch(peer_ip, peer_port);
            }
            // else: popup will show via lobby_has_pending_invite — no eager punch
            break;
        }
    }
    if (!found_invite && lobby_has_pending_invite) {
        // Expire stale invites after 10s to avoid ghost popups when peer disconnects
        if (SDL_GetTicks() - lobby_pending_invite_time > 10000) {
            lobby_has_pending_invite = false;
            lobby_pending_invite_name[0] = '\0';
            current_opponent_id[0] = '\0';
            lobby_pending_invite_region[0] = '\0';
            lobby_pending_invite_ping = -1;
            lobby_pending_invite_ft = 2;
        }
    }
}

// STUN discover thread function
static int SDLCALL stun_discover_thread_fn(void* data) {
    (void)data;
    bool ok = Stun_Discover(&stun_result, 0); // OS assigns free port
    SDL_SetAtomicInt(&lobby_thread_result, ok ? 1 : 0);
    SDL_SetAtomicInt(&lobby_async_state, ok ? LOBBY_ASYNC_READY : LOBBY_ASYNC_STUN_FAILED);
    return 0;
}

static int SDLCALL hole_punch_thread_fn(void* data) {
    (void)data;
    SDL_SetAtomicInt(&lobby_punch_cancel, 0);
    uint32_t start_ms = SDL_GetTicks();
    bool ok = Stun_HolePunch(&stun_result, &lobby_punch_peer_ip, &lobby_punch_peer_port, 10000, &lobby_punch_cancel);
    if (ok) {
        Stun_FormatIP(lobby_punch_peer_ip, lobby_punch_peer_ip_str, sizeof(lobby_punch_peer_ip_str));
        uint32_t rtt_ms = (SDL_GetTicks() - start_ms);
        if (rtt_ms > 200)
            rtt_ms = 200; // Cap at reasonable max for display
        if (rtt_ms < 1)
            rtt_ms = 1;
        lobby_pending_invite_ping = (int)rtt_ms;
    }
    SDL_SetAtomicInt(&lobby_thread_result, ok ? 1 : 0);
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_PUNCH_DONE);
    return 0;
}

// UPnP fallback thread function
static int SDLCALL upnp_fallback_thread_fn(void* data) {
    (void)data;
    bool ok =
        Upnp_AddMapping(&lobby_upnp_mapping, ntohs(stun_result.public_port), ntohs(stun_result.public_port), "UDP");
    SDL_SetAtomicInt(&lobby_thread_result, ok ? 1 : 0);
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_UPNP_DONE);
    return 0;
}

static void lobby_start_discover(void) {
    memset(my_room_code, 0, sizeof(my_room_code));
    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Discovering public endpoint...");
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_DISCOVERING);
    SDL_SetAtomicInt(&lobby_thread_result, 0);
    lobby_thread = SDL_CreateThread(stun_discover_thread_fn, "StunDiscover", NULL);
    if (!lobby_thread) {
        snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Failed to create STUN thread!");
        SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_STUN_FAILED);
    }
}

static void lobby_start_punch(uint32_t peer_ip, uint16_t peer_port) {
    lobby_punch_peer_ip = peer_ip;
    lobby_punch_peer_port = peer_port;
    Stun_FormatIP(peer_ip, lobby_punch_peer_ip_str, sizeof(lobby_punch_peer_ip_str));
    // Show display name in status if available, fall back to IP
    if (lobby_punch_peer_name[0]) {
        snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Hole punching to %s...", lobby_punch_peer_name);
    } else {
        snprintf(lobby_status_msg,
                 sizeof(lobby_status_msg),
                 "Hole punching to %s:%u...",
                 lobby_punch_peer_ip_str,
                 ntohs(peer_port));
    }
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_PUNCHING);
    SDL_SetAtomicInt(&lobby_thread_result, 0);
    lobby_thread = SDL_CreateThread(hole_punch_thread_fn, "HolePunch", NULL);
    if (!lobby_thread) {
        snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Failed to create hole punch thread!");
        SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_READY);
    }
}

static void lobby_cleanup_thread(void) {
    if (lobby_thread) {
        SDL_WaitThread(lobby_thread, NULL);
        lobby_thread = NULL;
    }
}

static void lobby_reset(void) {
    lobby_cleanup_thread();
    // Shut down ping probe before closing the STUN socket it uses
    if (ping_probe_initialized) {
        PingProbe_Shutdown();
        ping_probe_initialized = false;
    }
    Stun_CloseSocket(&stun_result);
    Upnp_RemoveMapping(&lobby_upnp_mapping);
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);

    // Clean up server browser state — only send Leave if no async action is in-flight
    if (lobby_server_registered && lobby_my_player_id[0]) {
        if (SDL_GetAtomicInt(&async_action_active) == 0) {
            AsyncLobbyAction(lobby_my_player_id, 3);
        }
    }
    lobby_server_registered = false;
    lobby_server_searching = false;
    SDL_SetAtomicInt(&lobby_server_player_count, 0);
    lobby_server_last_poll = 0;

    // Clear pending invite state
    lobby_has_pending_invite = false;
    lobby_pending_invite_name[0] = '\0';
    current_opponent_id[0] = '\0';
    lobby_pending_invite_region[0] = '\0';
    lobby_pending_invite_ping = -1;
    lobby_pending_invite_ft = 2;
    SDL_SetAtomicInt(&lobby_punch_cancel, 1); // Cancel any in-flight punch
    lobby_punch_peer_name[0] = '\0';
    lobby_connect_to_intent[0] = '\0';
    // Clear anti-spam declined player list
    declined_player_count = 0;

    // Reset async thread guards so lobby can restart cleanly
    SDL_SetAtomicInt(&async_presence_active, 0);
    SDL_SetAtomicInt(&async_action_active, 0);
}

// FPS history data (owned by sdl_app.c, just pointers here)
static const float* s_fps_history = NULL;
static int s_fps_history_count = 0;
static float s_fps_current = 0.0f;

#define HISTORY_MAX 128
static float ping_history[HISTORY_MAX] = { 0 };
static float rb_history[HISTORY_MAX] = { 0 };
static int history_offset = 0;
static bool history_full = false;

extern "C" {

void SDLNetplayUI_Init() {
    history_offset = 0;
    history_full = false;
    memset(ping_history, 0, sizeof(ping_history));
    memset(rb_history, 0, sizeof(rb_history));
    session_start_ticks = SDL_GetTicks();
}

void SDLNetplayUI_SetHUDVisible(bool visible) {
    hud_visible = visible;
}

bool SDLNetplayUI_IsHUDVisible() {
    return hud_visible;
}

void SDLNetplayUI_SetDiagnosticsVisible(bool visible) {
    diagnostics_visible = visible;
}

bool SDLNetplayUI_IsDiagnosticsVisible() {
    return diagnostics_visible;
}

void SDLNetplayUI_GetHUDText(char* buffer, size_t size) {
    NetworkStats stats;
    Netplay_GetNetworkStats(&stats);
    // Format matches official netstats_renderer: "R:%d P:%d"
    snprintf(buffer, size, "R:%d P:%d", stats.rollback, stats.ping);
}

void SDLNetplayUI_GetHistory(float* ping_hist, float* rb_hist, int* count) {
    int current_count = history_full ? HISTORY_MAX : history_offset;
    if (count)
        *count = current_count;
    for (int i = 0; i < current_count; i++) {
        int idx = history_full ? (history_offset + i) % HISTORY_MAX : i;
        if (ping_hist)
            ping_hist[i] = ping_history[idx];
        if (rb_hist)
            rb_hist[i] = rb_history[idx];
    }
}

void SDLNetplayUI_ProcessEvent(const SDL_Event* event) {
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        if (event->key.key == SDLK_F10) {
            diagnostics_visible = !diagnostics_visible;
        }
    }
}

void SDLNetplayUI_SetFPSHistory(const float* data, int count, float current_fps) {
    s_fps_history = data;
    s_fps_history_count = count;
    s_fps_current = current_fps;
}

const float* SDLNetplayUI_GetFPSHistory(int* out_count) {
    if (out_count)
        *out_count = s_fps_history_count;
    return s_fps_history;
}

float SDLNetplayUI_GetCurrentFPS() {
    return s_fps_current;
}

} // extern "C"

struct Toast {
    char message[64];
    float duration; // < 0 means infinite
    float timer;
    bool active;
    NetplayEventType associated_event; // To identify and remove specific types
};

#define MAX_TOASTS 5
static Toast toasts[MAX_TOASTS] = {};

static void AddToast(const char* msg, float duration, NetplayEventType event_type) {
    for (int i = 0; i < MAX_TOASTS; ++i) {
        if (!toasts[i].active) {
            snprintf(toasts[i].message, sizeof(toasts[i].message), "%s", msg);
            toasts[i].duration = duration;
            toasts[i].timer = 0.0f;
            toasts[i].active = true;
            toasts[i].associated_event = event_type;
            return;
        }
    }
}

static void RemoveToastsByType(NetplayEventType type) {
    for (int i = 0; i < MAX_TOASTS; ++i) {
        if (toasts[i].active && toasts[i].associated_event == type) {
            toasts[i].active = false;
        }
    }
}

extern "C" {

int SDLNetplayUI_GetActiveToastCount() {
    int count = 0;
    for (int i = 0; i < MAX_TOASTS; ++i) {
        if (toasts[i].active)
            count++;
    }
    return count;
}

} // extern "C"

static void ProcessEvents() {
    NetplayEvent event;
    while (Netplay_PollEvent(&event)) {
        switch (event.type) {
        case NETPLAY_EVENT_SYNCHRONIZING:
            AddToast("Synchronizing...", -1.0f, NETPLAY_EVENT_SYNCHRONIZING);
            break;
        case NETPLAY_EVENT_CONNECTED:
            RemoveToastsByType(NETPLAY_EVENT_SYNCHRONIZING);
            AddToast("Player Connected", 3.0f, NETPLAY_EVENT_CONNECTED);
            // Reset session start on connection
            session_start_ticks = SDL_GetTicks();
            break;
        case NETPLAY_EVENT_DISCONNECTED:
            RemoveToastsByType(NETPLAY_EVENT_SYNCHRONIZING); // Just in case
            AddToast("Player Disconnected", 3.0f, NETPLAY_EVENT_DISCONNECTED);
            break;
        default:
            break;
        }
    }
}

// RenderToasts() removed — replaced by RmlUI netplay overlay.

static void PushHistory(float ping, float rb) {
    ping_history[history_offset] = ping;
    rb_history[history_offset] = rb;
    history_offset = (history_offset + 1) % HISTORY_MAX;
    if (history_offset == 0)
        history_full = true;
}

// RenderDiagnostics() removed — replaced by RmlUI netplay overlay.

extern "C" {

void SDLNetplayUI_Render(int window_width, int window_height) {
    ProcessEvents();

    // Detect RUNNING -> EXITING transition for match reporting and auto-saving
    NetplaySessionState current_state = Netplay_GetSessionState();
    if (last_session_state == NETPLAY_SESSION_RUNNING && current_state == NETPLAY_SESSION_EXITING &&
        !match_result_reported) {
        
        // Unconditionally auto-save replay for all netplay matches (including direct P2P)
        NativeSave_AutoSaveReplay();

        // A netplay match just ended — report the result if we are in a lobby context
        // Winner_id: 0 = P1 won, 1 = P2 won (from game engine)
        // My_char[0/1]: character indices
        // PL_Wins[0/1]: round wins per player
        int my_player = Netplay_GetPlayerNumber();
        int total_rounds = PL_Wins[0] + PL_Wins[1];

        // Only report to server if both players have verified lobby identities
        if (lobby_my_player_id[0] && current_opponent_id[0]) {
            if (Winner_id >= 0 && total_rounds > 0) {
                // Determine winner's player_id
                const char* winner_pid = (Winner_id == my_player) ? lobby_my_player_id : current_opponent_id;

                const char* room_code = rmlui_casual_lobby_get_room_code();
                const char* match_source = (room_code && room_code[0]) ? "casual" : "ranked";
                int match_ft = Netplay_GetNegotiatedFT();

                AsyncReportMatch(lobby_my_player_id,
                                 current_opponent_id,
                                 winner_pid,
                                 My_char[my_player],
                                 My_char[1 - my_player],
                                 total_rounds,
                                 match_source,
                                 match_ft);
                SDL_Log("[NetplayUI] Match result queued: winner=%s rounds=%d", winner_pid, total_rounds);

                // If inside a casual lobby room, report match end for Winner Stays On rotation
                if (room_code && room_code[0]) {
                    LobbyServer_ReportMatchEnd(room_code, winner_pid);
                    SDL_Log("[NetplayUI] Casual lobby match end reported: room=%s winner=%s", room_code, winner_pid);
                }
            } else {
                // No natural conclusion — opponent likely ragequit/disconnected mid-match.
                // Report the disconnect so the server can track it.
                AsyncReportDisconnect(lobby_my_player_id, current_opponent_id);
                SDL_Log("[NetplayUI] Mid-match disconnect reported: opponent=%s", current_opponent_id);
            }
        }
        match_result_reported = true;
    }

    // Natural match completion: called from VS_Result auto-skip while game state is still valid.
    // This is separate from the RUNNING→EXITING detection above because natural match ends
    // don't transition to EXITING — they stay RUNNING and cycle back to character select.
    // See SDLNetplayUI_ReportNaturalMatchEnd() below for the entry point.

    // Reset flag when starting a new session
    if (current_state == NETPLAY_SESSION_RUNNING && last_session_state != NETPLAY_SESSION_RUNNING) {
        match_result_reported = false;
    }
    last_session_state = current_state;

    NetworkStats stats;
    Netplay_GetNetworkStats(&stats);
    PushHistory((float)stats.ping, (float)stats.rollback);

    // HUD is handled by RmlUi (rmlui_netplay_ui_update in render_overlays)

    if (Netplay_GetSessionState() == NETPLAY_SESSION_LOBBY) {
        int state = SDL_GetAtomicInt(&lobby_async_state);

        // Auto-start STUN discovery on lobby entry
        if (state == LOBBY_ASYNC_IDLE) {
            // Detect WiFi/wired once at lobby entry
            my_connection_type = NetDetect_GetConnectionType();
            SDL_Log("[lobby] Connection type detected: %s", my_connection_type);
            lobby_start_discover();
            state = SDL_GetAtomicInt(&lobby_async_state);
        }

        // Handle STUN discovery completion
        if (state == LOBBY_ASYNC_READY && my_room_code[0] == '\0') {
            lobby_cleanup_thread();
            Stun_EncodeEndpoint(stun_result.public_ip, stun_result.public_port, my_room_code);
            snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Ready.");

            // Initialize P2P ping probe on the STUN socket
            if (!ping_probe_initialized && stun_result.socket_fd >= 0) {
                PingProbe_Init(stun_result.socket_fd);
                ping_probe_initialized = true;
            }

            // Register with lobby server if configured
            if (LobbyServer_IsConfigured() && !lobby_server_registered) {
                // Use stable identity from identity.c (auto-generated on first launch)
                const char* client_id = Identity_GetPlayerId();
                const char* display = Identity_GetDisplayName();

                snprintf(lobby_my_player_id, sizeof(lobby_my_player_id), "%s", client_id);
                if (!lobby_server_registered) {
                    AsyncUpdatePresence(lobby_my_player_id, display, my_room_code, "");
                    lobby_server_registered = true;
                    // Auto-start searching if configured
                    if (Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH) && !lobby_server_searching) {
                        AsyncLobbyAction(lobby_my_player_id, 1);
                        lobby_server_searching = true;
                        lobby_server_last_poll = 0;
                    }
                }
            }
        }

        // Handle STUN failure
        if (state == LOBBY_ASYNC_STUN_FAILED) {
            lobby_cleanup_thread();
            snprintf(lobby_status_msg, sizeof(lobby_status_msg), "STUN failed. LAN discovery still active.");
        }

        // Handle hole punch completion
        if (state == LOBBY_ASYNC_PUNCH_DONE) {
            lobby_cleanup_thread();
            bool punch_ok = (SDL_GetAtomicInt(&lobby_thread_result) == 1);
            if (punch_ok) {
                // Punch succeeded — prepare connection parameters
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Hole punch success!");
                Netplay_SetRemoteIP(lobby_punch_peer_ip_str);
                Netplay_SetRemotePort(ntohs(lobby_punch_peer_port));
                Netplay_SetLocalPort(stun_result.local_port);
                Stun_SetNonBlocking(&stun_result);
                Netplay_SetStunSocket(stun_result.socket_fd);
                stun_result.socket_fd = -1; // Ownership transferred; prevent double-close
                Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);

                if (lobby_we_are_initiator) {
                    // Wait for receiver to accept before starting Gekko
                    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Waiting for opponent to accept...");
                    lobby_wait_peer_start = SDL_GetTicks();
                    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_WAIT_PEER);
                } else {
                    // Receiver: we already accepted — proceed immediately
                    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                    Netplay_Begin();
                }
            } else {
                // Punch failed — try UPnP fallback
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Hole punch failed. Trying UPnP port forward...");
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_UPNP_TRYING);
                SDL_SetAtomicInt(&lobby_thread_result, 0);
                lobby_thread = SDL_CreateThread(upnp_fallback_thread_fn, "UPnPFallback", NULL);
                if (!lobby_thread) {
                    snprintf(lobby_status_msg,
                             sizeof(lobby_status_msg),
                             "UPnP thread failed. Attempting connection anyway...");
                    Netplay_SetRemoteIP(lobby_punch_peer_ip_str);
                    Netplay_SetRemotePort(ntohs(lobby_punch_peer_port));
                    Netplay_SetLocalPort(stun_result.local_port);
                    Stun_SetNonBlocking(&stun_result);
                    Netplay_SetStunSocket(stun_result.socket_fd);
                    stun_result.socket_fd = -1;
                    Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);

                    if (lobby_we_are_initiator) {
                        snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Waiting for opponent to accept...");
                        lobby_wait_peer_start = SDL_GetTicks();
                        SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_WAIT_PEER);
                    } else {
                        SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                        Netplay_Begin();
                    }
                }
            }
        }

        // Handle UPnP completion
        if (state == LOBBY_ASYNC_UPNP_DONE) {
            lobby_cleanup_thread();
            bool upnp_ok = (SDL_GetAtomicInt(&lobby_thread_result) == 1);
            if (upnp_ok) {
                snprintf(lobby_status_msg,
                         sizeof(lobby_status_msg),
                         "UPnP port forward success!");
            } else {
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "UPnP failed. Attempting direct connection...");
            }
            Netplay_SetRemoteIP(lobby_punch_peer_ip_str);
            Netplay_SetRemotePort(ntohs(lobby_punch_peer_port));
            Netplay_SetLocalPort(stun_result.local_port);
            Stun_SetNonBlocking(&stun_result);
            Netplay_SetStunSocket(stun_result.socket_fd);
            stun_result.socket_fd = -1;
            Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);

            if (lobby_we_are_initiator) {
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Waiting for opponent to accept...");
                lobby_wait_peer_start = SDL_GetTicks();
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_WAIT_PEER);
            } else {
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                Netplay_Begin();
            }
        }

        // Handle wait-for-peer (initiator only): poll server for receiver's acceptance
        if (state == LOBBY_ASYNC_WAIT_PEER) {
            uint32_t elapsed = SDL_GetTicks() - lobby_wait_peer_start;
            bool peer_accepted = false;

            // Check if the receiver has set connect_to pointing at us
            int pc = SDL_GetAtomicInt(&lobby_server_player_count);
            for (int i = 0; i < pc; i++) {
                if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
                    continue;
                if (lobby_server_players[i].connect_to[0] &&
                    strcmp(lobby_server_players[i].connect_to, my_room_code) == 0) {
                    peer_accepted = true;
                    break;
                }
            }

            if (peer_accepted) {
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Opponent accepted! Connecting...");
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                Netplay_Begin();
            } else if (elapsed >= LOBBY_WAIT_PEER_TIMEOUT_MS) {
                // Timeout — proceed anyway as best-effort fallback
                SDL_Log("[lobby] Wait-for-peer timed out after %ums, proceeding anyway", elapsed);
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Peer timeout. Connecting...");
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                Netplay_Begin();
            }
            // else: keep waiting, status msg already set
        }

        // Re-read state after transitions
        state = SDL_GetAtomicInt(&lobby_async_state);

        // Run server polling/auto-connect regardless of which lobby UI is active
        lobby_poll_server();

    } else {
        // Don't tear down lobby when we're mid-match inside a casual room —
        // we still need presence heartbeat to keep server membership alive.
        const char* active_room = rmlui_casual_lobby_get_room_code();
        if (active_room && active_room[0]) {
            // Keep heartbeat alive for room membership during casual match
            uint32_t now = SDL_GetTicks();
            if (lobby_server_registered && lobby_my_player_id[0] &&
                now - lobby_server_last_poll >= LOBBY_POLL_INTERVAL_MS) {
                const char* display = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                if (!display || !display[0])
                    display = my_room_code;
                AsyncUpdatePresence(lobby_my_player_id, display, my_room_code, "");
                lobby_server_last_poll = now;
            }
        } else {
            // No active casual room — safe to tear down
            if (SDL_GetAtomicInt(&lobby_async_state) != LOBBY_ASYNC_IDLE) {
                lobby_reset();
            }
        }
    }

    // ImGui toasts and diagnostics — RmlUi now handles the F10 diagnostics panel
    // on all backends, so skip ImGui rendering. Keep the calls for when use_rmlui
    // is false and backend is not SDL2D (legacy fallback path, if needed).
    // In practice this is dead code since RmlUi Fx menus are always active.
}

void SDLNetplayUI_Shutdown() {}

// === Native lobby bridge implementations ===

void SDLNetplayUI_SetNativeLobbyActive(bool active) {
    native_lobby_active = active;
}
bool SDLNetplayUI_IsNativeLobbyActive() {
    return native_lobby_active;
}

const char* SDLNetplayUI_GetStatusMsg() {
    return lobby_status_msg;
}
const char* SDLNetplayUI_GetRoomCode() {
    return my_room_code;
}

bool SDLNetplayUI_IsDiscovering() {
    int s = SDL_GetAtomicInt(&lobby_async_state);
    return (s == LOBBY_ASYNC_DISCOVERING);
}

bool SDLNetplayUI_IsReady() {
    int s = SDL_GetAtomicInt(&lobby_async_state);
    return (s == LOBBY_ASYNC_READY || my_room_code[0] != '\0');
}

void SDLNetplayUI_StartSearch() {
    if (lobby_server_searching || !lobby_server_registered)
        return;
    AsyncLobbyAction(lobby_my_player_id, 1);
    lobby_server_searching = true;
    lobby_server_last_poll = 0;
}

void SDLNetplayUI_StopSearch() {
    if (!lobby_server_searching)
        return;
    AsyncLobbyAction(lobby_my_player_id, 2);
    lobby_server_searching = false;
    SDL_SetAtomicInt(&lobby_server_player_count, 0);
}

bool SDLNetplayUI_IsSearching() {
    return lobby_server_searching;
}

int SDLNetplayUI_GetOnlinePlayerCount() {
    // Count only searching players, excluding ourselves and filtered players
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        count++;
    }
    return count;
}

const char* SDLNetplayUI_GetOnlinePlayerName(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].display_name;
        count++;
    }
    return "";
}

const char* SDLNetplayUI_GetOnlinePlayerRoomCode(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].room_code;
        count++;
    }
    return "";
}

const char* SDLNetplayUI_GetOnlinePlayerRegion(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].region;
        count++;
    }
    return "";
}

const char* SDLNetplayUI_GetOnlinePlayerCountry(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].country;
        count++;
    }
    return "";
}

const char* SDLNetplayUI_GetOnlinePlayerConnType(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].connection_type;
        count++;
    }
    return "unknown";
}

int SDLNetplayUI_GetOnlinePlayerPing(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index) {
            // Use true P2P RTT from ping probe if available
            int p2p_rtt = PingProbe_GetRTT(lobby_server_players[i].player_id);
            if (p2p_rtt >= 0)
                return p2p_rtt;
            // Fallback: triangulated estimate
            int their_rtt = lobby_server_players[i].rtt_ms;
            if (lobby_my_rtt_ms > 0 && their_rtt > 0)
                return lobby_my_rtt_ms + their_rtt;
            return -1;
        }
        count++;
    }
    return -1;
}

void SDLNetplayUI_ConnectToPlayer(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index) {
            uint32_t peer_ip = 0;
            uint16_t peer_port = 0;
            if (Stun_DecodeEndpoint(lobby_server_players[i].room_code, &peer_ip, &peer_port)) {
                // Signal intent via lobby server
                const char* display_ct = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                if (!display_ct || !display_ct[0])
                    display_ct = my_room_code;
                AsyncUpdatePresence(lobby_my_player_id, display_ct, my_room_code, lobby_server_players[i].room_code);
                snprintf(
                    lobby_connect_to_intent, sizeof(lobby_connect_to_intent), "%s", lobby_server_players[i].room_code);
                snprintf(
                    lobby_punch_peer_name, sizeof(lobby_punch_peer_name), "%s", lobby_server_players[i].display_name);
                snprintf(current_opponent_id, sizeof(current_opponent_id), "%s", lobby_server_players[i].player_id);
                lobby_we_are_initiator = true; // We clicked Connect
                lobby_start_punch(peer_ip, peer_port);
            }
            return;
        }
        count++;
    }
}

bool SDLNetplayUI_HasPendingInvite() {
    return lobby_has_pending_invite;
}
const char* SDLNetplayUI_GetPendingInviteName() {
    return lobby_pending_invite_name;
}
const char* SDLNetplayUI_GetPendingInviteRegion() {
    return lobby_pending_invite_region;
}
int SDLNetplayUI_GetPendingInvitePing() {
    return lobby_pending_invite_ping;
}
int SDLNetplayUI_GetPendingInviteFT() {
    return lobby_pending_invite_ft;
}

int SDLNetplayUI_GetOnlinePlayerFT(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
            continue;
        if (!player_passes_filters(&lobby_server_players[i]))
            continue;
        if (count == index)
            return lobby_server_players[i].ft > 0 ? lobby_server_players[i].ft : 2;
        count++;
    }
    return 2;
}

void SDLNetplayUI_AcceptPendingInvite() {
    if (!lobby_has_pending_invite)
        return;
    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Connecting to %s...", lobby_pending_invite_name);
    snprintf(lobby_punch_peer_name, sizeof(lobby_punch_peer_name), "%s", lobby_pending_invite_name);
    const char* d2 = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
    if (!d2 || !d2[0])
        d2 = my_room_code;
    AsyncUpdatePresence(lobby_my_player_id, d2, my_room_code, lobby_pending_invite_room);
    snprintf(lobby_connect_to_intent, sizeof(lobby_connect_to_intent), "%s", lobby_pending_invite_room);
    lobby_has_pending_invite = false;
    lobby_we_are_initiator = false;
    lobby_start_punch(lobby_pending_invite_ip, lobby_pending_invite_port);
}

void SDLNetplayUI_DeclinePendingInvite() {
    if (!lobby_has_pending_invite)
        return;
    // Report decline to server for rate limiting
    // Find the player_id of the inviting player
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].display_name, lobby_pending_invite_name) == 0 &&
            strcmp(lobby_server_players[i].player_id, lobby_my_player_id) != 0) {
            LobbyServer_DeclineInvite(lobby_my_player_id, lobby_server_players[i].player_id);
            add_declined_player(lobby_server_players[i].player_id);
            break;
        }
    }
    lobby_has_pending_invite = false;
    lobby_pending_invite_name[0] = '\0';
    lobby_pending_invite_region[0] = '\0';
    lobby_pending_invite_ping = -1;
    lobby_pending_invite_ft = 2;
    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Declined invite.");
}

bool SDLNetplayUI_HasOutgoingChallenge() {
    int state = SDL_GetAtomicInt(&lobby_async_state);
    return lobby_we_are_initiator && (state == LOBBY_ASYNC_PUNCHING || state == LOBBY_ASYNC_UPNP_TRYING ||
                                      state == LOBBY_ASYNC_WAIT_PEER);
}

const char* SDLNetplayUI_GetOutgoingChallengeName() {
    return lobby_punch_peer_name;
}

int SDLNetplayUI_GetOutgoingChallengePing() {
    return lobby_pending_invite_ping;
}

void SDLNetplayUI_CancelOutgoingChallenge() {
    // Signal the hole punch thread to abort
    SDL_SetAtomicInt(&lobby_punch_cancel, 1);

    // Clear connection intent
    lobby_connect_to_intent[0] = '\0';
    lobby_we_are_initiator = false;
    lobby_punch_peer_name[0] = '\0';

    // If we were in WAIT_PEER, release the STUN socket that was transferred
    // to Netplay during the punch phase (Netplay_Begin was never called).
    int cancel_state = SDL_GetAtomicInt(&lobby_async_state);
    if (cancel_state == LOBBY_ASYNC_WAIT_PEER) {
        Netplay_SetStunSocket(-1); // Release ownership back
    }

    // Update server presence to clear connect_to
    const char* display = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
    if (!display || !display[0])
        display = my_room_code;
    AsyncUpdatePresence(lobby_my_player_id, display, my_room_code, "");

    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Challenge cancelled.");

    // Wait for thread to finish, then reset state to READY
    lobby_cleanup_thread();
    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_READY);
}

// === Phase 6: Casual lobby bridge functions ===

bool SDLNetplayUI_PlayerPassesFilters(const char* conn_type, int rtt_ms, const char* region) {
    // Build a temporary LobbyPlayer with the provided values and check filters
    LobbyPlayer temp;
    memset(&temp, 0, sizeof(temp));
    if (conn_type)
        snprintf(temp.connection_type, sizeof(temp.connection_type), "%s", conn_type);
    temp.rtt_ms = rtt_ms;
    if (region)
        snprintf(temp.region, sizeof(temp.region), "%s", region);
    return player_passes_filters(&temp);
}

void SDLNetplayUI_StartCasualMatchPunch(const char* opponent_room_code, const char* opponent_name,
                                        const char* opponent_player_id, bool we_are_p1) {
    if (!opponent_room_code || !opponent_room_code[0])
        return;

    // Track opponent explicit ID so we can report match results to the lobby server correctly
    if (opponent_player_id && opponent_player_id[0]) {
        snprintf(current_opponent_id, sizeof(current_opponent_id), "%s", opponent_player_id);
    } else {
        current_opponent_id[0] = '\0';
    }

    // Decode the opponent's STUN endpoint for later use (hole punch or LAN detection)
    uint32_t peer_ip = 0;
    uint16_t peer_port = 0;
    bool decoded = Stun_DecodeEndpoint(opponent_room_code, &peer_ip, &peer_port);

    // Check if opponent is on LAN — use direct connection if so (skip STUN hole punch)
    if (decoded) {
        NetplayDiscoveredPeer lan_peers[16];
        int lan_count = Discovery_GetPeers(lan_peers, 16);
        for (int i = 0; i < lan_count; i++) {
            // Match by display name (primary) — the beacon now includes identity names
            bool name_match = (opponent_name && opponent_name[0] && lan_peers[i].display_name[0] &&
                               strcmp(lan_peers[i].display_name, opponent_name) == 0);
            // Fallback: if the peer shares our STUN public IP, they're on the same LAN
            bool ip_match = (stun_result.public_ip != 0 && peer_ip == stun_result.public_ip);

            if ((name_match || ip_match) && lan_peers[i].ip[0]) {
                // Use the peer's STUN port (from their room code), NOT the discovery
                // beacon port (configuration.netplay.port). The casual lobby path uses
                // the STUN socket for Gekko, so both sides listen on their STUN port.
                uint16_t remote_port = ntohs(peer_port);
                SDL_Log("[casual] LAN peer detected: %s at %s:%u — using direct connection",
                        opponent_name ? opponent_name : lan_peers[i].display_name,
                        lan_peers[i].ip,
                        remote_port);
                snprintf(lobby_status_msg,
                         sizeof(lobby_status_msg),
                         "LAN: connecting to %s...",
                         opponent_name ? opponent_name : "peer");
                Netplay_SetRemoteIP(lan_peers[i].ip);
                Netplay_SetRemotePort(remote_port);
                Netplay_SetLocalPort(stun_result.local_port);
                // Transfer the STUN socket to Gekko for the LAN connection
                if (stun_result.socket_fd >= 0) {
                    Stun_SetNonBlocking(&stun_result);
                    Netplay_SetStunSocket(stun_result.socket_fd);
                    stun_result.socket_fd = -1; // Ownership transferred
                }
                Netplay_SetPlayerNumber(we_are_p1 ? 0 : 1);
                Netplay_Begin();
                return;
            }
        }
    }

    if (!decoded) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "[casual] Failed to decode opponent room code: %s", opponent_room_code);
        return;
    }

    // Set up lobby state for the punch
    if (opponent_name)
        snprintf(lobby_punch_peer_name, sizeof(lobby_punch_peer_name), "%s", opponent_name);
    lobby_we_are_initiator = we_are_p1;

    snprintf(
        lobby_status_msg, sizeof(lobby_status_msg), "Connecting to %s...", opponent_name ? opponent_name : "opponent");

    lobby_start_punch(peer_ip, peer_port);
}

void SDLNetplayUI_ReportNaturalMatchEnd(void) {
    // Called from VS_Result auto-skip while game state (Winner_id, PL_Wins, My_char)
    // is still valid. For natural match completion, the session stays RUNNING and
    // cycles back to character select — so the RUNNING→EXITING detection in
    // SDLNetplayUI_Render never fires.

    if (match_result_reported)
        return;

    // Auto-save replay locally
    NativeSave_AutoSaveReplay();

    int my_player = Netplay_GetPlayerNumber();
    int total_rounds = PL_Wins[0] + PL_Wins[1];

    if (lobby_my_player_id[0] && current_opponent_id[0]) {
        if (Winner_id >= 0 && total_rounds > 0) {
            const char* winner_pid = (Winner_id == my_player) ? lobby_my_player_id : current_opponent_id;

            const char* room_code = rmlui_casual_lobby_get_room_code();
            const char* match_source = (room_code && room_code[0]) ? "casual" : "ranked";
            int match_ft = Netplay_GetNegotiatedFT();

            AsyncReportMatch(lobby_my_player_id,
                             current_opponent_id,
                             winner_pid,
                             My_char[my_player],
                             My_char[1 - my_player],
                             total_rounds,
                             match_source,
                             match_ft);
            SDL_Log("[NetplayUI] Natural match end: winner=%s rounds=%d", winner_pid, total_rounds);

            // If inside a casual lobby room, report match end for Winner Stays On rotation
            if (room_code && room_code[0]) {
                LobbyServer_ReportMatchEnd(room_code, winner_pid);
                SDL_Log("[NetplayUI] Casual lobby match end reported: room=%s winner=%s", room_code, winner_pid);
            }
        }
    }

    match_result_reported = true;
}

} // extern "C"
