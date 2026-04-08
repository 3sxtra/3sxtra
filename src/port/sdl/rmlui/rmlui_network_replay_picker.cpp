/**
 * @file rmlui_network_replay_picker.cpp
 * @brief RmlUi data model for the online replay picker.
 *
 * Full-screen overlay fetching replay metadata from the lobby server,
 * displaying a paginated list, and downloading selected replays for
 * playback.  Follows the patterns in rmlui_replay_picker.cpp and
 * rmlui_leaderboard.cpp.
 *
 * Input is read via PLsw (legacy pad state) — same method as the
 * local replay picker.
 */

#include "port/sdl/rmlui/rmlui_network_replay_picker.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/lobby_server.h"
#include "port/save/native_save.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/* PLsw: legacy pad state for input polling */
extern u16 plsw_00[2];
extern u16 plsw_01[2];
}

/* ─── Constants ──────────────────────────────────────────────── */

#define NRP_SLOTS_PER_PAGE 10

/* Input bit masks — must match SWKey in sf33rd/AcrSDK/common/pad.h */
#define NRP_SWK_UP 0x0001    /* SWK_UP     = 1 << 0 */
#define NRP_SWK_DOWN 0x0002  /* SWK_DOWN   = 1 << 1 */
#define NRP_SWK_LEFT 0x0004  /* SWK_LEFT   = 1 << 2 */
#define NRP_SWK_RIGHT 0x0008 /* SWK_RIGHT  = 1 << 3 */
#define NRP_SWK_SOUTH 0x0100 /* SWK_SOUTH  = 1 << 8, confirm (LP) */
#define NRP_SWK_EAST 0x0200  /* SWK_EAST   = 1 << 9, cancel  (MK) */

/* ─── Slot entry ─────────────────────────────────────────────── */

struct NrpSlotEntry {
    int match_id;
    int index; // 0-based index on current page
    Rml::String p1_name;
    Rml::String p1_country;
    Rml::String p2_name;
    Rml::String p2_country;
    Rml::String p1_char_name;
    Rml::String p2_char_name;
    Rml::String winner_id;
    Rml::String date_str;
    Rml::String display_num;
    bool is_valid;

    bool operator==(const NrpSlotEntry& o) const {
        return match_id == o.match_id && index == o.index && p1_name == o.p1_name && p2_name == o.p2_name &&
               p1_country == o.p1_country && p2_country == o.p2_country && p1_char_name == o.p1_char_name &&
               p2_char_name == o.p2_char_name && winner_id == o.winner_id && date_str == o.date_str &&
               display_num == o.display_num && is_valid == o.is_valid;
    }
    bool operator!=(const NrpSlotEntry& o) const {
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

/* ─── State ──────────────────────────────────────────────────── */

static Rml::DataModelHandle s_model;
static bool s_model_registered = false;

static bool s_open = false;
static bool s_loading = false;
static bool s_downloading = false;
static Rml::String s_error;

static int s_cursor = 0; // selected row index on current page
static int s_page = 0;   // 0-indexed
static int s_total_replays = 0;
static int s_zone = 0; // 0=table, 1=page controls

static std::vector<NrpSlotEntry> s_slots;

/* Async fetch state */
static SDL_AtomicInt s_fetch_active = { 0 };
static SDL_AtomicInt s_fetch_done = { 0 };
static ReplayListEntry s_fetch_buf[NRP_SLOTS_PER_PAGE];
static int s_fetch_count = 0;
static int s_fetch_total = 0;
static int s_fetch_error = 0; /* 1 = network/request error */

/* Async download state */
static SDL_AtomicInt s_download_active = { 0 };
static SDL_AtomicInt s_download_done = { 0 };
static SDL_AtomicInt s_download_ok = { 0 };
static void* s_download_data = nullptr;
static size_t s_download_size = 0;

/* Poll result: 1=pending, 0=selected, -1=cancel */
static int s_result = 1;

/* ─── Async threads ──────────────────────────────────────────── */

static int SDLCALL async_fetch_fn(void* data) {
    int page = *(int*)data;
    free(data);

    int total = 0;
    ReplayListEntry entries[NRP_SLOTS_PER_PAGE];
    int count = LobbyServer_ListReplays(entries, NRP_SLOTS_PER_PAGE, page, &total);

    if (count < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[NetworkReplayPicker] Fetch failed (page %d)", page);
        s_fetch_error = 1;
        count = 0;
    } else {
        SDL_Log("[NetworkReplayPicker] Fetched %d replays (total=%d, page=%d)", count, total, page);
        s_fetch_error = 0;
    }

    memcpy(s_fetch_buf, entries, sizeof(entries));
    s_fetch_count = count;
    s_fetch_total = total;
    SDL_SetAtomicInt(&s_fetch_done, 1);
    SDL_SetAtomicInt(&s_fetch_active, 0);
    return 0;
}

static void AsyncFetchPage(int page) {
    if (SDL_GetAtomicInt(&s_fetch_active) != 0)
        return;
    SDL_SetAtomicInt(&s_fetch_active, 1);
    SDL_SetAtomicInt(&s_fetch_done, 0);

    int* arg = (int*)malloc(sizeof(int));
    *arg = page;
    SDL_Thread* t = SDL_CreateThread(async_fetch_fn, "AsyncFetchReplays", arg);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(arg);
        SDL_SetAtomicInt(&s_fetch_active, 0);
    }
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

static void AsyncDownloadReplay(int match_id) {
    if (SDL_GetAtomicInt(&s_download_active) != 0)
        return;
    SDL_SetAtomicInt(&s_download_active, 1);
    SDL_SetAtomicInt(&s_download_done, 0);
    SDL_SetAtomicInt(&s_download_ok, 0);

    DownloadArg* arg = (DownloadArg*)calloc(1, sizeof(DownloadArg));
    arg->match_id = match_id;
    SDL_Thread* t = SDL_CreateThread(async_download_fn, "AsyncDownloadReplay", arg);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(arg);
        SDL_SetAtomicInt(&s_download_active, 0);
    }
}

/* ─── Data model init ────────────────────────────────────────── */

static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("network_replay_picker");
    if (!ctor)
        return;

    /* Register slot struct */
    if (auto h = ctor.RegisterStruct<NrpSlotEntry>()) {
        h.RegisterMember("match_id", &NrpSlotEntry::match_id);
        h.RegisterMember("index", &NrpSlotEntry::index);
        h.RegisterMember("p1_name", &NrpSlotEntry::p1_name);
        h.RegisterMember("p1_country", &NrpSlotEntry::p1_country);
        h.RegisterMember("p2_name", &NrpSlotEntry::p2_name);
        h.RegisterMember("p2_country", &NrpSlotEntry::p2_country);
        h.RegisterMember("p1_char_name", &NrpSlotEntry::p1_char_name);
        h.RegisterMember("p2_char_name", &NrpSlotEntry::p2_char_name);
        h.RegisterMember("winner_id", &NrpSlotEntry::winner_id);
        h.RegisterMember("date_str", &NrpSlotEntry::date_str);
        h.RegisterMember("display_num", &NrpSlotEntry::display_num);
        h.RegisterMember("is_valid", &NrpSlotEntry::is_valid);
    }
    ctor.RegisterArray<std::vector<NrpSlotEntry>>();

    /* Bind data */
    ctor.Bind("nrp_open", &s_open);
    ctor.Bind("nrp_loading", &s_loading);
    ctor.Bind("nrp_downloading", &s_downloading);
    ctor.Bind("nrp_error", &s_error);
    ctor.Bind("nrp_slots", &s_slots);
    ctor.Bind("nrp_cursor", &s_cursor);
    ctor.Bind("nrp_zone", &s_zone);

    ctor.BindFunc("nrp_slot_count", [](Rml::Variant& v) { v = (int)s_slots.size(); });
    ctor.BindFunc("nrp_page", [](Rml::Variant& v) { v = s_page + 1; });
    ctor.BindFunc("nrp_total_pages", [](Rml::Variant& v) {
        int pages = (s_total_replays + NRP_SLOTS_PER_PAGE - 1) / NRP_SLOTS_PER_PAGE;
        if (pages < 1)
            pages = 1;
        v = pages;
    });
    ctor.BindFunc("nrp_has_prev", [](Rml::Variant& v) { v = (s_page > 0); });
    ctor.BindFunc("nrp_has_next", [](Rml::Variant& v) {
        int pages = (s_total_replays + NRP_SLOTS_PER_PAGE - 1) / NRP_SLOTS_PER_PAGE;
        v = (s_page + 1 < pages);
    });

    s_model = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi NetworkReplayPicker] Data model registered");
}

/* ─── Rebuild slot list from fetch buffer ────────────────────── */

static void rebuild_slots(void) {
    std::vector<NrpSlotEntry> next;
    next.reserve((size_t)s_fetch_count);

    for (int i = 0; i < s_fetch_count; i++) {
        NrpSlotEntry e;
        e.match_id = s_fetch_buf[i].match_id;
        e.index = i;
        e.p1_name = Rml::String(s_fetch_buf[i].p1_name);
        e.p1_country = Rml::String(s_fetch_buf[i].p1_country);
        e.p2_name = Rml::String(s_fetch_buf[i].p2_name);
        e.p2_country = Rml::String(s_fetch_buf[i].p2_country);
        e.p1_char_name = Rml::String(char_name(s_fetch_buf[i].p1_char));
        e.p2_char_name = Rml::String(char_name(s_fetch_buf[i].p2_char));

        if (strcmp(s_fetch_buf[i].winner_id, s_fetch_buf[i].p1_id) == 0) {
            e.winner_id = Rml::String(s_fetch_buf[i].p1_name);
        } else if (strcmp(s_fetch_buf[i].winner_id, s_fetch_buf[i].p2_id) == 0) {
            e.winner_id = Rml::String(s_fetch_buf[i].p2_name);
        } else {
            e.winner_id = "";
        }

        e.date_str = Rml::String(s_fetch_buf[i].date);

        char num[8];
        SDL_snprintf(num, sizeof(num), "%d", s_page * NRP_SLOTS_PER_PAGE + i + 1);
        e.display_num = Rml::String(num);
        e.is_valid = true;
        next.push_back(e);
    }

    // Pad the array to avoid RmlUi out-of-bounds evaluation bug on shrunk data-for loops
    while (next.size() < NRP_SLOTS_PER_PAGE) {
        NrpSlotEntry pad = {};
        pad.is_valid = false;
        pad.match_id = -1;
        pad.index = (int)next.size();
        pad.p1_name = Rml::String();
        pad.p1_country = Rml::String();
        pad.p2_name = Rml::String();
        pad.p2_country = Rml::String();
        pad.p1_char_name = Rml::String();
        pad.p2_char_name = Rml::String();
        pad.winner_id = Rml::String();
        pad.date_str = Rml::String();
        pad.display_num = Rml::String();
        next.push_back(pad);
    }

    if (next != s_slots) {
        s_slots = std::move(next);
        s_model.DirtyVariable("nrp_slots");
        s_model.DirtyVariable("nrp_slot_count");
    }
}

/* ─── Public API ─────────────────────────────────────────────── */

extern "C" void rmlui_network_replay_picker_show(void) {
    if (!s_model_registered) {
        do_init();
        if (!s_model_registered)
            return;
    }

    rmlui_wrapper_show_game_document("network_replay_picker");
    s_open = true;
    s_loading = true;
    s_downloading = false;
    s_error = "";
    s_cursor = 0;
    s_page = 0;
    s_zone = 0;
    s_total_replays = 0;
    s_result = 1;
    s_slots.clear();

    if (s_model) {
        s_model.DirtyVariable("nrp_open");
        s_model.DirtyVariable("nrp_loading");
        s_model.DirtyVariable("nrp_downloading");
        s_model.DirtyVariable("nrp_error");
        s_model.DirtyVariable("nrp_cursor");
        s_model.DirtyVariable("nrp_zone");
        s_model.DirtyVariable("nrp_slots");
        s_model.DirtyVariable("nrp_slot_count");
        s_model.DirtyVariable("nrp_page");
        s_model.DirtyVariable("nrp_total_pages");
        s_model.DirtyVariable("nrp_has_prev");
        s_model.DirtyVariable("nrp_has_next");
    }

    /* Kick off first page fetch */
    AsyncFetchPage(0);
}

extern "C" void rmlui_network_replay_picker_hide(void) {
    s_open = false;
    if (s_model) {
        s_model.DirtyVariable("nrp_open");
    }
    rmlui_wrapper_hide_game_document("network_replay_picker");
}

extern "C" void rmlui_network_replay_picker_update(void) {
    if (!s_model_registered || !s_model || !s_open)
        return;

    /* ── Check async fetch completion ───────────────────────── */
    if (SDL_GetAtomicInt(&s_fetch_done)) {
        SDL_SetAtomicInt(&s_fetch_done, 0);
        s_total_replays = s_fetch_total;

        if (s_fetch_error) {
            s_error = "Failed to fetch replays";
        } else if (s_fetch_count == 0 && s_total_replays == 0) {
            s_error = "No replays available";
        } else {
            s_error = "";
        }
        s_loading = false;

        rebuild_slots();

        if (s_cursor >= (int)s_slots.size() && !s_slots.empty())
            s_cursor = (int)s_slots.size() - 1;
        if (s_cursor < 0)
            s_cursor = 0;

        s_model.DirtyVariable("nrp_loading");
        s_model.DirtyVariable("nrp_error");
        s_model.DirtyVariable("nrp_cursor");
        s_model.DirtyVariable("nrp_page");
        s_model.DirtyVariable("nrp_total_pages");
        s_model.DirtyVariable("nrp_has_prev");
        s_model.DirtyVariable("nrp_has_next");
    }

    /* ── Check async download completion ────────────────────── */
    if (SDL_GetAtomicInt(&s_download_done)) {
        SDL_SetAtomicInt(&s_download_done, 0);
        s_downloading = false;
        s_model.DirtyVariable("nrp_downloading");

        if (SDL_GetAtomicInt(&s_download_ok) && s_download_data && s_download_size > 0) {
            /* Inject replay data into the engine */
            /* The download format is: NativeReplayHeader + _REPLAY_W data
             * (same format as the upload in sdl_netplay_ui.cpp) */
            size_t header_size = 16; /* Default to v1 header size */
            if (s_download_size >= 8) {
                uint32_t version = *((uint32_t*)s_download_data + 1);
                if (version == 2) {
                    header_size = 24; /* v2 header adds 8 bytes of metadata */
                }
            }

            if (s_download_size > header_size) {
                size_t replay_data_size = s_download_size - header_size;
                if (replay_data_size <= sizeof(Replay_w)) {
                    memcpy(&Replay_w, (uint8_t*)s_download_data + header_size, replay_data_size);
                    s_result = 0; /* signal: ready to play */
                    SDL_Log("[NetworkReplayPicker] Replay injected into Replay_w (%zu bytes)", replay_data_size);
                } else {
                    s_error = "Replay data too large";
                    s_model.DirtyVariable("nrp_error");
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "[NetworkReplayPicker] Replay too large: %zu > %zu",
                                replay_data_size,
                                sizeof(Replay_w));
                }
            } else {
                s_error = "Invalid replay data";
                s_model.DirtyVariable("nrp_error");
            }
            free(s_download_data);
            s_download_data = nullptr;
        } else {
            s_error = "Download failed";
            s_model.DirtyVariable("nrp_error");
        }
    }
}

extern "C" int rmlui_network_replay_picker_poll(void) {
    if (!s_open)
        return -1;

    /* Don't process input during loading/downloading */
    if (s_loading || s_downloading)
        return 1;

    /* Read input from both players */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~plsw_01[i] & plsw_00[i]);
    }

    int slot_count = (int)s_slots.size();

    if (s_zone == 0) {
        /* ── Table zone ────────────────────────────────────────── */
        if (slot_count > 0) {
            if (trigger & NRP_SWK_UP) {
                s_cursor--;
                if (s_cursor < 0)
                    s_cursor = slot_count - 1;
                s_model.DirtyVariable("nrp_cursor");
                s_model.DirtyVariable("nrp_slots");
            }
            if (trigger & NRP_SWK_DOWN) {
                s_cursor++;
                if (s_cursor >= slot_count)
                    s_cursor = 0;
                s_model.DirtyVariable("nrp_cursor");
                s_model.DirtyVariable("nrp_slots");
            }
        }

        /* Move to page controls zone */
        if (trigger & NRP_SWK_DOWN && s_cursor == 0 && slot_count > 0) {
            /* Already wrapped — could switch zone instead */
        }

        /* Confirm — download the selected replay */
        if ((trigger & NRP_SWK_SOUTH) && slot_count > 0 && s_cursor < slot_count) {
            int match_id = s_slots[s_cursor].match_id;
            if (match_id >= 0) {
                s_downloading = true;
                s_error = "";
                s_model.DirtyVariable("nrp_downloading");
                s_model.DirtyVariable("nrp_error");
                AsyncDownloadReplay(match_id);
            }
        }

        /* L/R for page navigation (in table zone) */
        if (trigger & NRP_SWK_LEFT) {
            rmlui_network_replay_picker_prev_page();
        }
        if (trigger & NRP_SWK_RIGHT) {
            rmlui_network_replay_picker_next_page();
        }
    }

    /* Cancel — always available */
    if (trigger & NRP_SWK_EAST) {
        s_result = -1;
    }

    return s_result;
}

extern "C" void rmlui_network_replay_picker_prev_page(void) {
    if (s_page > 0 && !s_loading) {
        s_page--;
        s_loading = true;
        s_cursor = 0;
        s_error = "";
        s_model.DirtyVariable("nrp_loading");
        s_model.DirtyVariable("nrp_cursor");
        s_model.DirtyVariable("nrp_error");
        s_model.DirtyVariable("nrp_page");
        s_model.DirtyVariable("nrp_has_prev");
        s_model.DirtyVariable("nrp_has_next");
        AsyncFetchPage(s_page);
    }
}

extern "C" void rmlui_network_replay_picker_next_page(void) {
    int total_pages = (s_total_replays + NRP_SLOTS_PER_PAGE - 1) / NRP_SLOTS_PER_PAGE;
    if (s_page + 1 < total_pages && !s_loading) {
        s_page++;
        s_loading = true;
        s_cursor = 0;
        s_error = "";
        s_model.DirtyVariable("nrp_loading");
        s_model.DirtyVariable("nrp_cursor");
        s_model.DirtyVariable("nrp_error");
        s_model.DirtyVariable("nrp_page");
        s_model.DirtyVariable("nrp_has_prev");
        s_model.DirtyVariable("nrp_has_next");
        AsyncFetchPage(s_page);
    }
}
