/**
 * @file rmlui_leaderboard.cpp
 * @brief RmlUi Leaderboard data model.
 *
 * Displays a paginated leaderboard fetched from the lobby server.
 * Data is fetched asynchronously via SDL threads.
 *
 * Columns: Rank | Flag | Player | Char | W/M | Win% | Grade | Rating | DC%
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

// ─── Character name table (index 0-19 → 3-letter abbreviation) ──
static const char* s_char_names[20] = {
    "ALX", // 0  Alex
    "RYU", // 1  Ryu
    "YUN", // 2  Yun
    "DUD", // 3  Dudley
    "NEC", // 4  Necro
    "HUG", // 5  Hugo
    "IBK", // 6  Ibuki
    "ELN", // 7  Elena
    "ORO", // 8  Oro
    "YNG", // 9  Yang
    "KEN", // 10 Ken
    "SHN", // 11 Sean
    "URI", // 12 Urien
    "GIL", // 13 Gill
    "CHN", // 14 Chun-Li
    "MAK", // 15 Makoto
    "Q__", // 16 Q
    "TWL", // 17 Twelve
    "TRE", // 18 Treize (unused palette)
    "RMY", // 19 Remy
};

// ─── Grade label table (grade 0-11 → display string) ────────────
static const char* s_grade_labels[12] = { "F", "E", "E+", "D", "D+", "C", "C+", "B", "B+", "A", "A+", "S" };

// ─── Leaderboard entry struct for data-for ──────────────────────

struct LBItem {
    int rank;
    Rml::String rank_str; // "🥇"/"🥈"/"🥉" for top 3, number otherwise
    Rml::String name;
    Rml::String flag_icon;    // path to flag PNG
    Rml::String char_name;    // 3-letter character abbreviation
    Rml::String wins_matches; // "W/M" format (e.g. "45/120")
    int win_pct;
    Rml::String grade_str; // letter grade
    int dc_pct;
    Rml::String rating_str;
    Rml::String tier;
    bool is_me;
    bool is_top3; // rank 1-3 for medal styling
    bool is_valid;

    bool operator==(const LBItem& o) const {
        return rank == o.rank && rank_str == o.rank_str && name == o.name && flag_icon == o.flag_icon &&
               char_name == o.char_name && wins_matches == o.wins_matches && win_pct == o.win_pct &&
               grade_str == o.grade_str && dc_pct == o.dc_pct && rating_str == o.rating_str && tier == o.tier &&
               is_me == o.is_me && is_top3 == o.is_top3 && is_valid == o.is_valid;
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

    SDL_MemoryBarrierRelease(); /* ensure s_fetch_result writes visible before flag */
    SDL_SetAtomicInt(&s_fetch_done, 1);
    SDL_SetAtomicInt(&s_fetch_active, 0);
    return 0;
}

// ─── Lazy Init (deferred from boot to first use) ────────────────
static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("leaderboard");
    if (!ctor)
        return;

    // Register LBItem struct
    if (auto h = ctor.RegisterStruct<LBItem>()) {
        h.RegisterMember("rank", &LBItem::rank);
        h.RegisterMember("rank_str", &LBItem::rank_str);
        h.RegisterMember("name", &LBItem::name);
        h.RegisterMember("flag_icon", &LBItem::flag_icon);
        h.RegisterMember("char_name", &LBItem::char_name);
        h.RegisterMember("wins_matches", &LBItem::wins_matches);
        h.RegisterMember("win_pct", &LBItem::win_pct);
        h.RegisterMember("grade_str", &LBItem::grade_str);
        h.RegisterMember("dc_pct", &LBItem::dc_pct);
        h.RegisterMember("rating_str", &LBItem::rating_str);
        h.RegisterMember("tier", &LBItem::tier);
        h.RegisterMember("is_me", &LBItem::is_me);
        h.RegisterMember("is_top3", &LBItem::is_top3);
        h.RegisterMember("is_valid", &LBItem::is_valid);
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
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi Leaderboard] Data model registered (lazy)");
}

extern "C" void rmlui_leaderboard_init(void) { do_init(); }

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_leaderboard_update(void) {
    if (!s_model_registered) { do_init(); if (!s_model_registered) return; }
    if (!s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("leaderboard"))
        return;

    // Check if async fetch completed
    if (SDL_GetAtomicInt(&s_fetch_done)) {
        SDL_MemoryBarrierAcquire(); /* ensure s_fetch_result reads see writer's stores */
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
                item.is_top3 = (e->rank >= 1 && e->rank <= 3);

                // Rank display for top 3 with simple markers, numeric otherwise
                if (e->rank == 1)
                    item.rank_str = Rml::String("#1");
                else if (e->rank == 2)
                    item.rank_str = Rml::String("#2");
                else if (e->rank == 3)
                    item.rank_str = Rml::String("#3");
                else {
                    char buf[8];
                    SDL_snprintf(buf, sizeof(buf), "%d", e->rank);
                    item.rank_str = Rml::String(buf);
                }

                item.name = Rml::String(e->display_name[0] ? e->display_name : e->player_id);

                // Country flag icon
                if (e->country[0] && e->country[1]) {
                    char lower[3] = { (char)tolower((unsigned char)e->country[0]),
                                      (char)tolower((unsigned char)e->country[1]),
                                      0 };
                    char path[64];
                    SDL_snprintf(path, sizeof(path), "../flags_icons/%s.png", lower);
                    item.flag_icon = Rml::String(path);
                }

                // Most-played character
                if (e->most_played_char >= 0 && e->most_played_char < 20) {
                    item.char_name = Rml::String(s_char_names[e->most_played_char]);
                } else {
                    item.char_name = Rml::String("\xe2\x80\x94"); // em-dash
                }

                // Wins / total matches
                int total_matches = e->wins + e->losses + e->disconnects;
                {
                    char buf[24];
                    SDL_snprintf(buf, sizeof(buf), "%d/%d", e->wins, total_matches);
                    item.wins_matches = Rml::String(buf);
                }
                item.win_pct = total_matches > 0 ? (e->wins * 100 / total_matches) : 0;
                item.dc_pct = total_matches > 0 ? (e->disconnects * 100 / total_matches) : 0;

                // Grade
                if (e->grade >= 0 && e->grade <= 11) {
                    item.grade_str = Rml::String(s_grade_labels[e->grade]);
                } else {
                    item.grade_str = Rml::String("\xe2\x80\x94"); // em-dash
                }

                // Rating
                {
                    char buf[16];
                    SDL_snprintf(buf, sizeof(buf), "%.0f", (double)e->rating);
                    item.rating_str = Rml::String(buf);
                }
                item.tier = Rml::String(e->tier);
                item.is_me = (my_id && my_id[0] && strcmp(my_id, e->player_id) == 0);
                item.is_valid = true;
                next.push_back(item);
            }
            s_has_data = true;
        }

        // Pad the array to avoid RmlUi out-of-bounds evaluation bug on shrunk data-for loops
        while (next.size() < LB_PAGE_SIZE) {
            LBItem pad = {};
            pad.is_valid = false;
            pad.rank_str = Rml::String();
            pad.name = Rml::String();
            pad.flag_icon = Rml::String();
            pad.char_name = Rml::String();
            pad.wins_matches = Rml::String();
            pad.grade_str = Rml::String();
            pad.rating_str = Rml::String();
            pad.tier = Rml::String();
            next.push_back(pad);
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

// ─── Page navigation (called from ms_leaderboard.c) ──────────────
extern "C" void rmlui_leaderboard_next_page(void) {
    if ((s_page + 1) * LB_PAGE_SIZE < s_total) {
        rmlui_leaderboard_fetch_page(s_page + 1);
    }
}

extern "C" void rmlui_leaderboard_prev_page(void) {
    if (s_page > 0) {
        rmlui_leaderboard_fetch_page(s_page - 1);
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
