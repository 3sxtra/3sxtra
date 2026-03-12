/**
 * @file rmlui_network_lobby.cpp
 * @brief RmlUi Network Lobby data model.
 *
 * Replaces CPS3's effect_61/57/66 objects and SSPutStr_Bigger/
 * Renderer_Queue2DPrimitive rendering in Network_Lobby() with
 * an RmlUi overlay showing lobby items, peer lists, and popup modals.
 *
 * Key APIs:
 *   Menu_Cursor_Y[0] — cursor position (9 items: 0-8)
 *   Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT) — LAN auto-connect
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT) — NET auto-connect
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH) — NET auto-search
 *   Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK) — region lock
 *   Config_GetInt(CFG_KEY_NETPLAY_MAX_PING) — max ping filter
 *   Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI) — block WiFi
 *   Discovery_GetPeers() — LAN peer list
 *   SDLNetplayUI_* — Internet search, peer list, invite/challenge state
 *   LobbyServer_IsConfigured() — whether internet lobby is set up
 */

#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/discovery.h"
#include "netplay/lobby_server.h"
#include "port/config/config.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "sf33rd/Source/Game/engine/workuser.h"

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
    Rml::String country;     // ISO 3166-1 alpha-2 (e.g. "US", "JP")
    Rml::String ping_label;  // e.g. "~42ms" or "..."
    Rml::String ping_class;  // "ping-good" | "ping-ok" | "ping-bad"
    Rml::String conn_type;   // "wifi" | "wired" | "unknown"
    bool selected;           // true when g_net_peer_idx == this row

    bool operator==(const NetPeerItem& o) const {
        return name == o.name && country == o.country && ping_label == o.ping_label &&
               ping_class == o.ping_class && conn_type == o.conn_type && selected == o.selected;
    }
    bool operator!=(const NetPeerItem& o) const {
        return !(*this == o);
    }
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Live peer lists — rebuilt each update frame
static std::vector<LanPeerItem> s_lan_peers;
static std::vector<NetPeerItem> s_net_peers;

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
    int lan_peer_count;
    int net_peer_count;
    int lan_peer_idx;
    int net_peer_idx;
    int popup_type; // 0=none, 1=incoming, 2=outgoing
    Rml::String room_code;
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


extern "C" void rmlui_network_lobby_init(void) {
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
        h.RegisterMember("ping_label", &NetPeerItem::ping_label);
        h.RegisterMember("ping_class", &NetPeerItem::ping_class);
        h.RegisterMember("conn_type", &NetPeerItem::conn_type);
        h.RegisterMember("selected", &NetPeerItem::selected);
    }
    ctor.RegisterArray<std::vector<NetPeerItem>>();

    // ── Bind peer lists ──────────────────────────────────────────
    ctor.Bind("lan_peers", &s_lan_peers);
    ctor.Bind("net_peers", &s_net_peers);

    // ── Scalar bindings ──────────────────────────────────────────
    ctor.BindFunc("cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    ctor.BindFunc("lan_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT); });
    ctor.BindFunc("net_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT); });
    ctor.BindFunc("net_search_toggle", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH); });
    ctor.BindFunc("net_searching", [](Rml::Variant& v) { v = SDLNetplayUI_IsSearching(); });
    ctor.BindFunc("net_is_configured", [](Rml::Variant& v) { v = LobbyServer_IsConfigured(); });

    // New filter toggles
    ctor.BindFunc("region_lock", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK); });
    ctor.BindFunc("max_ping", [](Rml::Variant& v) { v = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING); });
    ctor.BindFunc("block_wifi", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI); });

    // Room code (shown under title bar)
    ctor.BindFunc("room_code", [](Rml::Variant& v) {
        const char* rc = SDLNetplayUI_GetRoomCode();
        v = Rml::String(rc ? rc : "");
    });

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
                    v = Rml::String(peers[i].name);
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
                v = Rml::String(peers[i].name);
                return;
            }
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_ping", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            int ping = SDLNetplayUI_GetPendingInvitePing();
            char buf[32];
            if (ping < 0)
                SDL_snprintf(buf, sizeof(buf), "...");
            else
                SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
            v = Rml::String(buf);
            return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            int ping = SDLNetplayUI_GetOutgoingChallengePing();
            char buf[32];
            if (ping < 0)
                SDL_snprintf(buf, sizeof(buf), "...");
            else
                SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
            v = Rml::String(buf);
            return;
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

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi NetworkLobby] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_network_lobby_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("network_lobby"))
        return;

    DIRTY_INT(cursor, (int)Menu_Cursor_Y[0]);
    DIRTY_BOOL(lan_auto, Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT));
    DIRTY_BOOL(net_auto, Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
    DIRTY_BOOL(net_search_toggle, Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH));
    DIRTY_BOOL(net_searching, SDLNetplayUI_IsSearching());
    DIRTY_BOOL(net_is_configured, LobbyServer_IsConfigured());
    DIRTY_BOOL(region_lock, Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK));
    DIRTY_INT(max_ping, Config_GetInt(CFG_KEY_NETPLAY_MAX_PING));
    DIRTY_BOOL(block_wifi, Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI));

    {
        const char* rc = SDLNetplayUI_GetRoomCode();
        DIRTY_STR(room_code, Rml::String(rc ? rc : ""));
    }

    // ── Rebuild LAN peer list ─────────────────────────────────────
    {
        NetplayDiscoveredPeer raw[16];
        int c = Discovery_GetPeers(raw, 16);
        DIRTY_INT(lan_peer_count, c);
        DIRTY_INT(lan_peer_idx, g_lobby_peer_idx);

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
    }

    // ── Rebuild NET peer list ─────────────────────────────────────
    {
        int nc = SDLNetplayUI_GetOnlinePlayerCount();
        DIRTY_INT(net_peer_count, nc);
        DIRTY_INT(net_peer_idx, g_net_peer_idx);

        std::vector<NetPeerItem> next;
        next.reserve((size_t)nc);
        for (int i = 0; i < nc; i++) {
            NetPeerItem item;
            item.name = Rml::String(SDLNetplayUI_GetOnlinePlayerName(i));

            // Country code
            const char* cc = SDLNetplayUI_GetOnlinePlayerCountry(i);
            item.country = Rml::String(cc ? cc : "");

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
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_network_lobby_show(void) {
    rmlui_wrapper_show_game_document("network_lobby");
}

extern "C" void rmlui_network_lobby_hide(void) {
    rmlui_wrapper_hide_game_document("network_lobby");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_network_lobby_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("network_lobby");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("network_lobby");
        s_model_registered = false;
    }
    s_lan_peers.clear();
    s_net_peers.clear();
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
