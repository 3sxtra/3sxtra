/**
 * @file rmlui_input_display.cpp
 * @brief RmlUi input history overlay — data model + per-frame tracking.
 *
 * Renders on the **window** context at native resolution for crisp icons,
 * but positions panels relative to the game viewport (letterbox rect) so
 * the display appears inside the game area, not over bezels.
 *
 * Uses CSS class names for sprite-sheet icon rendering via @spritesheet
 * decorators — matching the ImGui version's visual output.
 */
#include "port/sdl/rmlui_input_display.h"
#include "common.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/training_menu.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <string>
#include <vector>

extern "C" {
extern u16 p1sw_buff;
extern u16 p2sw_buff;
}

// ── Button icon struct for data binding ────────────────────────
struct ButtonIcon {
    Rml::String cls; // CSS class name, e.g. "icon btn-lp"
};

// ── Input row struct for data binding ──────────────────────────
struct InputRow {
    Rml::String direction;           // CSS class for direction icon, e.g. "icon dir-up"
    std::vector<ButtonIcon> buttons; // one entry per pressed button
    Rml::String frames;              // e.g. "3", "12"
};

// ── Internal tracking ──────────────────────────────────────────
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

// Viewport positioning (pixels, updated per-frame from letterbox rect)
static Rml::String s_p1_left = "10px";
static Rml::String s_p1_top = "100px";
static Rml::String s_p2_right = "10px";
static Rml::String s_p2_top = "100px";
static Rml::String s_icon_size = "24px";
static Rml::String s_font_size = "11px";
static Rml::String s_panel_width = "120px";

// Previous state for dirty checking
static size_t s_prev_p1_size = 0;
static size_t s_prev_p2_size = 0;
static u32 s_prev_p1_head_mask = 0;
static u32 s_prev_p2_head_mask = 0;
static bool s_prev_visible = false;
static int s_prev_win_w = 0;
static int s_prev_win_h = 0;

// ── Direction bitmask → icon CSS class ─────────────────────────
static Rml::String direction_to_class(u32 dir) {
    switch (dir & 0xF) {
    case 0x1:
        return "icon dir-up";
    case 0x2:
        return "icon dir-down";
    case 0x4:
        return "icon dir-left";
    case 0x8:
        return "icon dir-right";
    case 0x1 | 0x4:
        return "icon dir-ul";
    case 0x1 | 0x8:
        return "icon dir-ur";
    case 0x2 | 0x4:
        return "icon dir-dl";
    case 0x2 | 0x8:
        return "icon dir-dr";
    default:
        return "icon dir-neutral";
    }
}

// ── Ordered button bits for consistent rendering order ─────────
static const u32 s_button_bits[] = {
    0x10,  0x20,  0x40,  // LP, MP, HP
    0x100, 0x200, 0x400, // LK, MK, HK
    0x1000               // Start
};
static const char* s_button_classes[] = { "icon btn-lp", "icon btn-mp", "icon btn-hp", "icon btn-lk",
                                          "icon btn-mk", "icon btn-hk", "icon btn-st" };
static const int s_num_buttons = 7;

// ── Build button icon list from mask ───────────────────────────
static void buttons_to_icons(u32 mask, std::vector<ButtonIcon>& out) {
    out.clear();
    u32 actions = mask & ~0xF;
    if (actions == 0)
        return;
    for (int i = 0; i < s_num_buttons; i++) {
        if (actions & s_button_bits[i]) {
            out.push_back({ s_button_classes[i] });
        }
    }
}

// ── Build display rows from history (newest first) ─────────────
static void build_rows(const std::vector<InputInfo>& history, std::vector<InputRow>& rows) {
    rows.clear();
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        InputRow row;
        row.direction = direction_to_class(it->mask);
        buttons_to_icons(it->mask, row.buttons);

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

// ── Helper: format a float as "Npx" string ─────────────────────
static Rml::String px_str(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0fpx", v);
    return buf;
}

// ── Update viewport positioning strings ────────────────────────
static void update_viewport_positions(int win_w, int win_h) {
    SDL_FRect vp = get_letterbox_rect(win_w, win_h);
    float scale = (vp.h / 480.0f) * 0.85f;
    if (scale < 0.1f)
        scale = 0.1f;

    float margin = 10.0f * scale;
    float top_offset = 100.0f * scale;

    s_p1_left = px_str(vp.x + margin);
    s_p1_top = px_str(vp.y + top_offset);
    // P2: distance from right window edge to right edge of panel
    float p2_right = (float)win_w - (vp.x + vp.w - margin);
    s_p2_right = px_str(p2_right);
    s_p2_top = px_str(vp.y + top_offset);
    s_icon_size = px_str(32.0f * scale);
    s_font_size = px_str(14.0f * scale);
    s_panel_width = px_str(120.0f * scale);
}

// ── Init ───────────────────────────────────────────────────────
extern "C" void rmlui_input_display_init(void) {
    // Window context: renders at native resolution for crisp icons
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

    // Register ButtonIcon struct
    if (auto sh = ctor.RegisterStruct<ButtonIcon>()) {
        sh.RegisterMember("cls", &ButtonIcon::cls);
    }
    ctor.RegisterArray<std::vector<ButtonIcon>>();

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
    ctor.BindFunc("visible", [](Rml::Variant& v) { v = s_visible; });

    // Bind viewport positioning
    ctor.BindFunc("p1_left", [](Rml::Variant& v) { v = s_p1_left; });
    ctor.BindFunc("p1_top", [](Rml::Variant& v) { v = s_p1_top; });
    ctor.BindFunc("p2_right", [](Rml::Variant& v) { v = s_p2_right; });
    ctor.BindFunc("p2_top", [](Rml::Variant& v) { v = s_p2_top; });
    ctor.BindFunc("icon_size", [](Rml::Variant& v) { v = s_icon_size; });
    ctor.BindFunc("font_size", [](Rml::Variant& v) { v = s_font_size; });
    ctor.BindFunc("panel_width", [](Rml::Variant& v) { v = s_panel_width; });

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

    // Update viewport positioning when window size changes
    int win_w, win_h;
    SDL_Window* win = SDLApp_GetWindow();
    SDL_GetWindowSize(win, &win_w, &win_h);
    if (win_w != s_prev_win_w || win_h != s_prev_win_h) {
        s_prev_win_w = win_w;
        s_prev_win_h = win_h;
        update_viewport_positions(win_w, win_h);
        s_model_handle.DirtyVariable("p1_left");
        s_model_handle.DirtyVariable("p1_top");
        s_model_handle.DirtyVariable("p2_right");
        s_model_handle.DirtyVariable("p2_top");
        s_model_handle.DirtyVariable("icon_size");
        s_model_handle.DirtyVariable("font_size");
        s_model_handle.DirtyVariable("panel_width");
    }

    s_render_frame++;

    // Track P1 inputs
    u32 current_p1 = p1sw_buff;
    if (current_p1 != s_last_input_p1) {
        s_history_p1.push_back({ current_p1, s_render_frame });
        s_last_input_frame_p1 = s_render_frame;
        if (s_history_p1.size() > MAX_HISTORY_SIZE)
            s_history_p1.erase(s_history_p1.begin());
    }
    s_last_input_p1 = current_p1;

    // Track P2 inputs
    u32 current_p2 = p2sw_buff;
    if (current_p2 != s_last_input_p2) {
        s_history_p2.push_back({ current_p2, s_render_frame });
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
    if (!s_history_p1.empty())
        p1_dirty = true;
    if (!s_history_p2.empty())
        p2_dirty = true;

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
