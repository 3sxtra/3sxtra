/**
 * @file rmlui_replay_picker.cpp
 * @brief RmlUi Replay Picker data model + interaction.
 *
 * Replaces the ImGui ReplayPicker_Open/Update/GetSelectedSlot flow
 * with an RmlUi overlay showing replay file list and confirmation.
 * Input handling (cursor, confirm, cancel) is done here via PLsw polling;
 * the .rml document just reflects the data model state.
 */

#include "port/sdl/rmlui_replay_picker.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/native_save.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <vector>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
} // extern "C"

/* ── Character name table (same as game_hud) ───────────────────── */
static const char* const s_char_names[20] = {
    "RYU",   "ALEX",   "YUEN",    "DUDLEY", "NECRO", "HUGO",   "IBUKI",
    "ELENA", "ORO",    "YANG",    "KEN",    "SEAN",  "MAKOTO", "REMY",
    "Q",     "TWELVE", "CHUN-LI", "URIEN",  "GILL",  "AKUMA"
};

static const char* char_name(int idx) {
    if (idx >= 0 && idx < 20)
        return s_char_names[idx];
    return "???";
}

/* ── Data model ────────────────────────────────────────────────── */
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

/* ── Slot info for data binding ────────────────────────────────── */
struct SlotEntry {
    int index;
    bool exists;
    Rml::String p1_name;
    Rml::String p2_name;
    Rml::String date_str;
};

static std::vector<SlotEntry> s_slots;
static int s_cursor = 0;
static int s_mode = 0;       /* 0=load, 1=save */
static bool s_open = false;
static int s_result = 1;     /* 1=active, 0=done, -1=cancelled */
static int s_selected_slot = -1;

/* ── Cache for dirty detection ─────────────────────────────────── */
struct ReplayPickerCache {
    int cursor;
    int mode;
};
static ReplayPickerCache s_cache = {};

static void refresh_slot_data(void) {
    s_slots.clear();
    for (int i = 0; i < NATIVE_SAVE_REPLAY_SLOTS; i++) {
        SlotEntry entry;
        entry.index = i;
        entry.exists = NativeSave_ReplayExists(i) != 0;
        if (entry.exists) {
            _sub_info info;
            NativeSave_GetReplayInfo(i, &info);
            entry.p1_name = char_name(info.player[0]);
            entry.p2_name = char_name(info.player[1]);
            char buf[32];
            SDL_snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
                         info.date.year, info.date.month, info.date.day,
                         info.date.hour, info.date.min);
            entry.date_str = buf;
        } else {
            entry.p1_name = "---";
            entry.p2_name = "---";
            entry.date_str = "--- empty ---";
        }
        s_slots.push_back(entry);
    }
}

/* ── Init (called once at startup from sdl_app.c) ─────────────── */
extern "C" void rmlui_replay_picker_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("replay_picker");
    if (!ctor)
        return;

    /* Register SlotEntry struct */
    if (auto sh = ctor.RegisterStruct<SlotEntry>()) {
        sh.RegisterMember("index",    &SlotEntry::index);
        sh.RegisterMember("exists",   &SlotEntry::exists);
        sh.RegisterMember("p1_name",  &SlotEntry::p1_name);
        sh.RegisterMember("p2_name",  &SlotEntry::p2_name);
        sh.RegisterMember("date_str", &SlotEntry::date_str);
    }
    ctor.RegisterArray<std::vector<SlotEntry>>();

    ctor.Bind("rp_slots",  &s_slots);
    ctor.BindFunc("rp_cursor", [](Rml::Variant& v) { v = s_cursor; });
    ctor.BindFunc("rp_mode",   [](Rml::Variant& v) { v = s_mode; });
    ctor.BindFunc("rp_open",   [](Rml::Variant& v) { v = s_open; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi ReplayPicker] Data model registered");
}

/* ── Per-frame update (called from sdl_app.c render loop) ──────── */
extern "C" void rmlui_replay_picker_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    if (s_cursor != s_cache.cursor) {
        s_cache.cursor = s_cursor;
        s_model_handle.DirtyVariable("rp_cursor");
    }
    if (s_mode != s_cache.mode) {
        s_cache.mode = s_mode;
        s_model_handle.DirtyVariable("rp_mode");
    }
}

/* ── Show / Hide ───────────────────────────────────────────────── */
extern "C" void rmlui_replay_picker_show(void) {
    rmlui_wrapper_show_game_document("replay_picker");
    if (s_model_handle) {
        s_model_handle.DirtyVariable("rp_cursor");
        s_model_handle.DirtyVariable("rp_mode");
        s_model_handle.DirtyVariable("rp_open");
        s_model_handle.DirtyVariable("rp_slots");
    }
}

extern "C" void rmlui_replay_picker_hide(void) {
    rmlui_wrapper_hide_game_document("replay_picker");
}

/* ── Open (called from menu.c instead of ReplayPicker_Open) ───── */
extern "C" void rmlui_replay_picker_open(int mode) {
    s_mode = mode;
    s_cursor = 0;
    s_result = 1;
    s_selected_slot = -1;
    s_open = true;

    refresh_slot_data();
    rmlui_replay_picker_show();

    if (s_model_handle) {
        s_model_handle.DirtyVariable("rp_slots");
        s_model_handle.DirtyVariable("rp_cursor");
        s_model_handle.DirtyVariable("rp_mode");
        s_model_handle.DirtyVariable("rp_open");
    }

    SDL_Log("[RmlUi ReplayPicker] Opened (mode=%s)", mode == 0 ? "load" : "save");
}

/* ── Poll (called from menu.c instead of ReplayPicker_Update) ─── */
extern "C" int rmlui_replay_picker_poll(void) {
    if (!s_open)
        return s_result;

    /* Read controller input */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~PLsw[i][1] & PLsw[i][0]);
    }

    /* Navigate — 5-column grid layout */
    const int COLS = 5;
    if (trigger & 0x02) { /* Down — move one row */
        if (s_cursor + COLS < NATIVE_SAVE_REPLAY_SLOTS)
            s_cursor += COLS;
    }
    if (trigger & 0x01) { /* Up — move one row */
        if (s_cursor - COLS >= 0)
            s_cursor -= COLS;
    }
    if (trigger & 0x08) { /* Right */
        if (s_cursor + 1 < NATIVE_SAVE_REPLAY_SLOTS)
            s_cursor++;
    }
    if (trigger & 0x04) { /* Left */
        if (s_cursor > 0)
            s_cursor--;
    }

    /* Cancel (button 2) */
    if (trigger & 0x0200) {
        s_open = false;
        s_result = -1;
        rmlui_replay_picker_hide();
        return -1;
    }

    /* Confirm (button 1) */
    if (trigger & 0x0100) {
        if (s_mode == 0 && s_cursor < NATIVE_SAVE_REPLAY_SLOTS &&
            !s_slots[s_cursor].exists) {
            /* Can't load empty slot — do nothing */
        } else {
            s_selected_slot = s_cursor;
            s_open = false;
            s_result = 0;
            rmlui_replay_picker_hide();
            return 0;
        }
    }

    return 1;
}

/* ── Get selected slot ─────────────────────────────────────────── */
extern "C" int rmlui_replay_picker_get_slot(void) {
    return s_selected_slot;
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
