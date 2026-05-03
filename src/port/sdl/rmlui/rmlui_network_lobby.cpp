/**
 * @file rmlui_network_lobby.cpp
 * @brief RmlUi Network Lobby data model.
 *
 * Replaces CPS3's effect_61/57/66 objects and SSPutStr_Bigger/
 * Renderer_Queue2DPrimitive rendering in Network_Lobby() with
 * an RmlUi overlay showing lobby items, peer lists, and popup modals.
 *
 * Key APIs:
 *   g_state.Menu_Cursor_Y[0] — cursor position (9 items: 0-8)
 *   Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT) — LAN auto-connect
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT) — NET auto-accept
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH) — NET auto-search
 *   Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK) — region lock
 *   Config_GetInt(CFG_KEY_NETPLAY_MAX_PING) — max ping filter
 *   Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI) — block WiFi
 *   Discovery_GetPeers() — LAN peer list
 *   SDLNetplayUI_* — Internet search, peer list, invite/challenge state
 *   LobbyServer_IsConfigured() — whether internet lobby is set up
 */

#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "game_state.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Input.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/discovery.h"
#include "netplay/ping_probe.h"
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "port/config/config.h"
#include "port/config/paths.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/sdl/qr_texture.h"
#include "sf33rd/Source/Game/engine/state_user.h"

extern int g_lobby_peer_idx;
extern int g_net_peer_idx;
} // extern "C"

// ─── Peer list item structs for data-for ─────────────────────────

struct LanPeerItem {
    Rml::String name;
    bool selected; // true when g_lobby_peer_idx == this row

    bool operator==(const LanPeerItem& o) const {
        return name == o.name && selected == o.selected;
    }
    bool operator!=(const LanPeerItem& o) const {
        return !(*this == o);
    }
};

struct NetPeerItem {
    Rml::String name;
    Rml::String country;    // ISO 3166-1 alpha-2 (e.g. "US", "JP")
    Rml::String flag_icon;  // path to flag PNG (e.g. "assets/flags_icons/us.png")
    Rml::String ping_label; // e.g. "~42ms" or "..."
    Rml::String ping_class; // "ping-good" | "ping-ok" | "ping-bad"
    Rml::String conn_type;  // "wifi" | "wired" | "unknown"
    bool selected;          // true when g_net_peer_idx == this row

    bool operator==(const NetPeerItem& o) const {
        return name == o.name && country == o.country && flag_icon == o.flag_icon && ping_label == o.ping_label &&
               ping_class == o.ping_class && conn_type == o.conn_type && selected == o.selected;
    }
    bool operator!=(const NetPeerItem& o) const {
        return !(*this == o);
    }
};

struct RoomItem {
    Rml::String code;
    Rml::String name;
    int player_count;
    int max_players;
    bool selected;
    bool is_tournament;
    bool is_private;

    bool operator==(const RoomItem& o) const {
        return code == o.code && name == o.name && player_count == o.player_count && max_players == o.max_players &&
               selected == o.selected && is_tournament == o.is_tournament && is_private == o.is_private;
    }
    bool operator!=(const RoomItem& o) const {
        return !(*this == o);
    }
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Live peer lists — rebuilt each update frame
static std::vector<LanPeerItem> s_lan_peers;
static std::vector<NetPeerItem> s_net_peers;
static std::vector<RoomItem> s_room_list;
static int s_room_list_idx = 0;
static Uint64 s_room_list_poll_time = 0;

// Async room list fetch
static SDL_AtomicInt s_room_fetch_active = { 0 }; // 1 = fetch thread running
static SDL_AtomicInt s_room_fetch_done = { 0 };   // 1 = result ready
static RoomListItem s_room_fetch_buf[16];         // shared result buffer
static int s_room_fetch_count = 0;                // shared result count

static int SDLCALL async_room_list_fn(void* data) {
    (void)data;
    RoomListItem items[16];
    int rc = LobbyServer_ListRooms(items, 16);
    memcpy(s_room_fetch_buf, items, sizeof(items));
    s_room_fetch_count = rc;

    // Release barrier guarantees payload writes are visible before 'done' goes to 1
    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&s_room_fetch_done, 1);
    SDL_SetAtomicInt(&s_room_fetch_active, 0);
    return 0;
}

static void AsyncFetchRoomList(void) {
    if (SDL_GetAtomicInt(&s_room_fetch_active) != 0)
        return;
    SDL_SetAtomicInt(&s_room_fetch_active, 1);
    SDL_SetAtomicInt(&s_room_fetch_done, 0);
    SDL_Thread* t = SDL_CreateThread(async_room_list_fn, "AsyncRoomList", nullptr);
    if (t) {
        SDL_DetachThread(t);
    } else {
        SDL_SetAtomicInt(&s_room_fetch_active, 0);
    }
}

// ─── Room create/join state ──────────────────────────────────────
static Rml::String s_room_status;                 // "" | "Creating..." | "Joining..." | "ABCD" (room code)
static Rml::String s_join_room_code;              // entered room code for display
static SDL_AtomicInt s_room_async_active = { 0 }; // 1 = background op in progress

// Result from background thread
static SDL_AtomicInt s_room_async_done = { 0 }; // 1 = result ready
static SDL_AtomicInt s_room_async_ok = { 0 };   // 1 = success, 0 = fail
static char s_room_async_code[16] = { 0 };      // resulting room code on success
static char s_room_async_error[128] = { 0 };    // error message on failure

// Room-ready signal: set by async completion, consumed by ms_network_lobby.c
static bool s_room_ready = false;
static int s_room_async_type = 0; // RoomType of the pending room

// Create room settings (toggled by user before pressing CREATE)
static int s_create_room_type = ROOM_TYPE_CASUAL; // 0=casual, 2=tournament
static int s_create_tournament_format = TOURNAMENT_SINGLE_ELIM;
static Rml::String s_create_room_type_label = "CASUAL";
static Rml::String s_create_tournament_format_label = "SINGLE ELIM";
static int s_create_room_visibility = ROOM_VISIBILITY_PUBLIC;
static Rml::String s_create_room_visibility_label = "PUBLIC";
static char s_create_room_password[64] = { 0 };

static char s_active_room_password[64] = { 0 };

// Password prompt popup state (for joining private rooms OR setting create-room password)
static bool s_password_popup_visible = false;
static int s_password_popup_mode =
    0; // 0 = join (submits to AsyncJoinRoom), 1 = create (stores in s_create_room_password)
static char s_password_input[64] = { 0 };
static int s_password_input_len = 0;
static Rml::String s_password_input_display;
static char s_password_target_room[16] = { 0 };

// Arcade-style character cycling state
static int s_password_char_idx = 0; // index into charset for current cycling
static const char PASSWORD_CHARSET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const int PASSWORD_CHARSET_LEN = 36; // A-Z (26) + 0-9 (10)

// Create room password display label
static Rml::String s_create_password_label = "NONE";

static const char* room_type_label(int t) {
    switch (t) {
    case ROOM_TYPE_TOURNAMENT:
        return "TOURNAMENT";
    default:
        return "CASUAL";
    }
}
static const char* tournament_format_label(int f) {
    switch (f) {
    case TOURNAMENT_SINGLE_ELIM:
        return "SINGLE ELIM";
    case TOURNAMENT_DOUBLE_ELIM:
        return "DOUBLE ELIM";
    case TOURNAMENT_ROUND_ROBIN:
        return "ROUND ROBIN";
    case TOURNAMENT_SWISS:
        return "SWISS";
    default:
        return "SINGLE ELIM";
    }
}

// QR code BMP path — generated on room create, displayed in casual lobby
static char s_qr_image_path[512] = { 0 };

struct AsyncRoomData {
    int action;         // 1 = create, 2 = join, 3 = create_tournament
    char name[64];      // room name (create)
    char code[16];      // room code (join)
    int ft;             // FT mode for the room (create only)
    int room_type;      // RoomType enum (for create)
    int tournament_fmt; // TournamentFormat enum (for tournament create)
    char password[64];  // room password (create + join)
    int visibility;     // RoomVisibility enum (for create)
};

static int SDLCALL async_room_fn(void* data) {
    AsyncRoomData* d = (AsyncRoomData*)data;
    RoomState room;
    memset(&room, 0, sizeof(room));
    bool ok = false;

    if (d->action == 1) {
        ok = LobbyServer_CreateRoom(d->name, d->ft, d->password[0] ? d->password : NULL, d->visibility, &room);
    } else if (d->action == 2) {
        char err[128] = { 0 };
        ok = LobbyServer_JoinRoom(d->code, d->password[0] ? d->password : NULL, &room, err, sizeof(err));
        if (!ok && err[0]) {
            snprintf(s_room_async_error, sizeof(s_room_async_error), "%s", err);
        }
    } else if (d->action == 3) {
        ok = LobbyServer_CreateTournamentRoom(d->name,
                                              (TournamentFormat)d->tournament_fmt,
                                              16,
                                              d->ft,
                                              "rating",
                                              d->password[0] ? d->password : NULL,
                                              d->visibility,
                                              &room);
    }

    if (ok) {
        snprintf(s_room_async_code, sizeof(s_room_async_code), "%s", room.id);
        s_room_async_type = room.room_type;
        snprintf(s_active_room_password, sizeof(s_active_room_password), "%s", d->password);

        // Generate QR code BMP for private/password rooms (action 1 = create, 3 = tournament)
        // Must happen BEFORE s_room_async_ok is set — main thread reads qr_image_path
        // after observing async_done, so the path must be fully written first.
        s_qr_image_path[0] = '\0';
        if ((d->action == 1 || d->action == 3) && (d->password[0] || d->visibility == 1)) {
            const char* server_url = LobbyServer_GetBaseURL();
            if (server_url[0]) {
                char qr_url[512];
                snprintf(qr_url, sizeof(qr_url), "%s/qr-join?room=%s&pw=%s", server_url, room.id, d->password);
                const char* pref = Paths_GetPrefPath();
                snprintf(s_qr_image_path, sizeof(s_qr_image_path), "%sqr_join.bmp", pref ? pref : "");
                QRTexture_GenerateBMP(qr_url, s_qr_image_path, 4);
            }
        }

        SDL_SetAtomicInt(&s_room_async_ok, 1);
    } else {
        s_room_async_code[0] = '\0';
        SDL_SetAtomicInt(&s_room_async_ok, 0);
    }

    free(d);

    // Release barrier guarantees string writes are visible before 'done' goes to 1
    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&s_room_async_done, 1);
    SDL_SetAtomicInt(&s_room_async_active, 0);
    return 0;
}

static void AsyncCreateRoom(const char* name, const char* password, int visibility) {
    if (SDL_GetAtomicInt(&s_room_async_active) != 0)
        return;
    SDL_SetAtomicInt(&s_room_async_active, 1);
    SDL_SetAtomicInt(&s_room_async_done, 0);
    AsyncRoomData* d = (AsyncRoomData*)calloc(1, sizeof(AsyncRoomData));
    d->action = 1;
    d->ft = Config_GetInt(CFG_KEY_NETPLAY_FT);
    d->visibility = visibility;
    snprintf(d->name, sizeof(d->name), "%s", name ? name : "Casual Room");
    if (password && password[0])
        snprintf(d->password, sizeof(d->password), "%s", password);
    SDL_Thread* t = SDL_CreateThread(async_room_fn, "AsyncCreateRoom", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_room_async_active, 0);
    }
}

static void AsyncJoinRoom(const char* code, const char* password) {
    if (SDL_GetAtomicInt(&s_room_async_active) != 0)
        return;
    SDL_SetAtomicInt(&s_room_async_active, 1);
    SDL_SetAtomicInt(&s_room_async_done, 0);
    AsyncRoomData* d = (AsyncRoomData*)calloc(1, sizeof(AsyncRoomData));
    d->action = 2;
    snprintf(d->code, sizeof(d->code), "%s", code);
    if (password && password[0])
        snprintf(d->password, sizeof(d->password), "%s", password);
    SDL_Thread* t = SDL_CreateThread(async_room_fn, "AsyncJoinRoom", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_room_async_active, 0);
    }
}

// (Popup code removed — room joining is now list-based)

struct LobbyCache {
    int cursor;
    bool lan_auto;
    bool net_auto;
    bool net_search_toggle;
    bool net_searching;
    bool net_is_configured;
    bool region_lock;
    int max_ping;
    bool block_wifi;
    int ft_value;
    int lan_peer_count;
    int net_peer_count;
    int lan_peer_idx;
    int net_peer_idx;
    int popup_type; // 0=none, 1=incoming, 2=outgoing
    Rml::String room_code;
    Rml::String room_status;
    Rml::String join_room_code;
    int room_count;
    int room_list_idx;
};
static LobbyCache s_cache = {};

#define DIRTY_INT(nm, expr)                                                                                            \
    do {                                                                                                               \
        int _v = (expr);                                                                                               \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

#define DIRTY_BOOL(nm, expr)                                                                                           \
    do {                                                                                                               \
        bool _v = (expr);                                                                                              \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

#define DIRTY_STR(nm, expr)                                                                                            \
    do {                                                                                                               \
        Rml::String _v = (expr);                                                                                       \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("network_lobby");
    if (!ctor)
        return;

    // ── Register LanPeerItem struct ──────────────────────────────
    if (auto h = ctor.RegisterStruct<LanPeerItem>()) {
        h.RegisterMember("name", &LanPeerItem::name);
        h.RegisterMember("selected", &LanPeerItem::selected);
    }
    ctor.RegisterArray<std::vector<LanPeerItem>>();

    // ── Register NetPeerItem struct ──────────────────────────────
    if (auto h = ctor.RegisterStruct<NetPeerItem>()) {
        h.RegisterMember("name", &NetPeerItem::name);
        h.RegisterMember("country", &NetPeerItem::country);
        h.RegisterMember("flag_icon", &NetPeerItem::flag_icon);
        h.RegisterMember("ping_label", &NetPeerItem::ping_label);
        h.RegisterMember("ping_class", &NetPeerItem::ping_class);
        h.RegisterMember("conn_type", &NetPeerItem::conn_type);
        h.RegisterMember("selected", &NetPeerItem::selected);
    }
    ctor.RegisterArray<std::vector<NetPeerItem>>();

    // ── Register RoomItem struct ─────────────────────────────────
    if (auto h = ctor.RegisterStruct<RoomItem>()) {
        h.RegisterMember("code", &RoomItem::code);
        h.RegisterMember("name", &RoomItem::name);
        h.RegisterMember("player_count", &RoomItem::player_count);
        h.RegisterMember("max_players", &RoomItem::max_players);
        h.RegisterMember("selected", &RoomItem::selected);
        h.RegisterMember("is_tournament", &RoomItem::is_tournament);
        h.RegisterMember("is_private", &RoomItem::is_private);
    }
    ctor.RegisterArray<std::vector<RoomItem>>();

    // ── Bind peer + room lists ───────────────────────────────────
    ctor.Bind("lan_peers", &s_lan_peers);
    ctor.Bind("net_peers", &s_net_peers);
    ctor.Bind("room_list", &s_room_list);

    // ── Scalar bindings ──────────────────────────────────────────
    ctor.BindFunc("cursor", [](Rml::Variant& v) { v = (int)g_state.Menu_Cursor_Y[0]; });

    ctor.BindFunc("lan_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT); });
    ctor.BindFunc("net_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT); });
    ctor.BindFunc("net_search_toggle", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH); });
    ctor.BindFunc("net_searching", [](Rml::Variant& v) { v = SDLNetplayUI_IsSearching(); });
    ctor.BindFunc("net_is_configured", [](Rml::Variant& v) { v = LobbyServer_IsConfigured(); });

    // New filter toggles
    ctor.BindFunc("region_lock", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK); });
    ctor.BindFunc("max_ping", [](Rml::Variant& v) { v = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING); });
    ctor.BindFunc("block_wifi", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI); });
    ctor.BindFunc("ft_value", [](Rml::Variant& v) { v = Config_GetInt(CFG_KEY_NETPLAY_FT); });

    // Room code (shown under title bar)
    ctor.BindFunc("room_code", [](Rml::Variant& v) {
        const char* rc = SDLNetplayUI_GetRoomCode();
        v = Rml::String(rc ? rc : "");
    });

    // Room create/join status
    ctor.Bind("room_status", &s_room_status);
    ctor.Bind("join_room_code", &s_join_room_code);

    // Create room settings
    ctor.Bind("create_room_type", &s_create_room_type);
    ctor.Bind("create_room_type_label", &s_create_room_type_label);
    ctor.Bind("create_tournament_format", &s_create_tournament_format);
    ctor.Bind("create_tournament_format_label", &s_create_tournament_format_label);

    // Visibility + password popup
    ctor.Bind("create_room_visibility", &s_create_room_visibility);
    ctor.Bind("create_room_visibility_label", &s_create_room_visibility_label);
    ctor.Bind("create_password_label", &s_create_password_label);
    ctor.Bind("password_popup_visible", &s_password_popup_visible);
    ctor.Bind("password_popup_mode", &s_password_popup_mode);
    ctor.Bind("password_input_display", &s_password_input_display);

    // Room list scalars
    ctor.BindFunc("room_count", [](Rml::Variant& v) { v = (int)s_room_list.size(); });
    ctor.BindFunc("room_list_idx", [](Rml::Variant& v) { v = s_room_list_idx; });

    // LAN peer count / currently selected name (kept for legacy status bar)
    ctor.BindFunc("lan_peer_count", [](Rml::Variant& v) {
        NetplayDiscoveredPeer peers[16];
        v = Discovery_GetPeers(peers, 16);
    });
    ctor.BindFunc("lan_peer_name", [](Rml::Variant& v) {
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        if (count > 0) {
            int idx = g_lobby_peer_idx;
            if (idx < 0)
                idx = 0;
            if (idx >= count)
                idx = count - 1;
            v = Rml::String(peers[idx].name);
        } else {
            v = Rml::String("NONE");
        }
    });
    ctor.BindFunc("lan_peer_idx", [](Rml::Variant& v) { v = g_lobby_peer_idx; });

    // NET peer count / currently selected name (kept for legacy status bar)
    ctor.BindFunc("net_peer_count", [](Rml::Variant& v) { v = SDLNetplayUI_GetOnlinePlayerCount(); });
    ctor.BindFunc("net_peer_name", [](Rml::Variant& v) {
        int count = SDLNetplayUI_GetOnlinePlayerCount();
        if (count > 0) {
            int idx = g_net_peer_idx;
            if (idx < 0)
                idx = 0;
            if (idx >= count)
                idx = count - 1;
            v = Rml::String(SDLNetplayUI_GetOnlinePlayerName(idx));
        } else if (SDLNetplayUI_IsSearching()) {
            v = Rml::String("SEARCHING");
        } else {
            v = Rml::String("IDLE");
        }
    });
    ctor.BindFunc("net_peer_idx", [](Rml::Variant& v) { v = g_net_peer_idx; });

    // Status text
    ctor.BindFunc("status_text", [](Rml::Variant& v) {
        const char* msg = SDLNetplayUI_GetStatusMsg();
        if (msg[0]) {
            v = Rml::String(msg);
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        uint32_t target = Discovery_GetChallengeTarget();

        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me) {
                char buf[64];
                SDL_snprintf(buf, sizeof(buf), "CHALLENGED BY %s!", peers[i].name);
                v = Rml::String(buf);
                return;
            }
        }
        if (target != 0) {
            for (int i = 0; i < count; i++) {
                if (peers[i].instance_id == target) {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "CHALLENGING %s...", peers[i].name);
                    v = Rml::String(buf);
                    return;
                }
            }
        }
        if (SDLNetplayUI_IsDiscovering()) {
            v = Rml::String("DISCOVERING...");
            return;
        }
        v = Rml::String("");
    });

    // Popup state
    ctor.BindFunc("popup_type", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = 1;
            return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = 2;
            return;
        }
        if (Discovery_GetChallengeTarget() != 0) {
            v = 3;
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me) {
                v = 4;
                return;
            }
        }
        v = 0;
    });
    ctor.BindFunc("popup_title", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = Rml::String("INCOMING CHALLENGE!");
            return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge() || Discovery_GetChallengeTarget() != 0) {
            v = Rml::String("CONNECTING...");
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me) {
                v = Rml::String("INCOMING CHALLENGE!");
                return;
            }
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_name", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = Rml::String(SDLNetplayUI_GetPendingInviteName());
            return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = Rml::String(SDLNetplayUI_GetOutgoingChallengeName());
            return;
        }
        uint32_t target = Discovery_GetChallengeTarget();
        if (target != 0) {
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            for (int i = 0; i < count; i++) {
                if (peers[i].instance_id == target) {
                    v = Rml::String(peers[i].display_name[0] ? peers[i].display_name : peers[i].name);
                    return;
                }
            }
            v = Rml::String("...");
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me) {
                v = Rml::String(peers[i].display_name[0] ? peers[i].display_name : peers[i].name);
                return;
            }
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_ping", [](Rml::Variant& v) {
        auto format_ping = [](int ping) -> Rml::String {
            char buf[32];
            if (ping < 0)
                SDL_snprintf(buf, sizeof(buf), "...");
            else
                SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
            return Rml::String(buf);
        };
        if (SDLNetplayUI_HasPendingInvite()) {
            v = format_ping(SDLNetplayUI_GetPendingInvitePing());
            return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = format_ping(SDLNetplayUI_GetOutgoingChallengePing());
            return;
        }
        // LAN outgoing challenge
        uint32_t target = Discovery_GetChallengeTarget();
        if (target != 0) {
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            for (int i = 0; i < count; i++) {
                if (peers[i].instance_id == target && peers[i].player_id[0]) {
                    v = format_ping(PingProbe_GetRTT(peers[i].player_id));
                    return;
                }
            }
        }
        // LAN incoming challenge
        {
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            for (int i = 0; i < count; i++) {
                if (peers[i].is_challenging_me && peers[i].player_id[0]) {
                    v = format_ping(PingProbe_GetRTT(peers[i].player_id));
                    return;
                }
            }
        }
        v = Rml::String("...");
    });
    ctor.BindFunc("popup_region", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            const char* r = SDLNetplayUI_GetPendingInviteRegion();
            v = Rml::String(r ? r : "");
            return;
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_is_incoming", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = true;
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        bool outgoing = (Discovery_GetChallengeTarget() != 0);
        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me && !outgoing) {
                v = true;
                return;
            }
        }
        v = false;
    });
    ctor.BindFunc("popup_ft", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            int ft = SDLNetplayUI_GetPendingInviteFT();
            if (ft == 1)
                v = Rml::String("UNRANKED");
            else {
                char buf[16];
                snprintf(buf, sizeof(buf), "FT%d", ft);
                v = Rml::String(buf);
            }
            return;
        }
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        for (int i = 0; i < count; i++) {
            if (peers[i].is_challenging_me) {
                int ft = peers[i].ft_value;
                if (ft <= 0)
                    ft = 2;
                if (ft == 1)
                    v = Rml::String("UNRANKED");
                else {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "FT%d", ft);
                    v = Rml::String(buf);
                }
                return;
            }
        }
        v = Rml::String("");
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    // Start global SSE for QR join relay (receives REMOTE_JOIN events)
    if (LobbyServer_IsConfigured() && !LobbyServer_GlobalSSEIsConnected()) {
        LobbyServer_GlobalSSEConnect();
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi NetworkLobby] Data model registered (lazy)");
}

extern "C" void rmlui_network_lobby_init(void) {
    do_init();
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_network_lobby_update(void) {
    if (!s_model_registered) {
        do_init();
        if (!s_model_registered)
            return;
    }
    if (!s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("network_lobby"))
        return;

    // ── Deferred Global SSE connect ──────────────────────────────
    // do_init() runs before Identity_Init(), so the first connect attempt
    // may fail. Retry here each frame until connected.
    if (LobbyServer_IsConfigured() && !LobbyServer_GlobalSSEIsConnected()) {
        LobbyServer_GlobalSSEConnect();
    }

    // ── Poll global SSE for QR join events ────────────────────────
    // Only runs when network lobby is visible (not in a room).
    {
        SSEEvent gsse_evt;
        SSEEventType gsse_type = LobbyServer_GlobalSSEPoll(&gsse_evt);
        if (gsse_type == SSE_EVENT_REMOTE_JOIN && gsse_evt.remote_join_room[0]) {
            SDL_Log("[NetworkLobby] QR Join: joining room %s", gsse_evt.remote_join_room);
            AsyncJoinRoom(gsse_evt.remote_join_room,
                          gsse_evt.remote_join_password[0] ? gsse_evt.remote_join_password : NULL);
        }
    }

    // ── Check async room create/join completion ───────────────────
    if (SDL_GetAtomicInt(&s_room_async_done)) {
        // Acquire barrier pairs with Release in the worker, ensuring safety
        SDL_MemoryBarrierAcquire();
        SDL_SetAtomicInt(&s_room_async_done, 0);
        if (SDL_GetAtomicInt(&s_room_async_ok)) {
            // Success — signal ms_network_lobby.c to transition to casual lobby
            s_room_status = Rml::String(s_room_async_code);
            s_model_handle.DirtyVariable("room_status");

            s_room_ready = true;

            SDL_Log("[NetworkLobby] Room entered: %s", s_room_async_code);
        } else {
            if (strcmp(s_room_async_error, "Wrong password") == 0 ||
                strcmp(s_room_async_error, "Invalid password") == 0) {
                // Intercept password errors and trigger the password popup UI
                snprintf(s_password_target_room, sizeof(s_password_target_room), "%s", s_join_room_code.c_str());
                memset(s_password_input, 0, sizeof(s_password_input));
                s_password_input_len = 0;
                s_password_char_idx = 0;
                s_password_popup_mode = 0; // join mode
                s_password_input_display = "_";
                s_password_popup_visible = true;

                s_room_status = "";
                s_join_room_code = "";
                s_model_handle.DirtyVariable("password_popup_visible");
                s_model_handle.DirtyVariable("password_input_display");
                s_model_handle.DirtyVariable("room_status");
                s_model_handle.DirtyVariable("join_room_code");
            } else {
                if (s_room_async_error[0]) {
                    s_room_status = Rml::String(s_room_async_error);
                    s_room_async_error[0] = '\0';
                } else {
                    s_room_status = "FAILED";
                }
                s_join_room_code = "";
                s_model_handle.DirtyVariable("room_status");
                s_model_handle.DirtyVariable("join_room_code");
            }
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION, "[NetworkLobby] Room create/join failed: %s", s_room_status.c_str());
        }
    }

    DIRTY_INT(cursor, (int)g_state.Menu_Cursor_Y[0]);
    DIRTY_BOOL(lan_auto, Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT));
    DIRTY_BOOL(net_auto, Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
    DIRTY_BOOL(net_search_toggle, Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH));
    DIRTY_BOOL(net_searching, SDLNetplayUI_IsSearching());
    DIRTY_BOOL(net_is_configured, LobbyServer_IsConfigured());
    DIRTY_BOOL(region_lock, Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK));
    DIRTY_INT(max_ping, Config_GetInt(CFG_KEY_NETPLAY_MAX_PING));
    DIRTY_BOOL(block_wifi, Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI));
    DIRTY_INT(ft_value, Config_GetInt(CFG_KEY_NETPLAY_FT));
    DIRTY_STR(room_status, s_room_status);
    DIRTY_STR(join_room_code, s_join_room_code);

    {
        const char* rc = SDLNetplayUI_GetRoomCode();
        DIRTY_STR(room_code, Rml::String(rc ? rc : ""));
    }

    // ── Rebuild LAN peer list ─────────────────────────────────────
    {
        NetplayDiscoveredPeer raw[16];
        int c = Discovery_GetPeers(raw, 16);

        std::vector<LanPeerItem> next;
        next.reserve((size_t)c);
        for (int i = 0; i < c; i++) {
            LanPeerItem item;
            item.name = Rml::String(raw[i].name);
            item.selected = (i == g_lobby_peer_idx);
            next.push_back(item);
        }
        if (next != s_lan_peers) {
            s_lan_peers = std::move(next);
            s_model_handle.DirtyVariable("lan_peers");
        }
        // Dirty count AFTER array so RmlUi never sees count>0 with empty array
        DIRTY_INT(lan_peer_count, c);
        DIRTY_INT(lan_peer_idx, g_lobby_peer_idx);
    }

    // ── Rebuild NET peer list ─────────────────────────────────────
    {
        int nc = SDLNetplayUI_GetOnlinePlayerCount();

        std::vector<NetPeerItem> next;
        next.reserve((size_t)nc);
        for (int i = 0; i < nc; i++) {
            NetPeerItem item;
            item.name = Rml::String(SDLNetplayUI_GetOnlinePlayerName(i));

            // g_state.Country code + flag icon path
            const char* cc = SDLNetplayUI_GetOnlinePlayerCountry(i);
            item.country = Rml::String(cc ? cc : "");
            if (cc && cc[0] && cc[1]) {
                char lower[3] = { (char)tolower((unsigned char)cc[0]), (char)tolower((unsigned char)cc[1]), 0 };
                char path[64];
                SDL_snprintf(path, sizeof(path), "../flags_icons/%s.png", lower);
                item.flag_icon = Rml::String(path);
            }

            // Connection type
            const char* ct = SDLNetplayUI_GetOnlinePlayerConnType(i);
            item.conn_type = Rml::String(ct ? ct : "unknown");

            // Ping
            int ping = SDLNetplayUI_GetOnlinePlayerPing(i);
            if (ping >= 0) {
                char buf[16];
                SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
                item.ping_label = Rml::String(buf);
                if (ping < 60)
                    item.ping_class = "ping-good";
                else if (ping < 120)
                    item.ping_class = "ping-ok";
                else
                    item.ping_class = "ping-bad";
            } else {
                item.ping_label = "...";
                item.ping_class = "ping-bad";
            }

            item.selected = (i == g_net_peer_idx);
            next.push_back(item);
        }
        if (next != s_net_peers) {
            s_net_peers = std::move(next);
            s_model_handle.DirtyVariable("net_peers");
        }
        // Dirty count AFTER array so RmlUi never sees count>0 with empty array
        DIRTY_INT(net_peer_count, nc);
        DIRTY_INT(net_peer_idx, g_net_peer_idx);
    }

    // ── Rebuild ROOM list (poll every 3 seconds, async) ──────────────
    {
        Uint64 now = SDL_GetTicks();

        // Kick off async fetch every 3 seconds
        if (now - s_room_list_poll_time > 3000) {
            s_room_list_poll_time = now;
            AsyncFetchRoomList();
        }

        // Consume result when ready
        if (SDL_GetAtomicInt(&s_room_fetch_done)) {
            // Acquire barrier pairs with Release in the worker, ensuring safety
            SDL_MemoryBarrierAcquire();
            SDL_SetAtomicInt(&s_room_fetch_done, 0);
            int rc = s_room_fetch_count;
            DIRTY_INT(room_count, rc);

            if (s_room_list_idx >= rc)
                s_room_list_idx = rc > 0 ? rc - 1 : 0;
            DIRTY_INT(room_list_idx, s_room_list_idx);

            std::vector<RoomItem> next;
            next.reserve((size_t)rc);
            for (int i = 0; i < rc; i++) {
                RoomItem item;
                item.code = Rml::String(s_room_fetch_buf[i].code);
                item.name = Rml::String(s_room_fetch_buf[i].name);
                item.player_count = s_room_fetch_buf[i].player_count;
                item.max_players = s_room_fetch_buf[i].max_players;
                item.selected = (i == s_room_list_idx);
                item.is_tournament = (s_room_fetch_buf[i].room_type == ROOM_TYPE_TOURNAMENT);
                item.is_private = (s_room_fetch_buf[i].password_required != 0);
                next.push_back(item);
            }
            if (next != s_room_list) {
                s_room_list = std::move(next);
                s_model_handle.DirtyVariable("room_list");
            }
        }
    }

    // Popup type check
    {
        int pt = 0;
        if (SDLNetplayUI_HasPendingInvite())
            pt = 1;
        else if (SDLNetplayUI_HasOutgoingChallenge())
            pt = 2;
        else if (Discovery_GetChallengeTarget() != 0)
            pt = 3;
        else {
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            for (int i = 0; i < count; i++) {
                if (peers[i].is_challenging_me) {
                    pt = 4;
                    break;
                }
            }
        }
        DIRTY_INT(popup_type, pt);
    }

    // Always dirty dynamic string fields — cheap since RmlUi skips if DOM unchanged
    s_model_handle.DirtyVariable("lan_peer_name");
    s_model_handle.DirtyVariable("net_peer_name");
    s_model_handle.DirtyVariable("status_text");
    s_model_handle.DirtyVariable("popup_title");
    s_model_handle.DirtyVariable("popup_name");
    s_model_handle.DirtyVariable("popup_ping");
    s_model_handle.DirtyVariable("popup_region");
    s_model_handle.DirtyVariable("popup_is_incoming");
    s_model_handle.DirtyVariable("popup_ft");
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_network_lobby_show(void) {
    s_room_status = "";
    s_join_room_code = "";
    s_room_ready = false; // Clear stale room-ready signal from prior async ops
    rmlui_wrapper_show_game_document("network_lobby");

    if (Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH)) {
        SDLNetplayUI_StartSearch();
    }
}

extern "C" void rmlui_network_lobby_hide(void) {
    rmlui_wrapper_hide_game_document("network_lobby");
}

// ─── Room action handlers (called from menu.c confirm logic) ─────
extern "C" void rmlui_network_lobby_create_room(void) {
    if (SDL_GetAtomicInt(&s_room_async_active) != 0)
        return;
    const char* dn = Identity_GetDisplayName();
    char name[64];

    if (s_create_room_type == ROOM_TYPE_TOURNAMENT) {
        snprintf(name, sizeof(name), "%s's Tournament", dn ? dn : "Player");
        s_room_status = "Creating tournament...";
        if (s_model_handle)
            s_model_handle.DirtyVariable("room_status");

        // Use tournament create path
        SDL_SetAtomicInt(&s_room_async_active, 1);
        SDL_SetAtomicInt(&s_room_async_done, 0);
        AsyncRoomData* d = (AsyncRoomData*)calloc(1, sizeof(AsyncRoomData));
        d->action = 3; // tournament create
        d->ft = Config_GetInt(CFG_KEY_NETPLAY_FT);
        d->room_type = ROOM_TYPE_TOURNAMENT;
        d->tournament_fmt = s_create_tournament_format;
        d->visibility = s_create_room_visibility;
        if (s_create_room_password[0])
            snprintf(d->password, sizeof(d->password), "%s", s_create_room_password);
        snprintf(d->name, sizeof(d->name), "%s", name);
        SDL_Thread* t = SDL_CreateThread(async_room_fn, "AsyncCreateTRoom", d);
        if (t) {
            SDL_DetachThread(t);
        } else {
            free(d);
            SDL_SetAtomicInt(&s_room_async_active, 0);
        }
    } else {
        snprintf(name, sizeof(name), "%s's Room", dn ? dn : "Player");
        s_room_status = "Creating...";
        if (s_model_handle)
            s_model_handle.DirtyVariable("room_status");
        AsyncCreateRoom(name, s_create_room_password[0] ? s_create_room_password : NULL, s_create_room_visibility);
    }

    // Clear password after use and reset label
    memset(s_create_room_password, 0, sizeof(s_create_room_password));
    s_create_password_label = "NONE";
    if (s_model_handle)
        s_model_handle.DirtyVariable("create_password_label");
}

extern "C" void rmlui_network_lobby_cycle_room_type(int direction) {
    (void)direction;
    // Toggle between CASUAL and TOURNAMENT
    if (s_create_room_type == ROOM_TYPE_CASUAL)
        s_create_room_type = ROOM_TYPE_TOURNAMENT;
    else
        s_create_room_type = ROOM_TYPE_CASUAL;
    s_create_room_type_label = room_type_label(s_create_room_type);
    if (s_model_handle) {
        s_model_handle.DirtyVariable("create_room_type");
        s_model_handle.DirtyVariable("create_room_type_label");
    }
}

extern "C" int rmlui_network_lobby_get_create_room_type(void) {
    return s_create_room_type;
}

extern "C" void rmlui_network_lobby_cycle_tournament_format(int direction) {
    static const int formats[] = {
        TOURNAMENT_SINGLE_ELIM, TOURNAMENT_DOUBLE_ELIM, TOURNAMENT_ROUND_ROBIN, TOURNAMENT_SWISS
    };
    static const int fmt_count = 4;
    int idx = 0;
    for (int i = 0; i < fmt_count; i++) {
        if (formats[i] == s_create_tournament_format) {
            idx = i;
            break;
        }
    }
    if (direction < 0)
        idx = (idx - 1 + fmt_count) % fmt_count;
    else
        idx = (idx + 1) % fmt_count;
    s_create_tournament_format = formats[idx];
    s_create_tournament_format_label = tournament_format_label(s_create_tournament_format);
    if (s_model_handle) {
        s_model_handle.DirtyVariable("create_tournament_format");
        s_model_handle.DirtyVariable("create_tournament_format_label");
    }
}

extern "C" void rmlui_network_lobby_join_room(void) {
    // Join the selected room from the list
    if (s_room_list.empty())
        return;
    if (s_room_list_idx < 0 || s_room_list_idx >= (int)s_room_list.size())
        return;
    if (SDL_GetAtomicInt(&s_room_async_active) != 0)
        return;

    const RoomItem& selected = s_room_list[s_room_list_idx];

    // If room is private, show password popup instead of joining directly
    if (selected.is_private) {
        snprintf(s_password_target_room, sizeof(s_password_target_room), "%s", selected.code.c_str());
        memset(s_password_input, 0, sizeof(s_password_input));
        s_password_input_len = 0;
        s_password_char_idx = 0;
        s_password_popup_mode = 0; // join mode
        s_password_input_display = "_";
        s_password_popup_visible = true;
        if (s_model_handle) {
            s_model_handle.DirtyVariable("password_popup_visible");
            s_model_handle.DirtyVariable("password_input_display");
        }
        return;
    }

    s_join_room_code = selected.code;
    s_room_status = "Joining...";
    if (s_model_handle) {
        s_model_handle.DirtyVariable("join_room_code");
        s_model_handle.DirtyVariable("room_status");
    }
    AsyncJoinRoom(selected.code.c_str(), NULL);
}

/// Scroll room list selection (called from menu.c on Left/Right when cursor == 9)
extern "C" void rmlui_network_lobby_room_scroll(int delta) {
    int count = (int)s_room_list.size();
    if (count == 0)
        return;
    s_room_list_idx = (s_room_list_idx + delta + count) % count;
    // Update selection flags
    for (int i = 0; i < count; i++)
        s_room_list[i].selected = (i == s_room_list_idx);
    if (s_model_handle) {
        s_model_handle.DirtyVariable("room_list");
        s_model_handle.DirtyVariable("room_list_idx");
    }
}

// ─── Visibility cycle ────────────────────────────────────────────
extern "C" void rmlui_network_lobby_cycle_visibility(int direction) {
    (void)direction;
    if (s_create_room_visibility == ROOM_VISIBILITY_PUBLIC)
        s_create_room_visibility = ROOM_VISIBILITY_HIDDEN;
    else
        s_create_room_visibility = ROOM_VISIBILITY_PUBLIC;
    s_create_room_visibility_label = (s_create_room_visibility == ROOM_VISIBILITY_HIDDEN) ? "PRIVATE" : "PUBLIC";

    // When switching back to PUBLIC, clear any stored creation password
    if (s_create_room_visibility == ROOM_VISIBILITY_PUBLIC) {
        memset(s_create_room_password, 0, sizeof(s_create_room_password));
        s_create_password_label = "NONE";
    }

    if (s_model_handle) {
        s_model_handle.DirtyVariable("create_room_visibility");
        s_model_handle.DirtyVariable("create_room_visibility_label");
        s_model_handle.DirtyVariable("create_password_label");
    }
}

extern "C" int rmlui_network_lobby_get_visibility(void) {
    return s_create_room_visibility;
}

// ─── Password popup actions ──────────────────────────────────────

/// Helper: update display string from current input
static void password_update_display(void) {
    if (s_password_input_len == 0) {
        s_password_input_display = "_";
    } else {
        s_password_input_display = s_password_input;
    }
    if (s_model_handle)
        s_model_handle.DirtyVariable("password_input_display");
}

extern "C" void rmlui_network_lobby_password_input(int key) {
    // key codes for arcade-style cycling from gamepad:
    //   1 = UP   (cycle forward)
    //   2 = DOWN (cycle backward)
    //   3 = RIGHT (append current char and advance)
    //   4 = LEFT  (backspace)
    // key >= 32: direct ASCII character (keyboard passthrough)
    // key == 8: backspace (keyboard)

    if (key == 1) {
        // UP: cycle forward through charset
        s_password_char_idx = (s_password_char_idx + 1) % PASSWORD_CHARSET_LEN;
        if (s_password_input_len > 0) {
            s_password_input[s_password_input_len - 1] = PASSWORD_CHARSET[s_password_char_idx];
        } else {
            // First char: append
            s_password_input[0] = PASSWORD_CHARSET[s_password_char_idx];
            s_password_input[1] = '\0';
            s_password_input_len = 1;
        }
        password_update_display();
        return;
    }

    if (key == 2) {
        // DOWN: cycle backward through charset
        s_password_char_idx = (s_password_char_idx - 1 + PASSWORD_CHARSET_LEN) % PASSWORD_CHARSET_LEN;
        if (s_password_input_len > 0) {
            s_password_input[s_password_input_len - 1] = PASSWORD_CHARSET[s_password_char_idx];
        } else {
            s_password_input[0] = PASSWORD_CHARSET[s_password_char_idx];
            s_password_input[1] = '\0';
            s_password_input_len = 1;
        }
        password_update_display();
        return;
    }

    if (key == 3) {
        // RIGHT: lock current char and advance to next position
        if (s_password_input_len > 0 && s_password_input_len < 63) {
            // Current char already in buffer from UP/DOWN cycling
            s_password_input_len++;
            s_password_input[s_password_input_len - 1] = PASSWORD_CHARSET[0];
            s_password_input[s_password_input_len] = '\0';
            s_password_char_idx = 0;
        }
        password_update_display();
        return;
    }

    if (key == 4 || key == 8) {
        // LEFT or Backspace: delete last char
        if (s_password_input_len > 0) {
            s_password_input[--s_password_input_len] = '\0';
            s_password_char_idx = 0;
        }
        password_update_display();
        return;
    }

    // Direct ASCII key (keyboard passthrough)
    if (key >= 32 && key < 127 && s_password_input_len < 63) {
        s_password_input[s_password_input_len++] = (char)key;
        s_password_input[s_password_input_len] = '\0';
        s_password_char_idx = 0;
    }
    password_update_display();
}

extern "C" void rmlui_network_lobby_submit_password(void) {
    if (!s_password_popup_visible)
        return;

    // Strip trailing cycling char if it was just added by cycling (len > 0 but only cycled, never "locked")
    // Actually each char is valid — the buffer is always the full typed password.

    s_password_popup_visible = false;

    if (s_password_popup_mode == 1) {
        // CREATE MODE: store password for next room creation
        snprintf(s_create_room_password, sizeof(s_create_room_password), "%s", s_password_input);
        s_create_password_label = s_password_input_len > 0 ? s_password_input : "NONE";
        if (s_model_handle) {
            s_model_handle.DirtyVariable("password_popup_visible");
            s_model_handle.DirtyVariable("create_password_label");
        }
    } else if (s_password_popup_mode == 2) {
        // JOIN BY CODE MODE: input is the room code
        s_join_room_code = Rml::String(s_password_input);
        s_room_status = "Joining...";
        if (s_model_handle) {
            s_model_handle.DirtyVariable("password_popup_visible");
            s_model_handle.DirtyVariable("join_room_code");
            s_model_handle.DirtyVariable("room_status");
        }
        AsyncJoinRoom(s_password_input, NULL);
    } else {
        // JOIN MODE: join the room with the typed password
        s_join_room_code = Rml::String(s_password_target_room);
        s_room_status = "Joining...";
        if (s_model_handle) {
            s_model_handle.DirtyVariable("password_popup_visible");
            s_model_handle.DirtyVariable("join_room_code");
            s_model_handle.DirtyVariable("room_status");
        }
        AsyncJoinRoom(s_password_target_room, s_password_input[0] ? s_password_input : NULL);
    }

    // Clear password state
    memset(s_password_input, 0, sizeof(s_password_input));
    s_password_input_len = 0;
    s_password_char_idx = 0;
    s_password_target_room[0] = '\0';
}

extern "C" void rmlui_network_lobby_cancel_password(void) {
    s_password_popup_visible = false;
    memset(s_password_input, 0, sizeof(s_password_input));
    s_password_input_len = 0;
    s_password_char_idx = 0;
    s_password_target_room[0] = '\0';
    s_password_input_display = "";
    if (s_model_handle) {
        s_model_handle.DirtyVariable("password_popup_visible");
        s_model_handle.DirtyVariable("password_input_display");
    }
}

extern "C" void rmlui_network_lobby_open_create_password(void) {
    memset(s_password_input, 0, sizeof(s_password_input));
    s_password_input_len = 0;
    s_password_char_idx = 0;
    s_password_popup_mode = 1; // create mode
    s_password_input_display = "_";
    s_password_popup_visible = true;
    if (s_model_handle) {
        s_model_handle.DirtyVariable("password_popup_visible");
        s_model_handle.DirtyVariable("password_input_display");
    }
}

extern "C" void rmlui_network_lobby_open_join_code(void) {
    memset(s_password_input, 0, sizeof(s_password_input));
    s_password_input_len = 0;
    s_password_char_idx = 0;
    s_password_popup_mode = 2; // join by code mode
    s_password_input_display = "_";
    s_password_popup_visible = true;
    if (s_model_handle) {
        s_model_handle.DirtyVariable("password_popup_visible");
        s_model_handle.DirtyVariable("password_input_display");
    }
}

extern "C" bool rmlui_network_lobby_is_password_popup_visible(void) {
    return s_password_popup_visible;
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_network_lobby_shutdown(void) {
    // Disconnect global SSE when leaving network lobby
    LobbyServer_GlobalSSEDisconnect();

    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("network_lobby");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("network_lobby");
        s_model_registered = false;
    }
    s_lan_peers.clear();
    s_net_peers.clear();
    s_room_list.clear();
}

extern "C" bool rmlui_network_lobby_has_pending_room(void) {
    return s_room_ready;
}

extern "C" const char* rmlui_network_lobby_consume_pending_room(void) {
    s_room_ready = false;
    return s_room_async_code;
}

extern "C" bool rmlui_network_lobby_pending_room_is_tournament(void) {
    return s_room_async_type == ROOM_TYPE_TOURNAMENT;
}

extern "C" void rmlui_network_lobby_reset(void) {
    s_room_async_code[0] = '\0';
    s_room_async_error[0] = '\0';
    s_active_room_password[0] = '\0';
    SDL_SetAtomicInt(&s_room_async_ok, 0);
    SDL_SetAtomicInt(&s_room_async_done, 0);
    SDL_SetAtomicInt(&s_room_async_active, 0);
}

extern "C" const char* rmlui_network_lobby_get_active_password(void) {
    return s_active_room_password;
}

extern "C" const char* rmlui_network_lobby_get_qr_image_path(void) {
    return s_qr_image_path;
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
