/**
 * @file rmlui_replay_picker.cpp
 * @brief RmlUi Replay Picker data model + interaction.
 *
 * Replaces the ImGui ReplayPicker_Open/Update/GetSelectedSlot flow
 * with an RmlUi overlay showing replay file list and confirmation.
 * Input handling (cursor, confirm, cancel, tabs, pagination) is done
 * here via PLsw polling; the .rml document just reflects the data model.
 *
 * Layout: leaderboard-style dark panel with LOCAL/NETPLAY tabs,
 * paginated rows (5 per page), and footer with controls.
 */

#include "port/sdl/rmlui/rmlui_replay_picker.h"
#include "port/save/native_save.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

extern "C" {
#include "sf33rd/Source/Game/effect/eff76.h" /* chkNameAkuma */
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
} // extern "C"

/* ── Character name table (SF3:3S roster, index matches My_char) ── */
static const char* const s_char_names[21] = { "GILL",  "ALEX",    "RYU",    "YUN",  "DUDLEY", "NECRO", "HUGO",
                                              "IBUKI", "ELENA",   "ORO",    "YANG", "KEN",    "SEAN",  "URIEN",
                                              "GOUKI", "CHUN-LI", "MAKOTO", "Q",    "TWELVE", "REMY",  "AKUMA" };
#define CHAR_NAME_COUNT 21

static const char* safe_char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}
static const char* char_name(int my_char_id) {
    return safe_char_name(my_char_id + chkNameAkuma(my_char_id, 6));
}

/* ── Constants ─────────────────────────────────────────────────── */
#define RP_PAGE_SIZE 10

/* ── Data model ────────────────────────────────────────────────── */
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

/* ── Slot info for data binding ────────────────────────────────── */
struct SlotEntry {
    Rml::String filename; /* string filename */
    int display_num;      /* display number within tab (1-10) */
    int page_idx;         /* index within current page (0..PAGE_SIZE-1) for cursor matching */
    bool exists;
    Rml::String p1_name;
    Rml::String p1_country;
    Rml::String p2_name;
    Rml::String p2_country;
    Rml::String p1_char_name;
    Rml::String p2_char_name;
    Rml::String winner_id;
    Rml::String date_str;
};

/* s_all_slots holds ALL slots for the current tab; s_slots holds the current page slice */
static std::vector<SlotEntry> s_all_slots;
static std::vector<SlotEntry> s_slots;

static int s_cursor = 0; /* index into s_slots (0..PAGE_SIZE-1) */
static int s_zone = 0;   /* 0=data rows, 1=page control, 2=tab control */
static int s_mode = 0;   /* 0=load, 1=save */
static int s_tab = 0;    /* 0=LOCAL, 1=NETPLAY */
static int s_page = 0;   /* 0-indexed page within current tab */
static bool s_open = false;
static int s_result = 1; /* 1=active, 0=done, -1=cancelled */
static char s_selected_filename[128] = "";

/* ── Cache for dirty detection ─────────────────────────────────── */
struct ReplayPickerCache {
    int cursor;
    int zone;
    int mode;
    int tab;
    int page;
    int slot_count;
};
static ReplayPickerCache s_cache = {};

/* ── Helpers ───────────────────────────────────────────────────── */

static int total_pages(void) {
    int count = (int)s_all_slots.size();
    return count > 0 ? ((count + RP_PAGE_SIZE - 1) / RP_PAGE_SIZE) : 1;
}

static void refresh_slot_data(void) {
    /* Build full slot list for current tab */
    s_all_slots.clear();
    int is_netplay = (s_tab == 1) ? 1 : 0;
    char filenames[1000][128];
    int count = NativeSave_FindAllReplays(filenames, 1000, is_netplay);

    for (int i = 0; i < count; i++) {
        const char* filename = filenames[i];
        SlotEntry entry;
        entry.filename = filename;
        entry.display_num = i + 1;
        entry.exists = true;

        _sub_info info;
        if (NativeSave_GetReplayInfo(filename, &info) == 0) {
            entry.p1_char_name = char_name(info.player[0]);
            entry.p2_char_name = char_name(info.player[1]);
            entry.p1_name = entry.p1_char_name;
            entry.p1_country = "";
            entry.p2_name = entry.p2_char_name;
            entry.p2_country = "";

            int winner = NativeSave_GetReplayWinner(filename);
            if (winner == 0) {
                entry.winner_id = entry.p1_name;
            } else if (winner == 1) {
                entry.winner_id = entry.p2_name;
            } else {
                entry.winner_id = "";
            }

            char buf[32];
            SDL_snprintf(buf,
                         sizeof(buf),
                         "%04d-%02d-%02d %02d:%02d",
                         info.date.year,
                         info.date.month,
                         info.date.day,
                         info.date.hour,
                         info.date.min);
            entry.date_str = buf;
        } else {
            entry.p1_name = "Unknown";
            entry.p1_country = "";
            entry.p2_name = "Unknown";
            entry.p2_country = "";
            entry.p1_char_name = "???";
            entry.p2_char_name = "???";
            entry.winner_id = "";
            entry.date_str = filename; /* Fallback to filename if metadata is broken */
        }
        s_all_slots.push_back(entry);
    }

    if (s_mode == 1 && s_tab == 0) {
        SlotEntry entry;
        entry.filename = ""; /* Will trigger auto-generation in save logic */
        entry.display_num = count + 1;
        entry.exists = false;
        entry.p1_name = "--- NEW";
        entry.p1_country = "";
        entry.p2_name = "SAVE ---";
        entry.p2_country = "";
        entry.p1_char_name = "";
        entry.p2_char_name = "";
        entry.winner_id = "";
        entry.date_str = "";
        s_all_slots.push_back(entry);
    }

    /* Slice to current page */
    s_slots.clear();
    int page_start = s_page * RP_PAGE_SIZE;
    int page_end = std::min(page_start + RP_PAGE_SIZE, (int)s_all_slots.size());

    // Fix s_page out of bounds if items were removed
    if (page_start >= (int)s_all_slots.size() && s_page > 0) {
        s_page--;
        page_start = s_page * RP_PAGE_SIZE;
        page_end = std::min(page_start + RP_PAGE_SIZE, (int)s_all_slots.size());
    }

    for (int i = page_start; i < page_end; i++) {
        SlotEntry entry = s_all_slots[i];
        entry.page_idx = i - page_start;
        s_slots.push_back(entry);
    }
}

static void dirty_all(void) {
    if (!s_model_handle)
        return;
    s_model_handle.DirtyVariable("rp_cursor");
    s_model_handle.DirtyVariable("rp_zone");
    s_model_handle.DirtyVariable("rp_mode");
    s_model_handle.DirtyVariable("rp_open");
    s_model_handle.DirtyVariable("rp_slots");
    s_model_handle.DirtyVariable("rp_tab");
    s_model_handle.DirtyVariable("rp_page");
    s_model_handle.DirtyVariable("rp_total_pages");
    s_model_handle.DirtyVariable("rp_has_prev");
    s_model_handle.DirtyVariable("rp_has_next");
    s_model_handle.DirtyVariable("rp_slot_count");
}

/* ── Lazy init (deferred from boot to first use) ─────────────── */
static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("replay_picker");
    if (!ctor)
        return;

    /* Register SlotEntry struct */
    if (auto sh = ctor.RegisterStruct<SlotEntry>()) {
        sh.RegisterMember("filename", &SlotEntry::filename);
        sh.RegisterMember("display_num", &SlotEntry::display_num);
        sh.RegisterMember("page_idx", &SlotEntry::page_idx);
        sh.RegisterMember("exists", &SlotEntry::exists);
        sh.RegisterMember("p1_name", &SlotEntry::p1_name);
        sh.RegisterMember("p1_country", &SlotEntry::p1_country);
        sh.RegisterMember("p2_name", &SlotEntry::p2_name);
        sh.RegisterMember("p2_country", &SlotEntry::p2_country);
        sh.RegisterMember("p1_char_name", &SlotEntry::p1_char_name);
        sh.RegisterMember("p2_char_name", &SlotEntry::p2_char_name);
        sh.RegisterMember("winner_id", &SlotEntry::winner_id);
        sh.RegisterMember("date_str", &SlotEntry::date_str);
    }
    ctor.RegisterArray<std::vector<SlotEntry>>();

    ctor.Bind("rp_slots", &s_slots);
    ctor.BindFunc("rp_cursor", [](Rml::Variant& v) { v = s_cursor; });
    ctor.BindFunc("rp_zone", [](Rml::Variant& v) { v = s_zone; });
    ctor.BindFunc("rp_mode", [](Rml::Variant& v) { v = s_mode; });
    ctor.BindFunc("rp_open", [](Rml::Variant& v) { v = s_open; });
    ctor.BindFunc("rp_tab", [](Rml::Variant& v) { v = s_tab; });
    ctor.BindFunc("rp_page", [](Rml::Variant& v) { v = s_page + 1; }); /* 1-indexed for display */
    ctor.BindFunc("rp_total_pages", [](Rml::Variant& v) { v = total_pages(); });
    ctor.BindFunc("rp_has_prev", [](Rml::Variant& v) { v = s_page > 0; });
    ctor.BindFunc("rp_has_next", [](Rml::Variant& v) { v = (s_page + 1) < total_pages(); });
    ctor.BindFunc("rp_slot_count", [](Rml::Variant& v) { v = (int)s_slots.size(); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi ReplayPicker] Data model registered (lazy)");
}

extern "C" void rmlui_replay_picker_init(void) {
    do_init();
}

/* ── Per-frame update (called from sdl_app.c render loop) ──────── */
extern "C" void rmlui_replay_picker_update(void) {
    if (!s_model_registered) {
        do_init();
        if (!s_model_registered)
            return;
    }
    if (!s_model_handle)
        return;
    // ⚡ Skip when document is hidden
    if (!rmlui_wrapper_is_game_document_visible("replay_picker"))
        return;

    bool dirty = false;
    if (s_cursor != s_cache.cursor) {
        s_cache.cursor = s_cursor;
        s_model_handle.DirtyVariable("rp_cursor");
        dirty = true;
    }
    if (s_mode != s_cache.mode) {
        s_cache.mode = s_mode;
        s_model_handle.DirtyVariable("rp_mode");
        dirty = true;
    }
    if (s_tab != s_cache.tab) {
        s_cache.tab = s_tab;
        s_model_handle.DirtyVariable("rp_tab");
        dirty = true;
    }
    if (s_page != s_cache.page) {
        s_cache.page = s_page;
        s_model_handle.DirtyVariable("rp_page");
        s_model_handle.DirtyVariable("rp_total_pages");
        s_model_handle.DirtyVariable("rp_has_prev");
        s_model_handle.DirtyVariable("rp_has_next");
        dirty = true;
    }
    if ((int)s_slots.size() != s_cache.slot_count) {
        s_cache.slot_count = (int)s_slots.size();
        s_model_handle.DirtyVariable("rp_slot_count");
        s_model_handle.DirtyVariable("rp_slots");
        dirty = true;
    }
    (void)dirty;
}

/* ── Show / Hide ───────────────────────────────────────────────── */
extern "C" void rmlui_replay_picker_show(void) {
    rmlui_wrapper_show_game_document("replay_picker");
    dirty_all();
}

extern "C" void rmlui_replay_picker_hide(void) {
    rmlui_wrapper_hide_game_document("replay_picker");
}

/* ── Open (called from menu.c instead of ReplayPicker_Open) ───── */
extern "C" void rmlui_replay_picker_open(int mode) {
    s_mode = mode;
    s_tab = 0; /* default to LOCAL */
    s_page = 0;
    s_cursor = 0;
    s_zone = 0;
    s_result = 1;
    s_selected_filename[0] = '\0';
    s_open = true;

    refresh_slot_data();
    rmlui_replay_picker_show();
    dirty_all();

    SDL_Log("[RmlUi ReplayPicker] Opened (mode=%s)", mode == 0 ? "load" : "save");
}

/* ── Poll (called from menu.c instead of ReplayPicker_Update) ─── */
extern "C" int rmlui_replay_picker_poll(void) {
    if (!s_open)
        return s_result;

    /* Read controller input (edge-triggered) */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~PLsw[i][1] & PLsw[i][0]);
    }

    int max_cursor = (int)s_slots.size() - 1;
    if (max_cursor < 0)
        max_cursor = 0;

    /* ── DOWN (wraps from zone 2 → zone 0) ── */
    if (trigger & 0x02) {
        if (s_zone == 0) {
            if (s_cursor < max_cursor)
                s_cursor++;
            else
                s_zone = 1; /* move to page control */
        } else if (s_zone == 1) {
            s_zone = 2; /* move to tab control */
        } else {
            /* zone 2 → wrap to top */
            s_zone = 0;
            s_cursor = 0;
        }
        dirty_all();
    }

    /* ── UP (wraps from zone 0/cursor 0 → zone 2) ── */
    if (trigger & 0x01) {
        if (s_zone == 2) {
            s_zone = 1;
        } else if (s_zone == 1) {
            s_zone = 0; /* back to data rows, cursor stays at last row */
        } else if (s_zone == 0 && s_cursor > 0) {
            s_cursor--;
        } else {
            /* zone 0, cursor 0 → wrap to bottom */
            s_zone = 2;
        }
        dirty_all();
    }

    /* ── LEFT / RIGHT (context-dependent on zone) ── */
    if (trigger & 0x04) {  /* LEFT */
        if (s_zone == 2) { /* tab control — switch to LOCAL */
            if (s_tab != 0) {
                s_tab = 0;
                s_page = 0;
                refresh_slot_data();
            }
        } else if (s_zone == 1) { /* page control — prev page */
            if (s_page > 0) {
                s_page--;
                refresh_slot_data();
            }
        }
        dirty_all();
    }
    if (trigger & 0x08) {  /* RIGHT */
        if (s_zone == 2) { /* tab control — switch to NETPLAY */
            if (s_tab != 1) {
                s_tab = 1;
                s_page = 0;
                refresh_slot_data();
            }
        } else if (s_zone == 1) { /* page control — next page */
            if ((s_page + 1) < total_pages()) {
                s_page++;
                refresh_slot_data();
            }
        }
        dirty_all();
    }

    /* ── Cancel (B / EAST) ── */
    if (trigger & 0x0200) {
        if (s_zone > 0) {
            s_zone = 0; /* go back to data rows first */
            dirty_all();
        } else {
            s_open = false;
            s_result = -1;
            rmlui_replay_picker_hide();
            return -1;
        }
    }

    /* ── Confirm (A / SOUTH) ── */
    if (trigger & 0x0100) {
        if (s_zone == 0 && s_cursor < (int)s_slots.size()) {
            const SlotEntry& slot = s_slots[s_cursor];
            if (s_mode == 0 && !slot.exists) {
                /* Can't load empty slot — do nothing */
            } else {
                SDL_strlcpy(s_selected_filename, slot.filename.c_str(), sizeof(s_selected_filename));
                s_open = false;
                s_result = 0;
                rmlui_replay_picker_hide();
                return 0;
            }
        }
    }

    return 1;
}

/* ── Get selected filename ─────────────────────────────────────── */
extern "C" const char* rmlui_replay_picker_get_filename(void) {
    return s_selected_filename;
}

/* ── Shutdown ──────────────────────────────────────────────────── */
extern "C" void rmlui_replay_picker_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("replay_picker");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("replay_picker");
        s_model_registered = false;
    }
}
