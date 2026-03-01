/**
 * @file rmlui_network_lobby.cpp
 * @brief RmlUi Network Lobby data model.
 *
 * Replaces CPS3's effect_61/57/66 objects and SSPutStr_Bigger/
 * Renderer_Queue2DPrimitive rendering in Network_Lobby() with
 * an RmlUi overlay showing lobby items, peer lists, and popup modals.
 *
 * Key APIs:
 *   Menu_Cursor_Y[0] — cursor position (6 items)
 *   Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT) — LAN auto-connect
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT) — NET auto-connect
 *   Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH) — NET auto-search
 *   Discovery_GetPeers() — LAN peer list
 *   SDLNetplayUI_* — Internet search, peer list, invite/challenge state
 */

#include "port/sdl/rmlui_network_lobby.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include "netplay/discovery.h"
#include "port/config.h"
#include "port/sdl/sdl_netplay_ui.h"
#include "sf33rd/Source/Game/engine/workuser.h"

extern int g_lobby_peer_idx;
extern int g_net_peer_idx;
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct LobbyCache {
    int cursor;
    bool lan_auto;
    bool net_auto;
    bool net_search_toggle;
    bool net_searching;
    int lan_peer_count;
    int net_peer_count;
    int lan_peer_idx;
    int net_peer_idx;
    int popup_type; // 0=none, 1=incoming, 2=outgoing
};
static LobbyCache s_cache = {};

// Externally tracked peer indices — these are static in Network_Lobby(),
// but we need them here for the bindings. We'll read them via the
// lobby_cursor and the Discovery/SDLNetplayUI APIs directly.
// The peer selection index is tracked in menu.c's static variables;
// we display the name by scanning peers matching the cursor context.

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

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_network_lobby_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("network_lobby");
    if (!ctor)
        return;

    ctor.BindFunc("cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    // Toggle states
    ctor.BindFunc("lan_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT); });
    ctor.BindFunc("net_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT); });
    ctor.BindFunc("net_search_toggle", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH); });
    ctor.BindFunc("net_searching", [](Rml::Variant& v) { v = SDLNetplayUI_IsSearching(); });

    // LAN peer info
    ctor.BindFunc("lan_peer_count", [](Rml::Variant& v) {
        NetplayDiscoveredPeer peers[16];
        v = Discovery_GetPeers(peers, 16);
    });
    ctor.BindFunc("lan_peer_name", [](Rml::Variant& v) {
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        if (count > 0) {
            int idx = g_lobby_peer_idx;
            if (idx < 0) idx = 0;
            if (idx >= count) idx = count - 1;
            v = Rml::String(peers[idx].name);
        } else {
            v = Rml::String("NONE");
        }
    });
    ctor.BindFunc("lan_peer_idx", [](Rml::Variant& v) { v = g_lobby_peer_idx; });

    // NET peer info
    ctor.BindFunc("net_peer_count", [](Rml::Variant& v) { v = SDLNetplayUI_GetOnlinePlayerCount(); });
    ctor.BindFunc("net_peer_name", [](Rml::Variant& v) {
        int count = SDLNetplayUI_GetOnlinePlayerCount();
        if (count > 0) {
            int idx = g_net_peer_idx;
            if (idx < 0) idx = 0;
            if (idx >= count) idx = count - 1;
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
        // Check challenge states
        NetplayDiscoveredPeer peers[16];
        int count = Discovery_GetPeers(peers, 16);
        int target = Discovery_GetChallengeTarget();

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
                if ((int)peers[i].instance_id == target) {
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
        // 0=none, 1=incoming(internet), 2=outgoing(internet),
        // 3=outgoing(LAN), 4=incoming(LAN)
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
        // Check LAN incoming
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
        if (SDLNetplayUI_HasPendingInvite() || false) {
            // Check LAN incoming
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            bool lan_in = false;
            for (int i = 0; i < count; i++) {
                if (peers[i].is_challenging_me) {
                    lan_in = true;
                    break;
                }
            }
            if (SDLNetplayUI_HasPendingInvite() || lan_in) {
                v = Rml::String("INCOMING CHALLENGE!");
                return;
            }
        }
        if (SDLNetplayUI_HasOutgoingChallenge() || Discovery_GetChallengeTarget() != 0) {
            v = Rml::String("CONNECTING...");
            return;
        }
        // LAN incoming fallback
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
        // LAN challenge target
        int target = Discovery_GetChallengeTarget();
        if (target != 0) {
            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);
            for (int i = 0; i < count; i++) {
                if ((int)peers[i].instance_id == target) {
                    v = Rml::String(peers[i].name);
                    return;
                }
            }
            v = Rml::String("...");
            return;
        }
        // LAN incoming
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

    SDL_Log("[RmlUi NetworkLobby] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_network_lobby_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(cursor, (int)Menu_Cursor_Y[0]);
    DIRTY_BOOL(lan_auto, Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT));
    DIRTY_BOOL(net_auto, Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
    DIRTY_BOOL(net_search_toggle, Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH));
    DIRTY_BOOL(net_searching, SDLNetplayUI_IsSearching());

    {
        NetplayDiscoveredPeer peers[16];
        int c = Discovery_GetPeers(peers, 16);
        DIRTY_INT(lan_peer_count, c);
    }
    DIRTY_INT(net_peer_count, SDLNetplayUI_GetOnlinePlayerCount());
    DIRTY_INT(lan_peer_idx, g_lobby_peer_idx);
    DIRTY_INT(net_peer_idx, g_net_peer_idx);

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
}

#undef DIRTY_INT
#undef DIRTY_BOOL
