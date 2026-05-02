/**
 * @file rmlui_fx_option.cpp
 * @brief RmlUi FX Option screen data model.
 *
 * Controller-friendly, paged display/shader/visual settings menu.
 * Follows the extra_option.rml layout pattern (data-model cursor + paging)
 * but manages its own cursor and values entirely in C++ — does NOT use
 * the CPS3 g_state.Convert_Buff or g_state.Menu_Cursor_Y globals.
 *
 * Delegates to the same config/shader/mods APIs used by the F2 and F3
 * overlay menus.
 */

#include "port/sdl/rmlui/rmlui_fx_option.h"
#include "game_state.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "port/config/config.h"
#include "port/mods/modded_stage.h"
#include "port/rendering/sdl_bezel.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_debug_hud.h"
#include "port/sdl/app/sdl_app_scale.h"
#include "port/sdl/app/sdl_app_shader_config.h"

/* Globals from sdl_app.c — no header yet */
extern bool mods_menu_shader_bypass_enabled;
extern bool mods_menu_fast_pre_game;
} // extern "C"

// ─── Setting Definitions ────────────────────────────────────────
// Each setting has: label, get value string, cycle left, cycle right

struct FxSetting {
    const char* label;
    Rml::String (*get_value)(void);
    void (*cycle_left)(void);
    void (*cycle_right)(void);
};

// --- Shader System ---
static Rml::String get_shader_system(void) {
    return SDLAppShader_IsLibretroMode() ? "LIBRETRO" : "INTERNAL";
}
static void cycle_shader_system(void) {
    SDLAppShader_ToggleMode();
}

// --- Scale Mode ---
static Rml::String get_scale_mode(void) {
    return Rml::String(scale_mode_name());
}
static void cycle_scale_left(void) {
    int m = (int)scale_mode - 1;
    if (m < 0)
        m = SCALEMODE_COUNT - 1;
    scale_mode = (ScaleMode)m;
    Config_SetString(CFG_KEY_SCALEMODE, scale_mode_to_config_string(scale_mode));
    Config_Save();
}
static void cycle_scale_right(void) {
    int m = (int)scale_mode + 1;
    if (m >= SCALEMODE_COUNT)
        m = 0;
    scale_mode = (ScaleMode)m;
    Config_SetString(CFG_KEY_SCALEMODE, scale_mode_to_config_string(scale_mode));
    Config_Save();
}

// --- Shader Preset ---
static Rml::String get_preset_name(void) {
    int idx = SDLAppShader_GetCurrentIndex();
    const char* name = SDLAppShader_GetPresetName(idx);
    return name ? Rml::String(name) : Rml::String("NONE");
}
static void cycle_preset_left(void) {
    int count = SDLAppShader_GetAvailableCount();
    if (count <= 0)
        return;
    int idx = SDLAppShader_GetCurrentIndex() - 1;
    if (idx < 0)
        idx = count - 1;
    SDLAppShader_LoadPreset(idx);
}
static void cycle_preset_right(void) {
    int count = SDLAppShader_GetAvailableCount();
    if (count <= 0)
        return;
    int idx = SDLAppShader_GetCurrentIndex() + 1;
    if (idx >= count)
        idx = 0;
    SDLAppShader_LoadPreset(idx);
}

// --- Toggle helpers ---
static Rml::String bool_on_off(bool v) {
    return v ? "ON" : "OFF";
}

// --- HD Backgrounds ---
static Rml::String get_hd(void) {
    return bool_on_off(ModdedStage_IsEnabled());
}
static void toggle_hd(void) {
    bool on = !ModdedStage_IsEnabled();
    ModdedStage_SetEnabled(on);
    Config_SetBool(CFG_KEY_HD_STAGES, on);
    Config_Save();
}

// --- Shader Bypass ---
static Rml::String get_shader_bypass(void) {
    return bool_on_off(mods_menu_shader_bypass_enabled);
}
static void toggle_shader_bypass(void) {
    mods_menu_shader_bypass_enabled = !mods_menu_shader_bypass_enabled;
}

// --- Bezels ---
static Rml::String get_bezels(void) {
    return bool_on_off(BezelSystem_IsVisible());
}
static void toggle_bezels(void) {
    bool on = !BezelSystem_IsVisible();
    BezelSystem_SetVisible(on);
    Config_SetBool(CFG_KEY_BEZEL_ENABLED, on);
    Config_Save();
}

// --- VSync ---
static Rml::String get_vsync(void) {
    return bool_on_off(SDLApp_IsVSyncEnabled());
}
static void toggle_vsync(void) {
    SDLApp_SetVSync(!SDLApp_IsVSyncEnabled());
}

// --- Modded BGM ---
static Rml::String get_modded_bgm(void) {
    return bool_on_off(Config_GetBool(CFG_KEY_MODDED_BGM_ENABLED));
}
static void toggle_modded_bgm(void) {
    bool on = !Config_GetBool(CFG_KEY_MODDED_BGM_ENABLED);
    Config_SetBool(CFG_KEY_MODDED_BGM_ENABLED, on);
    Config_Save();
}

// --- Modded Voice ---
static Rml::String get_modded_voice(void) {
    return bool_on_off(Config_GetBool(CFG_KEY_MODDED_VOICE_ENABLED));
}
static void toggle_modded_voice(void) {
    bool on = !Config_GetBool(CFG_KEY_MODDED_VOICE_ENABLED);
    Config_SetBool(CFG_KEY_MODDED_VOICE_ENABLED, on);
    Config_Save();
}

// --- Fast Pre-Game ---
static Rml::String get_fast_pregame(void) {
    return bool_on_off(mods_menu_fast_pre_game);
}
static void toggle_fast_pregame(void) {
    mods_menu_fast_pre_game = !mods_menu_fast_pre_game;
}

// --- Debug HUD ---
static Rml::String get_debug_hud(void) {
    return bool_on_off(SDLAppDebugHud_IsVisible());
}
static void toggle_debug_hud(void) {
    SDLAppDebugHud_Toggle();
}

// --- Page Cycle (Dedicated UI Row) ---
extern "C" void rmlui_fx_option_page_left(void);
extern "C" void rmlui_fx_option_page_right(void);
static const int PAGE_COUNT = 2; // Forward declaration constant value

// --- Widescreen Stretch ---
static Rml::String get_scale_stretch(void) {
    return bool_on_off(scale_stretch_enabled);
}
static void cycle_scale_stretch(void) {
    toggle_scale_stretch();
}

// --- Fullscreen Type ---
static Rml::String get_fullscreen_type(void) {
    return Config_GetBool(CFG_KEY_FULLSCREEN_EXCLUSIVE) ? "Exclusive" : "Borderless";
}
static void toggle_fullscreen_type(void) {
    bool exclusive = !Config_GetBool(CFG_KEY_FULLSCREEN_EXCLUSIVE);
    SDLApp_SetFullscreenExclusive(exclusive);
    Config_Save();
}

// ─── State ───────────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
static int s_cursor = 0;
static int s_page = 0;

static Rml::String get_page_display(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", s_page + 1, PAGE_COUNT);
    return Rml::String(buf);
}

// ─── Page Table ──────────────────────────────────────────────────
// Page 0: Display (8 rows)
// Page 1: Audio & Mods (5 rows)

static const FxSetting s_page0[] = {
    { "PAGE", get_page_display, rmlui_fx_option_page_left, rmlui_fx_option_page_right },
    { "FULLSCREEN TYPE", get_fullscreen_type, toggle_fullscreen_type, toggle_fullscreen_type },
    { "WIDESCREEN STRETCH", get_scale_stretch, cycle_scale_stretch, cycle_scale_stretch },
    { "SHADER SYSTEM", get_shader_system, cycle_shader_system, cycle_shader_system },
    { "SCALE MODE", get_scale_mode, cycle_scale_left, cycle_scale_right },
    { "SHADER PRESET", get_preset_name, cycle_preset_left, cycle_preset_right },
    { "HD BACKGROUNDS", get_hd, toggle_hd, toggle_hd },
    { "SHADER BYPASS", get_shader_bypass, toggle_shader_bypass, toggle_shader_bypass },
};

static const FxSetting s_page1[] = {
    { "PAGE", get_page_display, rmlui_fx_option_page_left, rmlui_fx_option_page_right },
    { "BEZELS", get_bezels, toggle_bezels, toggle_bezels },
    { "VSYNC", get_vsync, toggle_vsync, toggle_vsync },
    { "MODDED BGM", get_modded_bgm, toggle_modded_bgm, toggle_modded_bgm },
    { "MODDED VOICE", get_modded_voice, toggle_modded_voice, toggle_modded_voice },
    { "FAST PRE-GAME", get_fast_pregame, toggle_fast_pregame, toggle_fast_pregame },
    { "DEBUG HUD", get_debug_hud, toggle_debug_hud, toggle_debug_hud },
};

struct Page {
    const FxSetting* settings;
    int count;
};

static const Page s_pages[] = {
    { s_page0, sizeof(s_page0) / sizeof(s_page0[0]) },
    { s_page1, sizeof(s_page1) / sizeof(s_page1[0]) },
};
// PAGE_COUNT is defined above to avoid forward declaration errors.

// Cached values for dirty detection
static Rml::String s_cached_values[10]; // max rows per page
static int s_cached_cursor = -1;
static int s_cached_page = -1;

static const Page& current_page(void) {
    return s_pages[s_page];
}

static void dirty_all_rows(void) {
    if (!s_model_handle)
        return;
    s_model_handle.DirtyVariable("fx_cursor");
    s_model_handle.DirtyVariable("fx_page");
    s_model_handle.DirtyVariable("fx_row_count");
    for (int i = 0; i < 10; i++) {
        char name[24];
        snprintf(name, sizeof(name), "fx_label_%d", i);
        s_model_handle.DirtyVariable(name);
        snprintf(name, sizeof(name), "fx_value_%d", i);
        s_model_handle.DirtyVariable(name);
    }
    // Reset cache
    s_cached_cursor = -1;
    s_cached_page = -1;
    for (auto& v : s_cached_values)
        v.clear();
}

static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("fx_option");
    if (!ctor)
        return;

    // Cursor and page
    ctor.BindFunc("fx_cursor", [](Rml::Variant& v) { v = s_cursor; });
    ctor.BindFunc("fx_page", [](Rml::Variant& v) { v = s_page; });
    ctor.BindFunc("fx_page_count", [](Rml::Variant& v) { v = PAGE_COUNT; });
    ctor.BindFunc("fx_row_count", [](Rml::Variant& v) { v = current_page().count; });

    // Per-row label and value (up to 10)
    for (int i = 0; i < 10; i++) {
        {
            char name[24];
            snprintf(name, sizeof(name), "fx_label_%d", i);
            int row = i;
            ctor.BindFunc(Rml::String(name), [row](Rml::Variant& v) {
                const Page& p = current_page();
                if (row < p.count)
                    v = Rml::String(p.settings[row].label);
                else
                    v = Rml::String("");
            });
        }
        {
            char name[24];
            snprintf(name, sizeof(name), "fx_value_%d", i);
            int row = i;
            ctor.BindFunc(Rml::String(name), [row](Rml::Variant& v) {
                const Page& p = current_page();
                if (row < p.count)
                    v = p.settings[row].get_value();
                else
                    v = Rml::String("");
            });
        }
    }

    // Event callbacks
    ctor.BindEventCallback("change_value_left", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        const Page& p = current_page();
        if (s_cursor < p.count && p.settings[s_cursor].cycle_left)
            p.settings[s_cursor].cycle_left();
    });
    ctor.BindEventCallback("change_value_right", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        const Page& p = current_page();
        if (s_cursor < p.count && p.settings[s_cursor].cycle_right)
            p.settings[s_cursor].cycle_right();
    });
    ctor.BindEventCallback("change_page_left", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        s_page = (s_page - 1 + PAGE_COUNT) % PAGE_COUNT;
        s_cursor = 0;
        dirty_all_rows();
    });
    ctor.BindEventCallback("change_page_right", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        s_page = (s_page + 1) % PAGE_COUNT;
        s_cursor = 0;
        dirty_all_rows();
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi FxOption] Data model registered");
}

extern "C" void rmlui_fx_option_init(void) {
    do_init();
}
// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_fx_option_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    if (!rmlui_wrapper_is_game_document_visible("fx_option"))
        return;

    // Dirty-check cursor
    if (s_cursor != s_cached_cursor) {
        s_cached_cursor = s_cursor;
        s_model_handle.DirtyVariable("fx_cursor");
    }

    // Dirty-check page
    if (s_page != s_cached_page) {
        s_cached_page = s_page;
        s_model_handle.DirtyVariable("fx_page");
        s_model_handle.DirtyVariable("fx_row_count");
        for (int i = 0; i < 7; i++) {
            char name[24];
            snprintf(name, sizeof(name), "fx_label_%d", i);
            s_model_handle.DirtyVariable(name);
        }
    }

    // Dirty-check values on current page
    const Page& p = current_page();
    for (int i = 0; i < p.count && i < 10; i++) {
        Rml::String val = p.settings[i].get_value();
        if (val != s_cached_values[i]) {
            s_cached_values[i] = val;
            char name[24];
            snprintf(name, sizeof(name), "fx_value_%d", i);
            s_model_handle.DirtyVariable(name);
        }
    }
}

// ─── Input handling ──────────────────────────────────────────────
// Called from ms_option_select.c or whichever screen owns the FX Option.
// The CPS3 state machine is NOT used — cursor/page management is self-contained.

extern "C" void rmlui_fx_option_cursor_up(void) {
    s_cursor = (s_cursor - 1 + current_page().count) % current_page().count;
}

extern "C" void rmlui_fx_option_cursor_down(void) {
    s_cursor = (s_cursor + 1) % current_page().count;
}

extern "C" void rmlui_fx_option_value_left(void) {
    const Page& p = current_page();
    if (s_cursor < p.count && p.settings[s_cursor].cycle_left)
        p.settings[s_cursor].cycle_left();
}

extern "C" void rmlui_fx_option_value_right(void) {
    const Page& p = current_page();
    if (s_cursor < p.count && p.settings[s_cursor].cycle_right)
        p.settings[s_cursor].cycle_right();
}

extern "C" void rmlui_fx_option_page_left(void) {
    s_page = (s_page - 1 + PAGE_COUNT) % PAGE_COUNT;
    s_cursor = 0;
    dirty_all_rows();
}

extern "C" void rmlui_fx_option_page_right(void) {
    s_page = (s_page + 1) % PAGE_COUNT;
    s_cursor = 0;
    dirty_all_rows();
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_fx_option_show(void) {
    if (!s_model_registered)
        do_init();
    s_cursor = 0;
    s_page = 0;
    rmlui_wrapper_show_game_document("fx_option");
    dirty_all_rows();
}

extern "C" void rmlui_fx_option_hide(void) {
    rmlui_wrapper_hide_game_document("fx_option");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_fx_option_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("fx_option");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("fx_option");
        s_model_registered = false;
    }
}
