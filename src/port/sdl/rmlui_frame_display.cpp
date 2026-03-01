/**
 * @file rmlui_frame_display.cpp
 * @brief RmlUi frame meter overlay — data model + per-frame tracking.
 *
 * Mirrors the ImGui frame_display.cpp functionality using RmlUi data bindings.
 * Frame cells are rendered as small div elements with CSS class-based coloring.
 * Stats text (Startup/Total/Advantage) is bound as formatted strings.
 *
 * Frame recording logic (idle detection, inactivity clear) is identical to
 * the ImGui version.
 */
#include "port/sdl/rmlui_frame_display.h"
#include "common.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/sdl/training_menu.h"
#include "sf33rd/Source/Game/training/training_state.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <cstdio>
#include <deque>
#include <string>
#include <vector>

extern "C" {
extern bool show_training_menu;
}

// ── Frame cell struct for data binding ─────────────────────────
struct FrameCell {
    Rml::String css_class; // "startup", "active", "recovery", etc.
};

// ── Internal state (mirrors ImGui frame_display.cpp) ───────────
struct FrameRecord {
    TrainingFrameState p1_state;
    TrainingFrameState p2_state;
    s32 g_frame;
};

static const size_t MAX_FRAME_HISTORY = 120;
static std::deque<FrameRecord> s_frame_history;
static s32 s_last_recorded_frame = -1;
static s32 s_consecutive_idle = 0;
static bool s_started_tracking = false;

// ── Data model state ───────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

static std::vector<FrameCell> s_p1_cells;
static std::vector<FrameCell> s_p2_cells;
static Rml::String s_p1_stats;
static Rml::String s_p2_stats;
static Rml::String s_p1_adv_class = "neutral";
static Rml::String s_p2_adv_class = "neutral";
static bool s_visible = false;

// Previous state for dirty checking
static Rml::String s_prev_p1_stats;
static Rml::String s_prev_p2_stats;
static size_t s_prev_cell_count = 0;
static bool s_prev_visible = false;

// ── TrainingFrameState → CSS class ─────────────────────────────
static Rml::String state_to_class(TrainingFrameState state) {
    switch (state) {
    case FRAME_STATE_STARTUP:
        return "startup";
    case FRAME_STATE_ACTIVE:
        return "active";
    case FRAME_STATE_RECOVERY:
        return "recovery";
    case FRAME_STATE_HITSTUN:
        return "hitstun";
    case FRAME_STATE_BLOCKSTUN:
        return "blockstun";
    case FRAME_STATE_DOWN:
        return "down";
    case FRAME_STATE_IDLE:
    default:
        return "idle";
    }
}

// ── Build stats string (same logic as ImGui version) ───────────
static void build_stats(char* buf, size_t sz, const TrainingPlayerState& ps) {
    if (ps.last_startup > 0)
        snprintf(buf, sz, "Startup %dF", (int)ps.last_startup);
    else
        snprintf(buf, sz, "Startup --");

    char tmp[64];
    s32 total = (s32)ps.last_startup + (s32)ps.last_active + (s32)ps.last_recovery;
    if (total > 0)
        snprintf(tmp, sizeof(tmp), " / Total %dF", total);
    else
        snprintf(tmp, sizeof(tmp), " / Total --");
    strncat(buf, tmp, sz - strlen(buf) - 1);

    if (ps.advantage_active) {
        strncat(buf, " / Advantage ...", sz - strlen(buf) - 1);
    } else if (ps.last_startup > 0 || ps.last_active > 0) {
        if (ps.advantage_value > 0)
            snprintf(tmp, sizeof(tmp), " / Advantage +%d", (int)ps.advantage_value);
        else if (ps.advantage_value < 0)
            snprintf(tmp, sizeof(tmp), " / Advantage %d", (int)ps.advantage_value);
        else
            snprintf(tmp, sizeof(tmp), " / Advantage 0");
        strncat(buf, tmp, sz - strlen(buf) - 1);
    } else {
        strncat(buf, " / Advantage --", sz - strlen(buf) - 1);
    }
}

// ── Advantage → CSS class ──────────────────────────────────────
static Rml::String advantage_class(s32 value, bool active, bool has_move) {
    if (active || !has_move)
        return "neutral";
    if (value > 0)
        return "positive";
    if (value < 0)
        return "negative";
    return "neutral";
}

// ── Init ───────────────────────────────────────────────────────
extern "C" void rmlui_frame_display_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi FrameDisplay] No context available");
        return;
    }

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("frame_display");
    if (!ctor) {
        SDL_Log("[RmlUi FrameDisplay] Failed to create data model");
        return;
    }

    // Register FrameCell struct
    if (auto sh = ctor.RegisterStruct<FrameCell>()) {
        sh.RegisterMember("css_class", &FrameCell::css_class);
    }
    ctor.RegisterArray<std::vector<FrameCell>>();

    // Bind cell arrays
    ctor.Bind("p1_cells", &s_p1_cells);
    ctor.Bind("p2_cells", &s_p2_cells);

    // Bind stats text
    ctor.BindFunc("p1_stats", [](Rml::Variant& v) { v = s_p1_stats; });
    ctor.BindFunc("p2_stats", [](Rml::Variant& v) { v = s_p2_stats; });

    // Bind advantage color classes
    ctor.BindFunc("p1_adv_class", [](Rml::Variant& v) { v = s_p1_adv_class; });
    ctor.BindFunc("p2_adv_class", [](Rml::Variant& v) { v = s_p2_adv_class; });

    // Bind visibility
    ctor.BindFunc("visible", [](Rml::Variant& v) { v = s_visible; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi FrameDisplay] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────
extern "C" void rmlui_frame_display_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    // Only show frame meter during active fights — not on menus/title screen
    extern u8 Play_Game;
    s_visible = g_training_menu_settings.show_frame_meter && !show_training_menu && (Play_Game == 1);

    // Show/hide document
    if (s_visible && !rmlui_wrapper_is_document_visible("frame_display")) {
        rmlui_wrapper_show_document("frame_display");
    } else if (!s_visible && rmlui_wrapper_is_document_visible("frame_display")) {
        rmlui_wrapper_hide_document("frame_display");
    }

    if (s_visible != s_prev_visible) {
        s_prev_visible = s_visible;
        s_model_handle.DirtyVariable("visible");
    }

    if (!s_visible)
        return;

    // Record frames (same logic as ImGui version)
    s32 current_frame = g_training_state.frame_number;
    bool both_idle = (g_training_state.p1.current_frame_state == FRAME_STATE_IDLE) &&
                     (g_training_state.p2.current_frame_state == FRAME_STATE_IDLE);

    if (both_idle && g_training_state.is_in_match) {
        if (current_frame != s_last_recorded_frame) {
            s_consecutive_idle++;
            if (s_consecutive_idle >= 90 && !s_frame_history.empty()) {
                s_frame_history.clear();
            }
        }
    } else {
        s_consecutive_idle = 0;
    }

    if (current_frame != s_last_recorded_frame && g_training_state.is_in_match && !both_idle) {
        FrameRecord rec;
        rec.p1_state = g_training_state.p1.current_frame_state;
        rec.p2_state = g_training_state.p2.current_frame_state;
        rec.g_frame = current_frame;
        s_frame_history.push_back(rec);
        if (s_frame_history.size() > MAX_FRAME_HISTORY)
            s_frame_history.pop_front();
        s_last_recorded_frame = current_frame;
        s_started_tracking = true;
    }

    if (!s_started_tracking)
        return;

    // Rebuild cell arrays
    bool cells_dirty = (s_frame_history.size() != s_prev_cell_count);
    // Always rebuild when history is growing (new frame added)
    if (s_frame_history.size() > 0 && s_frame_history.size() != s_prev_cell_count)
        cells_dirty = true;
    // Also rebuild when a frame was just recorded
    if (current_frame == s_last_recorded_frame && s_frame_history.size() > 0)
        cells_dirty = true;

    if (cells_dirty) {
        size_t n = s_frame_history.size();
        s_p1_cells.resize(n);
        s_p2_cells.resize(n);
        for (size_t i = 0; i < n; i++) {
            s_p1_cells[i].css_class = state_to_class(s_frame_history[i].p1_state);
            s_p2_cells[i].css_class = state_to_class(s_frame_history[i].p2_state);
        }
        s_prev_cell_count = n;
        s_model_handle.DirtyVariable("p1_cells");
        s_model_handle.DirtyVariable("p2_cells");
    }

    // Build stats strings
    char p1_buf[128] = "";
    build_stats(p1_buf, sizeof(p1_buf), g_training_state.p1);
    Rml::String new_p1_stats = p1_buf;

    // P2 stats: use own stats if P2 attacked, otherwise mirror P1 with flipped advantage
    char p2_buf[128] = "";
    bool p2_has_move = (g_training_state.p2.last_startup > 0 || g_training_state.p2.last_active > 0);
    if (p2_has_move) {
        build_stats(p2_buf, sizeof(p2_buf), g_training_state.p2);
    } else {
        s32 p2_adv = -g_training_state.p1.advantage_value;
        bool p1_move_done = (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0);
        if (p1_move_done && !g_training_state.p1.advantage_active) {
            if (p2_adv > 0)
                snprintf(p2_buf, sizeof(p2_buf), "Startup -- / Total -- / Advantage +%d", (int)p2_adv);
            else
                snprintf(p2_buf, sizeof(p2_buf), "Startup -- / Total -- / Advantage %d", (int)p2_adv);
        } else {
            snprintf(p2_buf, sizeof(p2_buf), "Startup -- / Total -- / Advantage --");
        }
    }
    Rml::String new_p2_stats = p2_buf;

    if (new_p1_stats != s_prev_p1_stats) {
        s_p1_stats = new_p1_stats;
        s_prev_p1_stats = new_p1_stats;
        s_model_handle.DirtyVariable("p1_stats");
    }

    if (new_p2_stats != s_prev_p2_stats) {
        s_p2_stats = new_p2_stats;
        s_prev_p2_stats = new_p2_stats;
        s_model_handle.DirtyVariable("p2_stats");
    }

    // Advantage color classes
    bool p1_has_move = (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0);
    Rml::String new_p1_adv =
        advantage_class(g_training_state.p1.advantage_value, g_training_state.p1.advantage_active, p1_has_move);
    if (new_p1_adv != s_p1_adv_class) {
        s_p1_adv_class = new_p1_adv;
        s_model_handle.DirtyVariable("p1_adv_class");
    }

    s32 p2_adv_val = p2_has_move ? g_training_state.p2.advantage_value : -g_training_state.p1.advantage_value;
    bool p2_resolved = p2_has_move ? (!g_training_state.p2.advantage_active &&
                                      (g_training_state.p2.last_startup > 0 || g_training_state.p2.last_active > 0))
                                   : (!g_training_state.p1.advantage_active &&
                                      (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0));
    Rml::String new_p2_adv = p2_resolved ? advantage_class(p2_adv_val, false, true) : "neutral";
    if (new_p2_adv != s_p2_adv_class) {
        s_p2_adv_class = new_p2_adv;
        s_model_handle.DirtyVariable("p2_adv_class");
    }
}

// ── Shutdown ───────────────────────────────────────────────────
extern "C" void rmlui_frame_display_shutdown(void) {
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("frame_display");
        s_model_registered = false;
    }
    s_frame_history.clear();
    s_p1_cells.clear();
    s_p2_cells.clear();
    s_consecutive_idle = 0;
    s_started_tracking = false;
    SDL_Log("[RmlUi FrameDisplay] Shut down");
}
