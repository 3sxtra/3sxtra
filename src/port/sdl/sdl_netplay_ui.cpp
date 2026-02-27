/**
 * @file sdl_netplay_ui.cpp
 * @brief Netplay HUD, diagnostics overlay, and toast notification system.
 *
 * Renders ping/rollback history graphs, connection status indicators,
 * and timed toast messages using ImGui during netplay sessions.
 */
#include "port/sdl/sdl_netplay_ui.h"
#include "imgui.h"
#include "netplay/netplay.h"
#include <SDL3/SDL.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "netplay/discovery.h"
#include "netplay/lobby_server.h"
#include "netplay/stun.h"
#include "netplay/upnp.h"
#include "port/config.h"

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
#define LOBBY_POLL_INTERVAL_MS 2000

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
static int lobby_pending_invite_ping = -1; // ms, -1 = unknown
static SDL_AtomicInt lobby_punch_cancel = { 0 }; // Set to 1 to cancel in-progress hole punch
static bool lobby_we_are_initiator = false; // true = we clicked Connect, false = they invited us
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
    int rtt_ms;
} AsyncPresenceData;

static SDL_AtomicInt async_presence_active = { 0 };

static int SDLCALL async_presence_fn(void* data) {
    AsyncPresenceData* d = (AsyncPresenceData*)data;
    LobbyServer_UpdatePresence(d->player_id, d->display_name, d->region, d->room_code, d->connect_to, d->rtt_ms);
    free(d);
    SDL_SetAtomicInt(&async_presence_active, 0);
    return 0;
}

static int lobby_my_rtt_ms = -1; // Our measured RTT to the lobby server

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

            uint32_t peer_ip = 0;
            uint16_t peer_port = 0;
            if (!Stun_DecodeEndpoint(lobby_server_players[i].room_code, &peer_ip, &peer_port))
                break;

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

// Hole punch thread function — also measures RTT for the invite popup
static int SDLCALL hole_punch_thread_fn(void* data) {
    (void)data;
    SDL_SetAtomicInt(&lobby_punch_cancel, 0);
    uint32_t start_ms = SDL_GetTicks();
    bool ok = Stun_HolePunch(&stun_result, lobby_punch_peer_ip, lobby_punch_peer_port, 10000, &lobby_punch_cancel);
    if (ok) {
        uint32_t rtt_ms = (SDL_GetTicks() - start_ms);
        if (rtt_ms > 200) rtt_ms = 200;  // Cap at reasonable max for display
        if (rtt_ms < 1) rtt_ms = 1;
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

    // Reset async thread guards so lobby can restart cleanly
    SDL_SetAtomicInt(&async_presence_active, 0);
    SDL_SetAtomicInt(&async_action_active, 0);
}

// Simple ImGui spinner (only used by deprecated ImGui lobby — see #if 0 below)
#if 0
static void ImGuiSpinner(const char* label) {
    float t = (float)SDL_GetTicks() / 1000.0f;
    const char* frames[] = { "|", "/", "-", "\\" };
    int idx = ((int)(t * 8.0f)) % 4;
    ImGui::Text("%s %s", frames[idx], label);
}
#endif

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

static void RenderToasts() {
    float dt = ImGui::GetIO().DeltaTime;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;

    float y_offset = 50.0f;

    for (int i = 0; i < MAX_TOASTS; ++i) {
        if (toasts[i].active) {
            if (toasts[i].duration >= 0.0f) {
                toasts[i].timer += dt;
                if (toasts[i].timer >= toasts[i].duration) {
                    toasts[i].active = false;
                    continue;
                }
            }

            ImGui::SetNextWindowPos(
                ImVec2(work_pos.x + work_size.x * 0.5f, work_pos.y + y_offset), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.7f);
            char window_id[32];
            snprintf(window_id, sizeof(window_id), "##Toast%d", i);

            if (ImGui::Begin(window_id,
                             NULL,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs)) {
                ImGui::Text("%s", toasts[i].message);
            }
            y_offset += ImGui::GetWindowHeight() + 5.0f;
            ImGui::End();
        }
    }
}

static void PushHistory(float ping, float rb) {
    ping_history[history_offset] = ping;
    rb_history[history_offset] = rb;
    history_offset = (history_offset + 1) % HISTORY_MAX;
    if (history_offset == 0)
        history_full = true;
}

static void RenderDiagnostics() {
    if (!diagnostics_visible)
        return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Diagnostics [F10]", &diagnostics_visible)) {
        // --- FPS Section (always visible) ---
        if (s_fps_history && s_fps_history_count > 0) {
            // Color-coded FPS
            ImVec4 fps_color;
            if (s_fps_current >= 55.0f)
                fps_color = ImVec4(0.2f, 1.0f, 0.4f, 1.0f);
            else if (s_fps_current >= 45.0f)
                fps_color = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
            else
                fps_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

            ImGui::TextColored(fps_color, "FPS: %.1f", s_fps_current);
            ImGui::SameLine();
            float ft_ms = s_fps_current > 0.0f ? 1000.0f / s_fps_current : 0.0f;
            ImGui::TextDisabled("(%.2f ms)", ft_ms);

            // Downsample for display
            static const int DISPLAY_MAX = 600;
            static float display_buf[600];
            const float* plot_data;
            int plot_count;

            if (s_fps_history_count <= DISPLAY_MAX) {
                plot_data = s_fps_history;
                plot_count = s_fps_history_count;
            } else {
                float bucket_size = (float)s_fps_history_count / DISPLAY_MAX;
                for (int i = 0; i < DISPLAY_MAX; i++) {
                    int start = (int)(i * bucket_size);
                    int end = (int)((i + 1) * bucket_size);
                    if (end > s_fps_history_count)
                        end = s_fps_history_count;
                    float sum = 0.0f;
                    for (int j = start; j < end; j++)
                        sum += s_fps_history[j];
                    display_buf[i] = sum / (end - start);
                }
                plot_data = display_buf;
                plot_count = DISPLAY_MAX;
            }

            // Compute stats for overlay and Y-axis
            float avg = 0.0f;
            float min_fps = plot_data[0], max_fps = plot_data[0];
            for (int i = 0; i < plot_count; i++) {
                avg += plot_data[i];
                if (plot_data[i] < min_fps)
                    min_fps = plot_data[i];
                if (plot_data[i] > max_fps)
                    max_fps = plot_data[i];
            }
            avg /= plot_count;

            // Dynamic Y-axis: pad around actual range, minimum 5 FPS span
            float range = max_fps - min_fps;
            if (range < 5.0f)
                range = 5.0f;
            float y_min = min_fps - range * 0.15f;
            float y_max = max_fps + range * 0.15f;
            if (y_min < 0.0f)
                y_min = 0.0f;

            int secs = s_fps_history_count / 60;
            char overlay[64];
            snprintf(overlay,
                     sizeof(overlay),
                     "avg: %.1f  |  %d:%02d  |  %d frames",
                     avg,
                     secs / 60,
                     secs % 60,
                     s_fps_history_count);

            // Chart fills available card width, fixed height
            float avail_w = ImGui::GetContentRegionAvail().x;
            ImGui::PlotLines("##fps_chart", plot_data, plot_count, 0, overlay, y_min, y_max, ImVec2(avail_w, 120));
        } else {
            ImGui::TextDisabled("FPS: waiting for data...");
        }

        // --- Netplay Section (only during active sessions) ---
        if (Netplay_GetSessionState() == NETPLAY_SESSION_RUNNING) {
            ImGui::Separator();

            NetworkStats metrics;
            Netplay_GetNetworkStats(&metrics);

            ImGui::Text("Current Ping: %d ms", metrics.ping);
            ImGui::Text("Current Rollback: %d frames", metrics.rollback);
            ImGui::Text("Delay: %d frames", metrics.delay);

            uint64_t duration = (SDL_GetTicks() - session_start_ticks) / 1000;
            int mins = (int)(duration / 60);
            int secs = (int)(duration % 60);
            ImGui::Text("Session Duration: %02d:%02d", mins, secs);

            ImGui::Separator();

            float max_ping = 0;
            for (int i = 0; i < HISTORY_MAX; ++i)
                if (ping_history[i] > max_ping)
                    max_ping = ping_history[i];

            ImGui::PlotLines(
                "Ping History", ping_history, HISTORY_MAX, history_offset, NULL, 0.0f, max_ping + 10.0f, ImVec2(0, 80));
            ImGui::PlotLines(
                "Rollback History", rb_history, HISTORY_MAX, history_offset, NULL, 0.0f, 10.0f, ImVec2(0, 80));
        }
    }
    ImGui::End();
}

extern "C" {

void SDLNetplayUI_Render(int window_width, int window_height) {
    ProcessEvents();

    NetworkStats stats;
    Netplay_GetNetworkStats(&stats);
    PushHistory((float)stats.ping, (float)stats.rollback);

    if (hud_visible && Netplay_GetSessionState() == NETPLAY_SESSION_RUNNING) {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav;

        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = work_pos.x + work_size.x - PAD;
        window_pos.y = work_pos.y + PAD;
        window_pos_pivot.x = 1.0f;
        window_pos_pivot.y = 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (ImGui::Begin("Netplay Mini-HUD", NULL, window_flags)) {
            char buffer[128];
            SDLNetplayUI_GetHUDText(buffer, sizeof(buffer));

            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (stats.ping > 150)
                color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
            else if (stats.ping > 80)
                color = ImVec4(1.0f, 0.9f, 0.2f, 1.0f); // Yellow

            if (stats.rollback > 3)
                color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red

            ImGui::TextColored(color, "%s", buffer);
        }
        ImGui::End();
    }

    if (Netplay_GetSessionState() == NETPLAY_SESSION_LOBBY) {
        int state = SDL_GetAtomicInt(&lobby_async_state);

        // Auto-start STUN discovery on lobby entry
        if (state == LOBBY_ASYNC_IDLE) {
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
                // Use persistent client ID as the hidden auth token to prevent spoofing.
                const char* client_id = Config_GetString(CFG_KEY_LOBBY_CLIENT_ID);
                if (!client_id || !client_id[0])
                    client_id = my_room_code; // Fallback

                // Use custom display name if set, otherwise fall back to room code
                const char* display = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                if (!display || !display[0])
                    display = my_room_code;

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

        // [DEPRECATED] ImGui lobby window — discontinued in favor of native lobby.
        // The underlying state machine (STUN, hole-punch, UPnP, server polling)
        // continues to run above; only the ImGui rendering is disabled.
#if 0
        if (!native_lobby_active) {

            // --- Fullscreen lobby window (matches F1/F2/F3 style) ---
            float font_scale = (float)window_height / 480.0f;
            ImGui::GetIO().FontGlobalScale = font_scale;

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)window_width, (float)window_height));
            ImGui::Begin("Network Lobby",
                         NULL,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse);

            // Centered title
            {
                const char* title = "NETWORK LOBBY";
                ImVec2 tsz = ImGui::CalcTextSize(title);
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - tsz.x) * 0.5f);
                ImGui::TextUnformatted(title);
            }
            ImGui::Separator();
            ImGui::Spacing();

            // Status message
            if (lobby_status_msg[0]) {
                ImGui::TextWrapped("%s", lobby_status_msg);
                ImGui::Spacing();
            }

            // ===== TWO-COLUMN LAYOUT =====
            float footer_h = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
            float table_h = ImGui::GetContentRegionAvail().y - footer_h;
            if (table_h < 100.0f)
                table_h = 100.0f;

            ImGuiTableFlags table_flags =
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX;

            if (ImGui::BeginTable("LobbyColumns", 2, table_flags, ImVec2(0, table_h))) {
                ImGui::TableSetupColumn("Internet", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableSetupColumn("LAN", ImGuiTableColumnFlags_WidthStretch, 0.5f);

                ImGui::TableNextRow();

                // ===== LEFT COLUMN: Internet Play =====
                ImGui::TableNextColumn();
                {
                    const char* hdr = "Internet Play";
                    ImVec2 hsz = ImGui::CalcTextSize(hdr);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - hsz.x) * 0.5f);
                    ImGui::TextUnformatted(hdr);
                }
                ImGui::Separator();
                ImGui::Spacing();

                if (state == LOBBY_ASYNC_DISCOVERING) {
                    ImGuiSpinner("Contacting STUN server...");

                } else if (state == LOBBY_ASYNC_PUNCHING) {
                    ImGuiSpinner("Hole punching...");
                    ImGui::TextDisabled("Both players must press Connect\nat roughly the same time.");

                } else if (state == LOBBY_ASYNC_UPNP_TRYING) {
                    ImGuiSpinner("Trying UPnP port forward...");
                    ImGui::TextDisabled("Requesting router to open port.");

                } else if (state == LOBBY_ASYNC_READY) {

                    // --- Server Browser ---
                    if (LobbyServer_IsConfigured() && lobby_server_registered) {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        bool lobby_auto = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                        if (ImGui::Checkbox("Auto-Connect", &lobby_auto)) {
                            Config_SetBool(CFG_KEY_LOBBY_AUTO_CONNECT, lobby_auto);
                            Config_Save();
                        }
                        ImGui::SameLine();
                        bool lobby_auto_search = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                        if (ImGui::Checkbox("Auto-Search", &lobby_auto_search)) {
                            Config_SetBool(CFG_KEY_LOBBY_AUTO_SEARCH, lobby_auto_search);
                            Config_Save();
                        }
                        ImGui::Spacing();

                        if (!lobby_server_searching) {
                            if (ImGui::Button("Search for Match")) {
                                AsyncLobbyAction(lobby_my_player_id, 1);
                                lobby_server_searching = true;
                                lobby_server_last_poll = 0; // Force immediate poll
                            }
                        } else {
                            if (ImGui::Button("Stop Searching")) {
                                AsyncLobbyAction(lobby_my_player_id, 2);
                                lobby_server_searching = false;
                                SDL_SetAtomicInt(&lobby_server_player_count, 0);
                            }

                            ImGui::Spacing();
                            ImGui::Text("Players Online:");

                            // Filter out ourselves
                            int shown = 0;
                            int pc = SDL_GetAtomicInt(&lobby_server_player_count);
                            for (int i = 0; i < pc; i++) {
                                if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
                                    continue;

                                ImGui::PushID(100 + i);
                                ImGui::Text("%s", lobby_server_players[i].display_name);
                                if (lobby_server_players[i].region[0]) {
                                    ImGui::SameLine();
                                    ImGui::TextDisabled("[%s]", lobby_server_players[i].region);
                                }
                                // Show indicator if this player is connecting to us
                                if (lobby_server_players[i].connect_to[0] &&
                                    strcmp(lobby_server_players[i].connect_to, my_room_code) == 0) {
                                    ImGui::SameLine();
                                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Connecting to you!");
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Connect")) {
                                    uint32_t peer_ip = 0;
                                    uint16_t peer_port = 0;
                                    if (Stun_DecodeEndpoint(lobby_server_players[i].room_code, &peer_ip, &peer_port)) {
                                        const char* display_ct = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
                                        if (!display_ct || !display_ct[0])
                                            display_ct = my_room_code;
                                        AsyncUpdatePresence(lobby_my_player_id,
                                                            display_ct,
                                                            my_room_code,
                                                            lobby_server_players[i].room_code);
                                        snprintf(lobby_connect_to_intent, sizeof(lobby_connect_to_intent), "%s", lobby_server_players[i].room_code);
                                        snprintf(lobby_punch_peer_name,
                                                 sizeof(lobby_punch_peer_name),
                                                 "%s",
                                                 lobby_server_players[i].display_name);
                                        lobby_we_are_initiator = true;
                                        lobby_start_punch(peer_ip, peer_port);
                                    } else {
                                        snprintf(lobby_status_msg,
                                                 sizeof(lobby_status_msg),
                                                 "Invalid code from %s",
                                                 lobby_server_players[i].display_name);
                                    }
                                }
                                ImGui::PopID();
                                shown++;
                            }
                            if (shown == 0) {
                                ImGui::TextDisabled("No other players searching.");
                            }
                        }
                    }

                } else if (state == LOBBY_ASYNC_STUN_FAILED) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "STUN failed.");
                    if (ImGui::SmallButton("Retry")) {
                        lobby_start_discover();
                    }
                }

                // ===== RIGHT COLUMN: LAN =====
                ImGui::TableNextColumn();
                {
                    const char* hdr = "Local Network (LAN)";
                    ImVec2 hsz = ImGui::CalcTextSize(hdr);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - hsz.x) * 0.5f);
                    ImGui::TextUnformatted(hdr);
                }
                ImGui::Separator();
                ImGui::Spacing();

                bool auto_connect = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                if (ImGui::Checkbox("Auto-Connect", &auto_connect)) {
                    Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, auto_connect);
                    Config_Save();
                }

                ImGui::Spacing();

                NetplayDiscoveredPeer peers[16];
                int num_peers = Discovery_GetPeers(peers, 16);

                if (num_peers == 0) {
                    ImGui::TextDisabled("No players found.");
                } else {
                    for (int i = 0; i < num_peers; i++) {
                        ImGui::PushID(i);
                        ImGui::Text("%s (%s)", peers[i].name, peers[i].ip);
                        if (peers[i].wants_auto_connect) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Waiting...");
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Connect")) {
                            Netplay_SetRemoteIP(peers[i].ip);
                            Netplay_SetRemotePort(peers[i].port);
                            Netplay_SetPlayerNumber(0); // We clicked Connect = P1
                            extern unsigned short g_netplay_port;
                            Netplay_SetLocalPort(g_netplay_port);
                            lobby_reset();
                            Netplay_Begin();
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::EndTable();
            }

            // Footer
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            {
                const char* footer = "ESC Cancel";
                ImVec2 fsz = ImGui::CalcTextSize(footer);
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - fsz.x) * 0.5f);
                ImGui::TextDisabled("%s", footer);
            }

            // Handle ESC key
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                lobby_reset();
                Netplay_HandleMenuExit();
            }

            ImGui::End();
            ImGui::GetIO().FontGlobalScale = 1.0f;
        } /* end if (!native_lobby_active) */
#endif
    } else {
        // Reset lobby state when not in lobby
        if (SDL_GetAtomicInt(&lobby_async_state) != LOBBY_ASYNC_IDLE) {
            lobby_reset();
        }
    }

    RenderToasts();
    RenderDiagnostics();
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
    // Count only searching players, excluding ourselves
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
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
        if (count == index)
            return lobby_server_players[i].room_code;
        count++;
    }
    return "";
}

void SDLNetplayUI_ConnectToPlayer(int index) {
    int count = 0;
    int pc = SDL_GetAtomicInt(&lobby_server_player_count);
    for (int i = 0; i < pc; i++) {
        if (strcmp(lobby_server_players[i].player_id, lobby_my_player_id) == 0)
            continue;
        if (strcmp(lobby_server_players[i].status, "searching") != 0)
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
                snprintf(lobby_connect_to_intent, sizeof(lobby_connect_to_intent), "%s", lobby_server_players[i].room_code);
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
    lobby_has_pending_invite = false;
    lobby_pending_invite_name[0] = '\0';
    lobby_pending_invite_region[0] = '\0';
    lobby_pending_invite_ping = -1;
    snprintf(lobby_status_msg, sizeof(lobby_status_msg), "Declined invite.");
}

} // extern "C"
