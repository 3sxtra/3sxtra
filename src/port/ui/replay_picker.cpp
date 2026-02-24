/**
 * @file replay_picker.cpp
 * @brief ImGui overlay for selecting replay slots (load/save).
 *
 * Shows a list of 20 replay slots with character names and dates.
 * Supports controller navigation (up/down/confirm/cancel) and mouse.
 */

#include "port/ui/replay_picker.h"
#include "port/native_save.h"

extern "C" {
#include "common.h"
#include "sf33rd/Source/Game/engine/workuser.h"
}

#include <imgui.h>
#include <stdio.h>
#include <string.h>

/* ── Character name table ──────────────────────────────────────────── */

static const char* s_char_names[20] = { "Gill",  "Alex",    "Ryu",    "Yun",  "Dudley", "Necro", "Hugo",
                                        "Ibuki", "Elena",   "Oro",    "Yang", "Ken",    "Sean",  "Urien",
                                        "Akuma", "Chun-Li", "Makoto", "Q",    "Twelve", "Remy" };

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

    /* ── Render ──────────────────────────────────────────────────── */

    ImGuiIO& io = ImGui::GetIO();
    float scale = io.DisplaySize.y / 480.0f;
    if (scale < 1.0f)
        scale = 1.0f;

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420 * scale, 360 * scale), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12 * scale, 12 * scale));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.15f, 0.15f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.2f, 0.2f, 0.35f, 1.0f));

    const char* title = (s_mode == 0) ? "Load Replay" : "Save Replay";
    bool visible = true;

    if (ImGui::Begin(title,
                     &visible,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoScrollbar)) {

        float row_h = 22.0f * scale;
        float list_h = ImGui::GetContentRegionAvail().y - 30 * scale;

        if (ImGui::BeginChild("ReplayList", ImVec2(0, list_h), true)) {
            for (int i = 0; i < NATIVE_SAVE_REPLAY_SLOTS; i++) {
                bool selected = (i == s_cursor);
                char label[128];

                if (s_slot_exists[i]) {
                    const _sub_info* info = &s_slot_info[i];
                    snprintf(label,
                             sizeof(label),
                             "%02d  %s vs %s  %04d-%02d-%02d %02d:%02d",
                             i + 1,
                             get_char_name(info->player[0]),
                             get_char_name(info->player[1]),
                             info->date.year,
                             info->date.month,
                             info->date.day,
                             info->date.hour,
                             info->date.min);
                } else {
                    snprintf(label, sizeof(label), "%02d  --- empty ---", i + 1);
                }

                if (ImGui::Selectable(label, selected, 0, ImVec2(0, row_h))) {
                    s_cursor = i;
                    /* Double-click or click to select */
                    if (s_mode == 1 || s_slot_exists[i]) {
                        s_selected_slot = i;
                        s_open = false;
                        s_result = 0;
                    }
                }

                /* Auto-scroll to cursor */
                if (selected) {
                    ImGui::SetScrollHereY();
                }
            }
        }
        ImGui::EndChild();

        /* Footer */
        ImGui::Separator();
        ImGui::TextDisabled("UP/DOWN: navigate  |  A: select  |  B: cancel");
    }
    ImGui::End();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    if (!visible) {
        s_open = false;
        s_result = -1;
        return -1;
    }

    return 1;
}
