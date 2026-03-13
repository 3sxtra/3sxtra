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
#include "netplay/stun.h"
#include "netplay/upnp.h"
#include "port/config/config.h"

// Master RmlUi toggle + per-component toggles
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
static uint32_t lobby_pending_invite_time = 0; // Timestamp when invite was detected (for expiry)
static char lobby_my_player_id[64] = { 0 };
static int lobby_my_rtt_ms = -1; // Our measured RTT to the lobby server
static const char* my_connection_type = "unknown"; // Detected once at lobby entry
#define LOBBY_POLL_INTERVAL_MS 2000

// Match reporting state
static NetplaySessionState last_session_state = NETPLAY_SESSION_IDLE;
static bool match_result_reported = false;
static SDL_AtomicInt async_match_report_active = { 0 };

static int async_match_report_fn(void* userdata) {
    MatchResult* result = (MatchResult*)userdata;
    LobbyServer_ReportMatch(result);
    free(result);
    SDL_SetAtomicInt(&async_match_report_active, 0);
    return 0;
}

static void AsyncReportMatch(const char* my_id, const char* opponent_id,
                              const char* winner_id, int my_char, int opp_char, int rounds) {
    if (!LobbyServer_IsConfigured() || !my_id || !my_id[0])
        return;
    if (SDL_GetAtomicInt(&async_match_report_active) != 0)
        return;
    SDL_SetAtomicInt(&async_match_report_active, 1);

    MatchResult* r = (MatchResult*)malloc(sizeof(MatchResult));
    memset(r, 0, sizeof(*r));
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

    SDL_Thread* t = SDL_CreateThread(async_match_report_fn, "AsyncMatchReport", r);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(r);
        SDL_SetAtomicInt(&async_match_report_active, 0);
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
    if (cooldown_ms <= 0) cooldown_ms = DEFAULT_INVITE_COOLDOWN_MS;
    else cooldown_ms *= 1000; // config is in seconds

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
            if (now < declined_players[i].cooldown_until) return true;
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
            if (strcmp(my_region, p->region) != 0) return false;
        }
    }
    // Max ping
    int max_ping = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
    if (max_ping > 0 && p->rtt_ms > 0) {
        // Estimate P2P ping as our RTT + their RTT
        int estimated_p2p = lobby_my_rtt_ms + p->rtt_ms;
        if (estimated_p2p > max_ping) return false;
    }
    // Block WiFi
    if (Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI)) {
        if (strcmp(p->connection_type, "wifi") == 0) return false;
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
static int lobby_pending_invite_ping = -1;       // ms, -1 = unknown
static SDL_AtomicInt lobby_punch_cancel = { 0 }; // Set to 1 to cancel in-progress hole punch
static bool lobby_we_are_initiator = false;      // true = we clicked Connect, false = they invited us
static char lobby_connect_to_intent[16] = { 0 }; // Current connect_to value preserved across heartbeats

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
} AsyncPresenceData;

static SDL_AtomicInt async_presence_active = { 0 };

static int SDLCALL async_presence_fn(void* data) {
    AsyncPresenceData* d = (AsyncPresenceData*)data;
    LobbyServer_UpdatePresence(d->player_id, d->display_name, d->region, d->room_code, d->connect_to, d->rtt_ms,
                               d->connection_type);
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

    // Measure HTTP RTT to the lobby server
    uint32_t t0 = SDL_GetTicks();
    int count = LobbyServer_GetSearching(temp_players, 16, NULL);
    uint32_t t1 = SDL_GetTicks();
    lobby_my_rtt_ms = (int)(t1 - t0);

    SDL_MemoryBarrierRelease();
    memcpy(lobby_server_players, temp_players, sizeof(temp_players));
    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&lobby_server_player_count, count);

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
                SDL_Log("[lobby] Ignoring invite from %s (declined cooldown active)", lobby_server_players[i].display_name);
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
                if (max_ping > 0 && lobby_server_players[i].rtt_ms > 0) {
                    int est = lobby_my_rtt_ms + lobby_server_players[i].rtt_ms;
                    if (est > max_ping)
                        reason = "high ping";
                }
                SDL_Log("[lobby] Auto-declined %s (%s)", lobby_server_players[i].display_name, reason);
                snprintf(lobby_status_msg, sizeof(lobby_status_msg),
                         "Declined %s (%s)", lobby_server_players[i].display_name, reason);
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
            lobby_pending_invite_ip = peer_ip;
            lobby_pending_invite_port = peer_port;
            snprintf(
                lobby_pending_invite_room, sizeof(lobby_pending_invite_room), "%s", lobby_server_players[i].room_code);
            snprintf(
                lobby_pending_invite_region, sizeof(lobby_pending_invite_region), "%s", lobby_server_players[i].region);

            // Estimate P2P ping from server RTTs: our_rtt + their_rtt
            int their_rtt = lobby_server_players[i].rtt_ms;
            if (lobby_my_rtt_ms > 0 && their_rtt > 0)
                lobby_pending_invite_ping = lobby_my_rtt_ms + their_rtt;
            else
                lobby_pending_invite_ping = -1;

            if (lobby_auto) {
                snprintf(lobby_status_msg,
                         sizeof(lobby_status_msg),
                         "Auto-connecting to %s...",
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
            lobby_pending_invite_region[0] = '\0';
            lobby_pending_invite_ping = -1;
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
    lobby_pending_invite_region[0] = '\0';
    lobby_pending_invite_ping = -1;
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

    // Detect RUNNING -> EXITING transition for match reporting
    NetplaySessionState current_state = Netplay_GetSessionState();
    if (last_session_state == NETPLAY_SESSION_RUNNING &&
        current_state == NETPLAY_SESSION_EXITING &&
        !match_result_reported && lobby_my_player_id[0]) {
        // A netplay match just ended — report the result
        // Winner_id: 0 = P1 won, 1 = P2 won (from game engine)
        // My_char[0/1]: character indices
        // PL_Wins[0/1]: round wins per player
        int my_player = 0; // Local player is always P1 in netplay
        int total_rounds = PL_Wins[0] + PL_Wins[1];

        if (Winner_id >= 0 && total_rounds > 0) {
            // Determine winner's player_id
            // If we are P1 (player 0) and Winner_id==0, we won
            const char* winner_pid = (Winner_id == my_player) ?
                lobby_my_player_id : "opponent"; // opponent ID comes from lobby

            // Find opponent's ID from the lobby player list
            const char* opponent_pid = "";
            int player_count = SDL_GetAtomicInt(&lobby_server_player_count);
            for (int i = 0; i < player_count; i++) {
                if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) != 0 &&
                    lobby_server_players[i].player_id[0]) {
                    opponent_pid = lobby_server_players[i].player_id;
                    break;
                }
            }

            if (opponent_pid[0]) {
                winner_pid = (Winner_id == my_player) ? lobby_my_player_id : opponent_pid;
                AsyncReportMatch(lobby_my_player_id, opponent_pid, winner_pid,
                                 My_char[my_player], My_char[1 - my_player], total_rounds);
                SDL_Log("[NetplayUI] Match result queued: winner=%s rounds=%d", winner_pid, total_rounds);
            }
        }
        match_result_reported = true;
    }

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
                // Punch succeeded — hand off the punched socket to GekkoNet
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Hole punch success! Connecting...");
                Netplay_SetRemoteIP(lobby_punch_peer_ip_str);
                Netplay_SetRemotePort(ntohs(lobby_punch_peer_port));
                Netplay_SetLocalPort(stun_result.local_port);
                Stun_SetNonBlocking(&stun_result);
                Netplay_SetStunSocket(stun_result.socket_fd);
                stun_result.socket_fd = -1; // Ownership transferred; prevent double-close
                SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);
                Netplay_Begin();
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
                    SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
                    Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);
                    Netplay_Begin();
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
                         "UPnP port forward success! Connecting via %s:%u...",
                         lobby_upnp_mapping.external_ip,
                         lobby_upnp_mapping.external_port);
            } else {
                snprintf(lobby_status_msg, sizeof(lobby_status_msg), "UPnP failed. Attempting direct connection...");
            }
            Netplay_SetRemoteIP(lobby_punch_peer_ip_str);
            Netplay_SetRemotePort(ntohs(lobby_punch_peer_port));
            Netplay_SetLocalPort(stun_result.local_port);
            Stun_SetNonBlocking(&stun_result);
            Netplay_SetStunSocket(stun_result.socket_fd);
            stun_result.socket_fd = -1;
            SDL_SetAtomicInt(&lobby_async_state, LOBBY_ASYNC_IDLE);
            Netplay_SetPlayerNumber(lobby_we_are_initiator ? 0 : 1);
            Netplay_Begin();
        }

        // Re-read state after transitions
        state = SDL_GetAtomicInt(&lobby_async_state);

        // Run server polling/auto-connect regardless of which lobby UI is active
        lobby_poll_server();

    } else {
        // Reset lobby state when not in lobby
        if (SDL_GetAtomicInt(&lobby_async_state) != LOBBY_ASYNC_IDLE) {
            lobby_reset();
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
            // Estimate P2P ping as sum of both RTTs
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
    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Declined invite.");
}

bool SDLNetplayUI_HasOutgoingChallenge() {
    int state = SDL_GetAtomicInt(&lobby_async_state);
    return lobby_we_are_initiator && (state == LOBBY_ASYNC_PUNCHING || state == LOBBY_ASYNC_UPNP_TRYING);
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

} // extern "C"
