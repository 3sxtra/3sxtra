/**
 * @file rmlui_input_display.cpp
 * @brief RmlUi input history overlay — data model + per-frame tracking.
 *
 * Mirrors the ImGui input_display.cpp functionality using RmlUi data bindings.
 * Instead of sprite-sheet icons, uses FGC text notation (arrows + button labels)
 * which scales perfectly at all resolutions.
 *
 * Input tracking logic (history, inactivity timeout) is identical to the ImGui version.
 */
#include "port/sdl/rmlui_input_display.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/sdl/training_menu.h"
#include "common.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <string>
#include <vector>

extern "C" {
    extern u16 p1sw_buff;
    extern u16 p2sw_buff;
}

// ── Input row struct for data binding ──────────────────────────
struct InputRow {
    Rml::String direction;  // e.g. "↗", "↓", "" (neutral)
    Rml::String buttons;    // e.g. "LP MP", "HK"
    Rml::String frames;     // e.g. "3", "12"
};

// ── Internal tracking (mirrors ImGui input_display.cpp) ────────
struct InputInfo {
    u32 mask;
    s32 frame;
};

static std::vector<InputInfo> s_history_p1;
static std::vector<InputInfo> s_history_p2;
static const size_t MAX_HISTORY_SIZE = 10;
static s32 s_render_frame = 0;
static u32 s_last_input_p1 = 0;
static u32 s_last_input_p2 = 0;
static s32 s_last_input_frame_p1 = 0;
static s32 s_last_input_frame_p2 = 0;
static const s32 INACTIVITY_TIMEOUT_FRAMES = 60;

// ── Data model state ───────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

static std::vector<InputRow> s_rows_p1;
static std::vector<InputRow> s_rows_p2;
static bool s_visible = false;

// Previous state for dirty checking
static size_t s_prev_p1_size = 0;
static size_t s_prev_p2_size = 0;
static u32 s_prev_p1_head_mask = 0;
static u32 s_prev_p2_head_mask = 0;
static bool s_prev_visible = false;

// ── Direction bitmask → arrow string ───────────────────────────
static Rml::String direction_to_string(u32 dir) {
    switch (dir & 0xF) {
    case 0x1:         return "\xe2\x86\x91";       // ↑
    case 0x2:         return "\xe2\x86\x93";       // ↓
    case 0x4:         return "\xe2\x86\x90";       // ←
    case 0x8:         return "\xe2\x86\x92";       // →
    case 0x1 | 0x4:   return "\xe2\x86\x96";       // ↖
    case 0x1 | 0x8:   return "\xe2\x86\x97";       // ↗
    case 0x2 | 0x4:   return "\xe2\x86\x99";       // ↙
    case 0x2 | 0x8:   return "\xe2\x86\x98";       // ↘
    default:          return "\xc2\xb7";            // · (neutral)
    }
}

// ── Button bitmask → label string ──────────────────────────────
static Rml::String buttons_to_string(u32 mask) {
    Rml::String result;
    // Punches
    if (mask & 0x10)  { if (!result.empty()) result += " "; result += "LP"; }
    if (mask & 0x20)  { if (!result.empty()) result += " "; result += "MP"; }
    if (mask & 0x40)  { if (!result.empty()) result += " "; result += "HP"; }
    // Kicks
    if (mask & 0x100) { if (!result.empty()) result += " "; result += "LK"; }
    if (mask & 0x200) { if (!result.empty()) result += " "; result += "MK"; }
    if (mask & 0x400) { if (!result.empty()) result += " "; result += "HK"; }
    // Start
    if (mask & 0x1000){ if (!result.empty()) result += " "; result += "ST"; }
    return result;
}

// ── Build display rows from history (newest first) ─────────────
static void build_rows(const std::vector<InputInfo>& history,
                       std::vector<InputRow>& rows) {
    rows.clear();
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        InputRow row;
        row.direction = direction_to_string(it->mask);
        row.buttons = buttons_to_string(it->mask & ~0xF);

        // Frame duration
        s32 next_frame;
        if (it == history.rbegin()) {
            next_frame = s_render_frame + 1;
        } else {
            auto next_it = it - 1;
            next_frame = next_it->frame;
        }
        s32 diff = next_frame - it->frame;
        if (diff < 999) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", diff);
            row.frames = buf;
        } else {
            row.frames = "-";
        }
        rows.push_back(row);
    }
}

// ── Init ───────────────────────────────────────────────────────
extern "C" void rmlui_input_display_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi InputDisplay] No context available");
        return;
    }

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("input_display");
    if (!ctor) {
        SDL_Log("[RmlUi InputDisplay] Failed to create data model");
        return;
    }

    // Register InputRow struct
    if (auto sh = ctor.RegisterStruct<InputRow>()) {
        sh.RegisterMember("direction", &InputRow::direction);
        sh.RegisterMember("buttons", &InputRow::buttons);
        sh.RegisterMember("frames", &InputRow::frames);
    }
    ctor.RegisterArray<std::vector<InputRow>>();

    // Bind arrays
    ctor.Bind("p1_history", &s_rows_p1);
    ctor.Bind("p2_history", &s_rows_p2);

    // Bind visibility
    ctor.BindFunc("visible",
        [](Rml::Variant& v) { v = s_visible; }
    );

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi InputDisplay] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────
extern "C" void rmlui_input_display_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    s_visible = g_training_menu_settings.show_inputs;

    // Show/hide document
    if (s_visible && !rmlui_wrapper_is_document_visible("input_display")) {
        rmlui_wrapper_show_document("input_display");
    } else if (!s_visible && rmlui_wrapper_is_document_visible("input_display")) {
        rmlui_wrapper_hide_document("input_display");
    }

    if (s_visible != s_prev_visible) {
        s_prev_visible = s_visible;
        s_model_handle.DirtyVariable("visible");
    }

    if (!s_visible)
        return;

    s_render_frame++;

    // Track P1 inputs
    u32 current_p1 = p1sw_buff;
    if (current_p1 != s_last_input_p1) {
        s_history_p1.push_back({current_p1, s_render_frame});
        s_last_input_frame_p1 = s_render_frame;
        if (s_history_p1.size() > MAX_HISTORY_SIZE)
            s_history_p1.erase(s_history_p1.begin());
    }
    s_last_input_p1 = current_p1;

    // Track P2 inputs
    u32 current_p2 = p2sw_buff;
    if (current_p2 != s_last_input_p2) {
        s_history_p2.push_back({current_p2, s_render_frame});
        s_last_input_frame_p2 = s_render_frame;
        if (s_history_p2.size() > MAX_HISTORY_SIZE)
            s_history_p2.erase(s_history_p2.begin());
    }
    s_last_input_p2 = current_p2;

    // Inactivity timeout
    if (!s_history_p1.empty() && (s_render_frame - s_last_input_frame_p1) > INACTIVITY_TIMEOUT_FRAMES)
        s_history_p1.clear();
    if (!s_history_p2.empty() && (s_render_frame - s_last_input_frame_p2) > INACTIVITY_TIMEOUT_FRAMES)
        s_history_p2.clear();

    // Dirty check — rebuild rows only when history changed
    bool p1_dirty = (s_history_p1.size() != s_prev_p1_size) ||
                    (!s_history_p1.empty() && s_history_p1.back().mask != s_prev_p1_head_mask);
    bool p2_dirty = (s_history_p2.size() != s_prev_p2_size) ||
                    (!s_history_p2.empty() && s_history_p2.back().mask != s_prev_p2_head_mask);

    // Frame durations change every frame for the newest entry, so always rebuild if non-empty
    if (!s_history_p1.empty()) p1_dirty = true;
    if (!s_history_p2.empty()) p2_dirty = true;

    if (p1_dirty) {
        build_rows(s_history_p1, s_rows_p1);
        s_prev_p1_size = s_history_p1.size();
        s_prev_p1_head_mask = s_history_p1.empty() ? 0 : s_history_p1.back().mask;
        s_model_handle.DirtyVariable("p1_history");
    }

    if (p2_dirty) {
        build_rows(s_history_p2, s_rows_p2);
        s_prev_p2_size = s_history_p2.size();
        s_prev_p2_head_mask = s_history_p2.empty() ? 0 : s_history_p2.back().mask;
        s_model_handle.DirtyVariable("p2_history");
    }
}

// ── Shutdown ───────────────────────────────────────────────────
extern "C" void rmlui_input_display_shutdown(void) {
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("input_display");
        s_model_registered = false;
    }
    s_history_p1.clear();
    s_history_p2.clear();
    s_rows_p1.clear();
    s_rows_p2.clear();
    s_render_frame = 0;
    s_last_input_p1 = 0;
    s_last_input_p2 = 0;
    SDL_Log("[RmlUi InputDisplay] Shut down");
}
