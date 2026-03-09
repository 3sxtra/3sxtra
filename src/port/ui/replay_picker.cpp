/**
 * @file replay_picker.cpp
 * @brief Replay slot picker — input/state logic.
 *
 * Shows a list of 20 replay slots with character names and dates.
 * Supports controller navigation (up/down/confirm/cancel) and mouse.
 * ImGui rendering removed — UI handled by rmlui_replay_picker.
 */

#include "port/ui/replay_picker.h"
#include "port/save/native_save.h"

extern "C" {
#include "common.h"
#include "sf33rd/Source/Game/engine/workuser.h"
}

#include <stdio.h>
#include <string.h>

/* ── Character name table ──────────────────────────────────────────── */

static const char* s_char_names[20] = { "Gill",  "Alex",    "Ryu",    "Yun",  "Dudley", "Necro", "Hugo",
                                        "Ibuki", "Elena",   "Oro",    "Yang", "Ken",    "Sean",  "Urien",
                                        "Akuma", "Chun-Li", "Makoto", "Q",    "Twelve", "Remy" };

static const char* get_char_name(int id) __attribute__((unused));
static const char* get_char_name(int id) {
    if (id >= 0 && id < 20)
        return s_char_names[id];
    return "???";
}

/* ── State ─────────────────────────────────────────────────────────── */

static bool s_open = false;
static int s_mode = 0; /* 0=load, 1=save */
static int s_cursor = 0;
static int s_result = 1; /* 1=active, 0=done, -1=cancelled */
static int s_selected_slot = -1;
static bool s_slot_exists[NATIVE_SAVE_REPLAY_SLOTS];
static _sub_info s_slot_info[NATIVE_SAVE_REPLAY_SLOTS];

static void refresh_slot_info(void) {
    for (int i = 0; i < NATIVE_SAVE_REPLAY_SLOTS; i++) {
        s_slot_exists[i] = NativeSave_ReplayExists(i) != 0;
        if (s_slot_exists[i]) {
            NativeSave_GetReplayInfo(i, &s_slot_info[i]);
        } else {
            memset(&s_slot_info[i], 0, sizeof(_sub_info));
        }
    }
}

/* ── Public API ────────────────────────────────────────────────────── */

extern "C" void ReplayPicker_Open(int mode) {
    s_open = true;
    s_mode = mode;
    s_cursor = 0;
    s_result = 1;
    s_selected_slot = -1;
    refresh_slot_info();
}

extern "C" int ReplayPicker_IsOpen(void) {
    return s_open ? 1 : 0;
}

extern "C" int ReplayPicker_GetSelectedSlot(void) {
    return s_selected_slot;
}

extern "C" int ReplayPicker_Update(void) {
    if (!s_open)
        return s_result;

    /* Read controller input */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~PLsw[i][1] & PLsw[i][0]);
    }

    /* Navigate */
    if (trigger & 0x02) { /* Down */
        s_cursor++;
        if (s_cursor >= NATIVE_SAVE_REPLAY_SLOTS)
            s_cursor = NATIVE_SAVE_REPLAY_SLOTS - 1;
    }
    if (trigger & 0x01) { /* Up */
        s_cursor--;
        if (s_cursor < 0)
            s_cursor = 0;
    }

    /* Cancel (button 2 / circle) */
    if (trigger & 0x0200) {
        s_open = false;
        s_result = -1;
        return -1;
    }

    /* Confirm (button 1 / cross) */
    if (trigger & 0x0100) {
        if (s_mode == 0 && !s_slot_exists[s_cursor]) {
            /* Can't load empty slot — do nothing */
        } else {
            s_selected_slot = s_cursor;
            s_open = false;
            s_result = 0;
            return 0;
        }
    }

    /* ImGui rendering removed — UI handled by rmlui_replay_picker */
    return 1;
}
