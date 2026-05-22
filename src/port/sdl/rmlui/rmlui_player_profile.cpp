/**
 * @file rmlui_player_profile.cpp
 * @brief RmlUi Player Profile data model.
 */

#include "port/sdl/rmlui/rmlui_player_profile.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/engine/state_user.h" /* SWK constants */
} // extern "C"

// ─── Match history struct for data-for ──────────────────────────

struct ProfileMatchItem {
    int match_id;
    Rml::String result_str; // "WIN" or "LOSS"
    bool is_win;
    Rml::String player_char_name;
    Rml::String opponent_char_name;
    Rml::String opponent_name;
    Rml::String opponent_country;
    Rml::String date_str;
    Rml::String replay_status; // "Saved", "Unavailable"
    bool selected;
    bool is_valid;

    bool operator==(const ProfileMatchItem& o) const {
        return result_str == o.result_str && is_win == o.is_win && player_char_name == o.player_char_name &&
               opponent_char_name == o.opponent_char_name && opponent_name == o.opponent_name &&
               opponent_country == o.opponent_country && date_str == o.date_str && replay_status == o.replay_status &&
               selected == o.selected && is_valid == o.is_valid;
    }
    bool operator!=(const ProfileMatchItem& o) const {
        return !(*this == o);
    }
};

// ─── Character name table (SF3:3S roster) ───────────────────────
static const char* const s_char_names[21] = { "GILL",  "ALEX",    "RYU",    "YUN",  "DUDLEY", "NECRO", "HUGO",
                                              "IBUKI", "ELENA",   "ORO",    "YANG", "KEN",    "SEAN",  "URIEN",
                                              "GOUKI", "CHUN-LI", "MAKOTO", "Q",    "TWELVE", "REMY",  "AKUMA" };
static const char* char_name(int idx) {
    if (idx >= 0 && idx < 21)
        return s_char_names[idx];
    return "???";
}

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

static std::vector<ProfileMatchItem> s_match_history;
static int s_match_history_count = 0;

static Rml::String s_avatar_url;
static Rml::String s_player_name;
static Rml::String s_player_title;
static Rml::String s_tier;
static Rml::String s_rating;
static Rml::String s_global_rank;

static int s_wins = 0;
static int s_losses = 0;
static Rml::String s_win_rate;
static int s_max_streak = 0;
static int s_disconnects = 0;
static bool s_disconnects_bad = false;

static Rml::String s_network_status;
static bool s_status_bad = false;
static bool s_status_good = false;

static Rml::String s_status_text;

// Async fetch state
static SDL_AtomicInt s_fetch_active = { 0 };
static SDL_AtomicInt s_fetch_done = { 0 };

static PlayerStats s_fetch_stats;
static bool s_fetch_success = false;

// Async download state
static SDL_AtomicInt s_download_active = { 0 };
static SDL_AtomicInt s_download_done = { 0 };
static SDL_AtomicInt s_download_ok = { 0 };
static void* s_download_data = nullptr;
static size_t s_download_size = 0;
static bool s_downloading = false;

static int s_cursor = 0;
static int s_result = 1;

// ─── Dummy / Mock Data for Matches ──────────────────────────────
static std::vector<ReplayListEntry> s_fetch_matches;

// ─── Async Fetch ────────────────────────────────────────────────
static int async_fetch_profile_fn(void* userdata) {
    (void)userdata;
    const char* my_id = Identity_GetPlayerId();

    if (my_id && my_id[0]) {
        s_fetch_success = LobbyServer_GetPlayerStats(my_id, &s_fetch_stats);

        s_fetch_matches.clear();
        ReplayListEntry entries[10];
        int count = LobbyServer_GetPlayerMatchHistory(my_id, entries, 10, 0, NULL);
        if (count > 0) {
            for (int i = 0; i < count; i++) {
                s_fetch_matches.push_back(entries[i]);
            }
        }
    } else {
        s_fetch_success = false;
    }

    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&s_fetch_done, 1);
    SDL_SetAtomicInt(&s_fetch_active, 0);
    return 0;
}

struct DownloadArg {
    int match_id;
};

static int SDLCALL async_download_fn(void* data) {
    DownloadArg* arg = (DownloadArg*)data;
    void* replay_data = nullptr;
    size_t sz = LobbyServer_DownloadReplay(arg->match_id, &replay_data);
    free(arg);

    if (sz > 0 && replay_data) {
        s_download_data = replay_data;
        s_download_size = sz;
        SDL_SetAtomicInt(&s_download_ok, 1);
    } else {
        s_download_data = nullptr;
        s_download_size = 0;
        SDL_SetAtomicInt(&s_download_ok, 0);
        free(replay_data);
    }
    SDL_SetAtomicInt(&s_download_done, 1);
    SDL_SetAtomicInt(&s_download_active, 0);
    return 0;
}

static void AsyncDownloadProfileReplay(int match_id) {
    if (SDL_GetAtomicInt(&s_download_active) != 0)
        return;
    SDL_SetAtomicInt(&s_download_active, 1);
    SDL_SetAtomicInt(&s_download_done, 0);
    SDL_SetAtomicInt(&s_download_ok, 0);

    DownloadArg* arg = (DownloadArg*)calloc(1, sizeof(DownloadArg));
    if (!arg) {
        SDL_SetAtomicInt(&s_download_active, 0);
        return;
    }
    arg->match_id = match_id;
    SDL_Thread* t = SDL_CreateThread(async_download_fn, "AsyncDownloadReplay", arg);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(arg);
        SDL_SetAtomicInt(&s_download_active, 0);
    }
}

// ─── Lazy Init ──────────────────────────────────────────────────
static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("player_profile");
    if (!ctor)
        return;

    // Register ProfileMatchItem struct
    if (auto h = ctor.RegisterStruct<ProfileMatchItem>()) {
        h.RegisterMember("result_str", &ProfileMatchItem::result_str);
        h.RegisterMember("is_win", &ProfileMatchItem::is_win);
        h.RegisterMember("player_char_name", &ProfileMatchItem::player_char_name);
        h.RegisterMember("opponent_char_name", &ProfileMatchItem::opponent_char_name);
        h.RegisterMember("opponent_name", &ProfileMatchItem::opponent_name);
        h.RegisterMember("opponent_country", &ProfileMatchItem::opponent_country);
        h.RegisterMember("date_str", &ProfileMatchItem::date_str);
        h.RegisterMember("replay_status", &ProfileMatchItem::replay_status);
        h.RegisterMember("selected", &ProfileMatchItem::selected);
        h.RegisterMember("is_valid", &ProfileMatchItem::is_valid);
    }
    ctor.RegisterArray<std::vector<ProfileMatchItem>>();
    ctor.Bind("match_history", &s_match_history);

    ctor.Bind("match_history_count", &s_match_history_count);

    ctor.Bind("avatar_url", &s_avatar_url);
    ctor.Bind("player_name", &s_player_name);
    ctor.Bind("player_title", &s_player_title);
    ctor.Bind("tier", &s_tier);
    ctor.Bind("rating", &s_rating);
    ctor.Bind("global_rank", &s_global_rank);

    ctor.Bind("wins", &s_wins);
    ctor.Bind("losses", &s_losses);
    ctor.Bind("win_rate", &s_win_rate);
    ctor.Bind("max_streak", &s_max_streak);

    ctor.Bind("disconnects", &s_disconnects);
    ctor.Bind("disconnects_bad", &s_disconnects_bad);

    ctor.Bind("network_status", &s_network_status);
    ctor.Bind("status_bad", &s_status_bad);
    ctor.Bind("status_good", &s_status_good);

    ctor.Bind("status_text", &s_status_text);

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi Player Profile] Data model registered");
}

extern "C" void rmlui_player_profile_init(void) {
    do_init();
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_player_profile_update(void) {
    if (!s_model_registered) {
        do_init();
        if (!s_model_registered)
            return;
    }
    if (!s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("player_profile"))
        return;

    // Check if async fetch completed
    if (SDL_GetAtomicInt(&s_fetch_done)) {
        SDL_MemoryBarrierAcquire();
        SDL_SetAtomicInt(&s_fetch_done, 0);

        if (s_fetch_success) {
            s_tier = Rml::String(s_fetch_stats.tier[0] ? s_fetch_stats.tier : "Unranked");
            char buf[16];
            SDL_snprintf(buf, sizeof(buf), "%.0f", (double)s_fetch_stats.rating);
            s_rating = Rml::String(buf);

            s_wins = s_fetch_stats.wins;
            s_losses = s_fetch_stats.losses;

            int total = s_wins + s_losses;
            if (total > 0) {
                SDL_snprintf(buf, sizeof(buf), "%d%%", (s_wins * 100) / total);
            } else {
                SDL_snprintf(buf, sizeof(buf), "0%%");
            }
            s_win_rate = Rml::String(buf);

            // For now map server disconnects to percentage manually (needs server updates to track properly)
            // Just displaying raw disconnect count or estimating rate
            int dc_rate = 0;
            if (total + s_fetch_stats.disconnects > 0) {
                dc_rate = (s_fetch_stats.disconnects * 100) / (total + s_fetch_stats.disconnects);
            }
            s_disconnects = dc_rate;
            s_disconnects_bad = dc_rate > 5;

            if (dc_rate > 5) {
                s_network_status = "WARNING";
                s_status_bad = true;
                s_status_good = false;
            } else {
                s_network_status = "GOOD";
                s_status_bad = false;
                s_status_good = true;
            }
        } else {
            s_tier = "---";
            s_rating = "---";
            s_wins = 0;
            s_losses = 0;
            s_win_rate = "---";
            s_disconnects = 0;
            s_network_status = "UNAVAILABLE";
            s_status_bad = false;
            s_status_good = true;
        }

        // Convert fetched matches to UI model
        s_match_history.clear();
        const char* my_id = Identity_GetPlayerId();
        for (const auto& m : s_fetch_matches) {
            ProfileMatchItem item;
            item.match_id = m.match_id;
            item.selected = false;

            bool im_p1 = (strcmp(m.p1_name, Identity_GetDisplayName()) == 0);
            item.opponent_name = im_p1 ? m.p2_name : m.p1_name;
            item.opponent_country = im_p1 ? m.p2_country : m.p1_country;
            item.player_char_name = char_name(im_p1 ? m.p1_char : m.p2_char);
            item.opponent_char_name = char_name(im_p1 ? m.p2_char : m.p1_char);
            item.date_str = m.date;

            item.is_win = (strcmp(m.winner_id, my_id) == 0);
            item.result_str = item.is_win ? "WIN" : "LOSS";

            item.replay_status = "Saved"; // Assume saved if on server for now
            item.is_valid = true;

            s_match_history.push_back(item);
        }
        s_match_history_count = (int)s_fetch_matches.size();

        while (s_match_history.size() < 10) {
            ProfileMatchItem pad = {};
            pad.is_valid = false;
            pad.result_str = Rml::String();
            pad.player_char_name = Rml::String();
            pad.opponent_char_name = Rml::String();
            pad.opponent_name = Rml::String();
            pad.opponent_country = Rml::String();
            pad.date_str = Rml::String();
            pad.replay_status = Rml::String();
            s_match_history.push_back(pad);
        }

        s_model_handle.DirtyVariable("match_history");
        s_model_handle.DirtyVariable("match_history_count");
        s_fetch_matches.clear(); // Free memory

        s_model_handle.DirtyVariable("tier");
        s_model_handle.DirtyVariable("rating");
        s_model_handle.DirtyVariable("wins");
        s_model_handle.DirtyVariable("losses");
        s_model_handle.DirtyVariable("win_rate");
        s_model_handle.DirtyVariable("disconnects");
        s_model_handle.DirtyVariable("disconnects_bad");
        s_model_handle.DirtyVariable("network_status");
        s_model_handle.DirtyVariable("status_bad");
        s_model_handle.DirtyVariable("status_good");

        s_status_text = "Updated from Server";
        s_model_handle.DirtyVariable("status_text");
    }

    if (SDL_GetAtomicInt(&s_download_done)) {
        SDL_MemoryBarrierAcquire();
        SDL_SetAtomicInt(&s_download_done, 0);
        s_downloading = false;

        if (SDL_GetAtomicInt(&s_download_ok) && s_download_data && s_download_size > 0) {
            size_t header_size = 16;
            if (s_download_size >= 8) {
                uint32_t version = *((uint32_t*)s_download_data + 1);
                if (version == 2)
                    header_size = 24;
            }

            if (s_download_size > header_size) {
                size_t replay_data_size = s_download_size - header_size;
                if (replay_data_size <= sizeof(Replay_w)) {
                    memcpy(&Replay_w, (uint8_t*)s_download_data + header_size, replay_data_size);
                    s_result = 0; // Trigger playback
                } else {
                    s_status_text = "Replay too large.";
                    s_model_handle.DirtyVariable("status_text");
                }
            } else {
                s_status_text = "Invalid replay data.";
                s_model_handle.DirtyVariable("status_text");
            }
            free(s_download_data);
            s_download_data = nullptr;
        } else {
            s_status_text = "Download failed.";
            s_model_handle.DirtyVariable("status_text");
        }
    }
}

// ─── Fetch ───────────────────────────────────────────────────────
extern "C" void rmlui_player_profile_fetch(void) {
    if (SDL_GetAtomicInt(&s_fetch_active))
        return;

    s_player_name = Identity_GetDisplayName()[0] ? Identity_GetDisplayName() : "Player 1";
    s_player_title = "Street Fighter"; // Dummy Title
    s_avatar_url = "";                 // No avatar system yet
    s_global_rank = "---";             // Require another API call later
    s_max_streak = 0;                  // Not returned from simple stats yet

    s_match_history.clear(); // Clear existing array while loading
    while (s_match_history.size() < 10) {
        ProfileMatchItem pad = {};
        pad.is_valid = false;
        pad.result_str = Rml::String();
        pad.player_char_name = Rml::String();
        pad.opponent_char_name = Rml::String();
        pad.opponent_name = Rml::String();
        pad.opponent_country = Rml::String();
        pad.date_str = Rml::String();
        pad.replay_status = Rml::String();
        s_match_history.push_back(pad);
    }
    s_match_history_count = 0;

    s_cursor = 0;
    s_result = 1;

    if (s_model_handle) {
        s_model_handle.DirtyVariable("player_name");
        s_model_handle.DirtyVariable("player_title");
        s_model_handle.DirtyVariable("global_rank");
        s_model_handle.DirtyVariable("max_streak");
        s_model_handle.DirtyVariable("match_history");
        s_status_text = "Fetching profile...";
        s_model_handle.DirtyVariable("status_text");
    }

    if (!LobbyServer_IsConfigured()) {
        s_status_text = "Not Connected";
        if (s_model_handle)
            s_model_handle.DirtyVariable("status_text");
        return;
    }

    SDL_SetAtomicInt(&s_fetch_active, 1);
    SDL_SetAtomicInt(&s_fetch_done, 0);

    SDL_Thread* t = SDL_CreateThread(async_fetch_profile_fn, "AsyncProfile", NULL);
    if (t) {
        SDL_DetachThread(t);
    } else {
        SDL_SetAtomicInt(&s_fetch_active, 0);
    }
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_player_profile_show(void) {
    if (!s_model_registered)
        rmlui_player_profile_init();
    rmlui_wrapper_show_game_document("player_profile");
    rmlui_player_profile_fetch();
}

extern "C" void rmlui_player_profile_hide(void) {
    rmlui_wrapper_hide_game_document("player_profile");
}

extern "C" int rmlui_player_profile_poll(unsigned short trigger) {
    if (!s_model_registered || !s_model_handle || s_downloading)
        return s_result;

    if (SDL_GetAtomicInt(&s_fetch_active)) {
        if (trigger & 0x0200)
            s_result = -1;
        return s_result;
    }

    int count = s_match_history_count;
    if (count > 0) {
        int old_cursor = s_cursor;
        if (trigger & 0x0010) { // UP
            s_cursor--;
            if (s_cursor < 0)
                s_cursor = count - 1;
        }
        if (trigger & 0x0020) { // DOWN
            s_cursor++;
            if (s_cursor >= count)
                s_cursor = 0;
        }

        if (old_cursor != s_cursor) {
            for (int i = 0; i < count; i++) {
                s_match_history[i].selected = (i == s_cursor);
            }
            s_model_handle.DirtyVariable("match_history");
        }

        if (trigger & 0x0100) { // ACCEPT / A
            if (s_cursor >= 0 && s_cursor < count) {
                int match_id = s_match_history[s_cursor].match_id;
                if (match_id >= 0) {
                    s_downloading = true;
                    s_status_text = "Downloading...";
                    s_model_handle.DirtyVariable("status_text");
                    AsyncDownloadProfileReplay(match_id);
                }
            }
        }
    }

    if (trigger & 0x0200) { // CANCEL / B
        s_result = -1;
    }

    return s_result;
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_player_profile_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("player_profile");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("player_profile");
        s_model_registered = false;
    }
    s_match_history.clear();
}
