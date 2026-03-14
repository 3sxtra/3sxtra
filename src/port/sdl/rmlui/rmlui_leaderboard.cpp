/**
 * @file rmlui_leaderboard.cpp
 * @brief RmlUi Leaderboard data model.
 *
 * Displays a paginated leaderboard fetched from the lobby server.
 * Data is fetched asynchronously via SDL threads.
 */

#include "port/sdl/rmlui/rmlui_leaderboard.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
} // extern "C"

// ─── Leaderboard entry struct for data-for ──────────────────────

struct LBItem {
    int rank;
    Rml::String name;
    int wins;
    int losses;
    int win_pct;
    int dc_pct;
    Rml::String rating_str;
    Rml::String tier;
    bool is_me;

    bool operator==(const LBItem& o) const {
        return rank == o.rank && name == o.name && wins == o.wins && losses == o.losses && win_pct == o.win_pct &&
               dc_pct == o.dc_pct && rating_str == o.rating_str && tier == o.tier && is_me == o.is_me;
    }
    bool operator!=(const LBItem& o) const {
        return !(*this == o);
    }
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

static std::vector<LBItem> s_entries;
static int s_page = 0;
static int s_total = 0;
static bool s_loading = false;
static bool s_has_data = false;

// Async fetch state
static SDL_AtomicInt s_fetch_active = { 0 };
static SDL_AtomicInt s_fetch_done = { 0 };

#define LB_PAGE_SIZE 20

struct FetchResult {
    LeaderboardEntry entries[LB_PAGE_SIZE];
    int count;
    int page;
    int total;
};

static FetchResult s_fetch_result;

static int async_fetch_fn(void* userdata) {
    int page = *(int*)userdata;
    free(userdata);

    int total = 0;
    LeaderboardEntry entries[LB_PAGE_SIZE];
    int count = LobbyServer_GetLeaderboard(entries, LB_PAGE_SIZE, page, &total);

    s_fetch_result.count = count;
    s_fetch_result.page = page;
    s_fetch_result.total = total;
    if (count > 0) {
        memcpy(s_fetch_result.entries, entries, sizeof(LeaderboardEntry) * (size_t)count);
    }

    SDL_SetAtomicInt(&s_fetch_done, 1);
    SDL_SetAtomicInt(&s_fetch_active, 0);
    return 0;
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_leaderboard_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("leaderboard");
    if (!ctor)
        return;

    // Register LBItem struct
    if (auto h = ctor.RegisterStruct<LBItem>()) {
        h.RegisterMember("rank", &LBItem::rank);
        h.RegisterMember("name", &LBItem::name);
        h.RegisterMember("wins", &LBItem::wins);
        h.RegisterMember("losses", &LBItem::losses);
        h.RegisterMember("win_pct", &LBItem::win_pct);
        h.RegisterMember("dc_pct", &LBItem::dc_pct);
        h.RegisterMember("rating_str", &LBItem::rating_str);
        h.RegisterMember("tier", &LBItem::tier);
        h.RegisterMember("is_me", &LBItem::is_me);
    }
    ctor.RegisterArray<std::vector<LBItem>>();
    ctor.Bind("entries", &s_entries);

    // Scalars
    ctor.BindFunc("page", [](Rml::Variant& v) { v = s_page + 1; }); // 1-indexed for display
    ctor.BindFunc("total_pages",
                  [](Rml::Variant& v) { v = s_total > 0 ? ((s_total + LB_PAGE_SIZE - 1) / LB_PAGE_SIZE) : 1; });
    ctor.BindFunc("total_players", [](Rml::Variant& v) { v = s_total; });
    ctor.BindFunc("loading", [](Rml::Variant& v) { v = s_loading; });
    ctor.BindFunc("has_data", [](Rml::Variant& v) { v = s_has_data; });
    ctor.BindFunc("has_prev", [](Rml::Variant& v) { v = s_page > 0; });
    ctor.BindFunc("has_next", [](Rml::Variant& v) { v = (s_page + 1) * LB_PAGE_SIZE < s_total; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_leaderboard_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("leaderboard"))
        return;

    // Check if async fetch completed
    if (SDL_GetAtomicInt(&s_fetch_done)) {
        SDL_SetAtomicInt(&s_fetch_done, 0);
        s_loading = false;

        const char* my_id = Identity_GetPlayerId();
        std::vector<LBItem> next;

        if (s_fetch_result.count > 0) {
            s_page = s_fetch_result.page;
            s_total = s_fetch_result.total;
            next.reserve((size_t)s_fetch_result.count);

            for (int i = 0; i < s_fetch_result.count; i++) {
                LeaderboardEntry* e = &s_fetch_result.entries[i];
                LBItem item;
                item.rank = e->rank;
                item.name = Rml::String(e->display_name[0] ? e->display_name : e->player_id);
                item.wins = e->wins;
                item.losses = e->losses;
                int total = e->wins + e->losses + e->disconnects;
                item.win_pct = total > 0 ? (e->wins * 100 / total) : 0;
                item.dc_pct = total > 0 ? (e->disconnects * 100 / total) : 0;

                char buf[16];
                SDL_snprintf(buf, sizeof(buf), "%.0f", (double)e->rating);
                item.rating_str = Rml::String(buf);
                item.tier = Rml::String(e->tier);
                item.is_me = (my_id && my_id[0] && strcmp(my_id, e->player_id) == 0);
                next.push_back(item);
            }
            s_has_data = true;
        }

        if (next != s_entries) {
            s_entries = std::move(next);
            s_model_handle.DirtyVariable("entries");
        }

        s_model_handle.DirtyVariable("page");
        s_model_handle.DirtyVariable("total_pages");
        s_model_handle.DirtyVariable("total_players");
        s_model_handle.DirtyVariable("loading");
        s_model_handle.DirtyVariable("has_data");
        s_model_handle.DirtyVariable("has_prev");
        s_model_handle.DirtyVariable("has_next");
    }
}

// ─── Fetch ───────────────────────────────────────────────────────
extern "C" void rmlui_leaderboard_fetch_page(int page) {
    if (SDL_GetAtomicInt(&s_fetch_active))
        return;
    if (!LobbyServer_IsConfigured())
        return;

    s_loading = true;
    if (s_model_handle)
        s_model_handle.DirtyVariable("loading");

    SDL_SetAtomicInt(&s_fetch_active, 1);
    SDL_SetAtomicInt(&s_fetch_done, 0);

    int* p = (int*)malloc(sizeof(int));
    *p = page;
    SDL_Thread* t = SDL_CreateThread(async_fetch_fn, "AsyncLeaderboard", p);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(p);
        SDL_SetAtomicInt(&s_fetch_active, 0);
        s_loading = false;
    }
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_leaderboard_show(void) {
    rmlui_wrapper_show_game_document("leaderboard");
    // Auto-fetch page 0 on show
    rmlui_leaderboard_fetch_page(0);
}

extern "C" void rmlui_leaderboard_hide(void) {
    rmlui_wrapper_hide_game_document("leaderboard");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_leaderboard_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("leaderboard");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("leaderboard");
        s_model_registered = false;
    }
    s_entries.clear();
}
