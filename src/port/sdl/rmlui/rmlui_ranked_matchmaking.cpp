#include "port/sdl/rmlui/rmlui_ranked_matchmaking.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#ifdef ENABLE_RMLUI
#include "RmlUi/Core.h"
#include <SDL3/SDL.h>

extern "C" {
#include "port/config/config.h"
#include "port/config/paths.h"
#include "structs.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "netplay/discovery.h"
#include "netplay/lobby_server.h"
#include "netplay/ping_probe.h"

extern s16 g_net_peer_idx;
extern s16 Menu_Cursor_Y[2];
}

#include <vector>

namespace {

struct RankedPeerItem {
    Rml::String name;
    Rml::String country;
    Rml::String flag_icon;
    Rml::String ping_label;
    Rml::String ping_class;
    Rml::String conn_type;
    bool selected;

    bool operator==(const RankedPeerItem& o) const {
        return name == o.name && country == o.country && flag_icon == o.flag_icon &&
               ping_label == o.ping_label && ping_class == o.ping_class &&
               conn_type == o.conn_type && selected == o.selected;
    }
    bool operator!=(const RankedPeerItem& o) const { return !(*this == o); }
};

static std::vector<RankedPeerItem> s_net_peers;

// State
static bool s_model_registered = false;
static Rml::DataModelHandle s_model_handle;
static bool s_wants_leave = false;

struct RankedLobbyCache {
    bool net_auto;
    bool net_search_toggle;
    bool net_searching;
    bool net_is_configured;
    bool region_lock;
    int max_ping;
    bool block_wifi;
    int ft_value;
    int net_peer_count;
    int net_peer_idx;
    int cursor;
    int popup_type; // 0=none, 1=incoming, 2=outgoing
};
static RankedLobbyCache s_cache = {};

#define DIRTY_INT(nm, expr) \
    do { \
        int _v = (expr); \
        if (_v != s_cache.nm) { \
            s_cache.nm = _v; \
            s_model_handle.DirtyVariable(#nm); \
        } \
    } while (0)

#define DIRTY_BOOL(nm, expr) \
    do { \
        bool _v = (expr); \
        if (_v != s_cache.nm) { \
            s_cache.nm = _v; \
            s_model_handle.DirtyVariable(#nm); \
        } \
    } while (0)

static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("ranked_matchmaking");
    if (!ctor) return;

    if (auto h = ctor.RegisterStruct<RankedPeerItem>()) {
        h.RegisterMember("name", &RankedPeerItem::name);
        h.RegisterMember("country", &RankedPeerItem::country);
        h.RegisterMember("flag_icon", &RankedPeerItem::flag_icon);
        h.RegisterMember("ping_label", &RankedPeerItem::ping_label);
        h.RegisterMember("ping_class", &RankedPeerItem::ping_class);
        h.RegisterMember("conn_type", &RankedPeerItem::conn_type);
        h.RegisterMember("selected", &RankedPeerItem::selected);
    }
    ctor.RegisterArray<std::vector<RankedPeerItem>>();
    ctor.Bind("net_peers", &s_net_peers);

    ctor.BindFunc("net_auto", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT); });
    ctor.BindFunc("net_search_toggle", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH); });
    ctor.BindFunc("net_searching", [](Rml::Variant& v) { v = SDLNetplayUI_IsSearching(); });
    ctor.BindFunc("net_is_configured", [](Rml::Variant& v) { v = LobbyServer_IsConfigured(); });

    ctor.BindFunc("region_lock", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK); });
    ctor.BindFunc("max_ping", [](Rml::Variant& v) { v = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING); });
    ctor.BindFunc("block_wifi", [](Rml::Variant& v) { v = Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI); });
    ctor.BindFunc("ft_value", [](Rml::Variant& v) { v = Config_GetInt(CFG_KEY_NETPLAY_FT); });

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
    ctor.BindFunc("cursor", [](Rml::Variant& v) {
        v = (int)Menu_Cursor_Y[0];
    });

    ctor.BindFunc("status_text", [](Rml::Variant& v) {
        const char* msg = SDLNetplayUI_GetStatusMsg();
        if (msg[0]) {
            v = Rml::String(msg);
            return;
        }
        if (SDLNetplayUI_IsSearching()) {
            v = Rml::String("DISCOVERING...");
            return;
        }
        v = Rml::String("");
    });

    ctor.BindFunc("popup_type", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = 1; return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = 2; return;
        }
        v = 0;
    });
    ctor.BindFunc("popup_title", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = Rml::String("INCOMING CHALLENGE!"); return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = Rml::String("CONNECTING..."); return;
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_name", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = Rml::String(SDLNetplayUI_GetPendingInviteName()); return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = Rml::String(SDLNetplayUI_GetOutgoingChallengeName()); return;
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_ping", [](Rml::Variant& v) {
        auto format_ping = [](int ping) -> Rml::String {
            char buf[32];
            if (ping < 0) SDL_snprintf(buf, sizeof(buf), "...");
            else SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
            return Rml::String(buf);
        };
        if (SDLNetplayUI_HasPendingInvite()) {
            v = format_ping(SDLNetplayUI_GetPendingInvitePing()); return;
        }
        if (SDLNetplayUI_HasOutgoingChallenge()) {
            v = format_ping(SDLNetplayUI_GetOutgoingChallengePing()); return;
        }
        v = Rml::String("...");
    });
    ctor.BindFunc("popup_region", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            const char* r = SDLNetplayUI_GetPendingInviteRegion();
            v = Rml::String(r ? r : ""); return;
        }
        v = Rml::String("");
    });
    ctor.BindFunc("popup_is_incoming", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            v = true; return;
        }
        v = false;
    });
    ctor.BindFunc("popup_ft", [](Rml::Variant& v) {
        if (SDLNetplayUI_HasPendingInvite()) {
            int ft = SDLNetplayUI_GetPendingInviteFT();
            if (ft == 1) v = Rml::String("UNRANKED");
            else {
                char buf[16];
                snprintf(buf, sizeof(buf), "FT%d", ft);
                v = Rml::String(buf);
            }
            return;
        }
        v = Rml::String("");
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi RankedMatchmaking] Data model registered");
}

} // namespace

extern "C" void rmlui_ranked_matchmaking_init(void) {
    do_init();
}

extern "C" void rmlui_ranked_matchmaking_update(void) {
    if (!s_model_registered) { do_init(); if (!s_model_registered) return; }
    if (!s_model_handle) return;
    if (!rmlui_wrapper_is_game_document_visible("ranked_matchmaking")) return;

    DIRTY_BOOL(net_auto, Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
    DIRTY_BOOL(net_search_toggle, Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH));
    DIRTY_BOOL(net_searching, SDLNetplayUI_IsSearching());
    DIRTY_BOOL(net_is_configured, LobbyServer_IsConfigured());
    DIRTY_BOOL(region_lock, Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK));
    DIRTY_INT(max_ping, Config_GetInt(CFG_KEY_NETPLAY_MAX_PING));
    DIRTY_BOOL(block_wifi, Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI));
    DIRTY_INT(ft_value, Config_GetInt(CFG_KEY_NETPLAY_FT));

    DIRTY_INT(net_peer_count, SDLNetplayUI_GetOnlinePlayerCount());
    DIRTY_INT(net_peer_idx, g_net_peer_idx);
    DIRTY_INT(cursor, (int)Menu_Cursor_Y[0]);

    s_model_handle.DirtyVariable("status_text");
    {
        int nc = SDLNetplayUI_GetOnlinePlayerCount();
        std::vector<RankedPeerItem> next;
        next.reserve((size_t)nc);
        for (int i = 0; i < nc; i++) {
            RankedPeerItem item;
            item.name = Rml::String(SDLNetplayUI_GetOnlinePlayerName(i));

            const char* cc = SDLNetplayUI_GetOnlinePlayerCountry(i);
            item.country = Rml::String(cc ? cc : "");
            if (cc && cc[0] && cc[1]) {
                char lower[3] = { (char)tolower((unsigned char)cc[0]), (char)tolower((unsigned char)cc[1]), 0 };
                char path[64];
                SDL_snprintf(path, sizeof(path), "../flags_icons/%s.png", lower);
                item.flag_icon = Rml::String(path);
            }

            const char* ct = SDLNetplayUI_GetOnlinePlayerConnType(i);
            item.conn_type = Rml::String(ct ? ct : "unknown");

            int ping = SDLNetplayUI_GetOnlinePlayerPing(i);
            if (ping >= 0) {
                char buf[16];
                SDL_snprintf(buf, sizeof(buf), "~%dms", ping);
                item.ping_label = Rml::String(buf);
                if (ping < 60) item.ping_class = "ping-good";
                else if (ping < 120) item.ping_class = "ping-ok";
                else item.ping_class = "ping-bad";
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
        DIRTY_INT(net_peer_count, nc);
        DIRTY_INT(net_peer_idx, g_net_peer_idx);
    }

    static int s_last_cursor = -1;
    if (Menu_Cursor_Y[0] != s_last_cursor) {
        s_last_cursor = Menu_Cursor_Y[0];
        s_model_handle.DirtyVariable("cursor");
    }

    {
        int pt = 0;
        if (SDLNetplayUI_HasPendingInvite()) {
            pt = 1;
        } else if (SDLNetplayUI_HasOutgoingChallenge()) {
            pt = 2;
        }
        DIRTY_INT(popup_type, pt);
    }

    s_model_handle.DirtyVariable("popup_title");
    s_model_handle.DirtyVariable("popup_name");
    s_model_handle.DirtyVariable("popup_ping");
    s_model_handle.DirtyVariable("popup_region");
    s_model_handle.DirtyVariable("popup_is_incoming");
    s_model_handle.DirtyVariable("popup_ft");
}

extern "C" void rmlui_ranked_matchmaking_show(void) {
    if (!s_model_registered) do_init();
    s_wants_leave = false;
    rmlui_wrapper_show_game_document("ranked_matchmaking");

    if (Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH)) {
        SDLNetplayUI_StartSearch();
    }
}

extern "C" void rmlui_ranked_matchmaking_hide(void) {
    rmlui_wrapper_hide_game_document("ranked_matchmaking");
}

extern "C" bool rmlui_ranked_matchmaking_wants_leave(void) {
    return s_wants_leave;
}

extern "C" void rmlui_ranked_matchmaking_consume_leave(void) {
    s_wants_leave = false;
}

#endif /* ENABLE_RMLUI */
