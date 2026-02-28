/**
 * @file rmlui_mods_menu.cpp
 * @brief RmlUi mods overlay menu — data model registration and per-frame sync.
 *
 * Mirrors the ImGui mods_menu.cpp functionality using RmlUi data bindings.
 * Each game-state variable is bound via BindFunc() so the .rml document can
 * use data-checked / data-value attributes to read and write state.
 *
 * The per-frame update function dirty-checks all bound variables against a
 * cached snapshot, calling DirtyVariable() only when something changes. This
 * keeps the retained-mode DOM efficient — only changed elements re-render.
 */
#include "port/sdl/rmlui_mods_menu.h"
#include "port/config.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/sdl_bezel.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {

/* Modded stage system */
void ModdedStage_SetEnabled(bool enabled);
bool ModdedStage_IsEnabled(void);
bool ModdedStage_IsActiveForCurrentStage(void);
int ModdedStage_GetLayerCount(void);
int ModdedStage_GetLoadedStageIndex(void);
void ModdedStage_SetDisableRendering(bool disabled);
bool ModdedStage_IsRenderingDisabled(void);
void ModdedStage_SetAnimationsDisabled(bool disabled);
bool ModdedStage_IsAnimationsDisabled(void);

/* Shader bypass for HD stages */
extern bool mods_menu_shader_bypass_enabled;

/* Engine debug options */
#include "sf33rd/Source/Game/debug/debug_config.h"

/* Game state — 0 = in menus, 1-2 = in gameplay */
extern unsigned char Play_Game;

/* Debug HUD toggle */
extern bool show_debug_hud;
}

// -------------------------------------------------------------------
// Data model state
// -------------------------------------------------------------------
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Cached snapshot for dirty detection
struct ModsSnapshot {
    bool hd_enabled;
    bool hd_active;
    bool shader_bypass;
    bool bezel_enabled;
    bool debug_hud;
    bool render_disabled;
    bool anims_disabled;
    bool bg_draw_off;
    bool blue_back;
    bool hide_shadows;
    bool hide_pal_sprites;
    bool hide_cps3_sprites;
    bool hide_rgb_sprites;
    int sprite_type_sb;
    bool freeze_effects;
    bool mute_bgm;
    bool in_game;
};
static ModsSnapshot s_cache = {};

// Auto-reset tracking (mirrors mods_menu.cpp logic)
static unsigned char s_prev_play_game = 0;

static void reset_debug_on_exit_game(void) {
    Debug_w[DEBUG_NO_DISP_SHADOW] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_PAL] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_CP3] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_RGB] = 0;
    Debug_w[DEBUG_NO_DISP_TYPE_SB] = 0;
    Debug_w[DEBUG_BG_DRAW_OFF] = 0;
    Debug_w[DEBUG_BLUE_BACK] = 0;
    Debug_w[DEBUG_EFF_NOT_MOVE] = 0;
    Debug_w[DEBUG_PUB_BGM_OFF] = 0;
}

// -------------------------------------------------------------------
// Init — register data model
// -------------------------------------------------------------------
extern "C" void rmlui_mods_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi Mods] No context available for data model registration");
        return;
    }

    Rml::DataModelConstructor constructor = ctx->CreateDataModel("mods");
    if (!constructor) {
        SDL_Log("[RmlUi Mods] Failed to create data model constructor");
        return;
    }

    // --- HD Stage Backgrounds ---
    constructor.BindFunc(
        "hd_enabled",
        [](Rml::Variant& v) { v = ModdedStage_IsEnabled(); },
        [](const Rml::Variant& v) { ModdedStage_SetEnabled(v.Get<bool>()); });
    constructor.BindFunc("hd_active", [](Rml::Variant& v) { v = ModdedStage_IsActiveForCurrentStage(); });
    constructor.BindFunc(
        "shader_bypass",
        [](Rml::Variant& v) { v = mods_menu_shader_bypass_enabled; },
        [](const Rml::Variant& v) { mods_menu_shader_bypass_enabled = v.Get<bool>(); });

    // --- Bezels ---
    constructor.BindFunc(
        "bezel_enabled",
        [](Rml::Variant& v) { v = BezelSystem_IsVisible(); },
        [](const Rml::Variant& v) {
            bool on = v.Get<bool>();
            BezelSystem_SetVisible(on);
            Config_SetBool(CFG_KEY_BEZEL_ENABLED, on);
        });

    // --- Debug HUD ---
    constructor.BindFunc(
        "debug_hud",
        [](Rml::Variant& v) { v = show_debug_hud; },
        [](const Rml::Variant& v) {
            show_debug_hud = v.Get<bool>();
            Config_SetBool(CFG_KEY_DEBUG_HUD, show_debug_hud);
            Config_Save();
        });

    // --- Stage Rendering ---
    constructor.BindFunc(
        "render_disabled",
        [](Rml::Variant& v) { v = ModdedStage_IsRenderingDisabled(); },
        [](const Rml::Variant& v) { ModdedStage_SetDisableRendering(v.Get<bool>()); });
    constructor.BindFunc(
        "anims_disabled",
        [](Rml::Variant& v) { v = ModdedStage_IsAnimationsDisabled(); },
        [](const Rml::Variant& v) { ModdedStage_SetAnimationsDisabled(v.Get<bool>()); });

    // --- Debug options (Debug_w array) ---
    constructor.BindFunc(
        "bg_draw_off",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_BG_DRAW_OFF] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_BG_DRAW_OFF] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "blue_back",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_BLUE_BACK] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_BLUE_BACK] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "hide_shadows",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_NO_DISP_SHADOW] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_NO_DISP_SHADOW] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "hide_pal_sprites",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_NO_DISP_SPR_PAL] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_NO_DISP_SPR_PAL] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "hide_cps3_sprites",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_NO_DISP_SPR_CP3] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_NO_DISP_SPR_CP3] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "hide_rgb_sprites",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_NO_DISP_SPR_RGB] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_NO_DISP_SPR_RGB] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "sprite_type_sb",
        [](Rml::Variant& v) { v = (int)Debug_w[DEBUG_NO_DISP_TYPE_SB]; },
        [](const Rml::Variant& v) { Debug_w[DEBUG_NO_DISP_TYPE_SB] = (s8)v.Get<int>(); });
    constructor.BindFunc(
        "freeze_effects",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_EFF_NOT_MOVE] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_EFF_NOT_MOVE] = v.Get<bool>() ? 1 : 0; });
    constructor.BindFunc(
        "mute_bgm",
        [](Rml::Variant& v) { v = (Debug_w[DEBUG_PUB_BGM_OFF] != 0); },
        [](const Rml::Variant& v) { Debug_w[DEBUG_PUB_BGM_OFF] = v.Get<bool>() ? 1 : 0; });

    // --- Read-only state ---
    constructor.BindFunc("in_game", [](Rml::Variant& v) { v = (Play_Game != 0); });

    s_model_handle = constructor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi Mods] Data model registered (17 bindings)");
}

// -------------------------------------------------------------------
// Per-frame update — dirty check and auto-reset
// -------------------------------------------------------------------
extern "C" void rmlui_mods_menu_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    // Auto-reset debug options when transitioning from gameplay to menus
    if (s_prev_play_game != 0 && Play_Game == 0) {
        reset_debug_on_exit_game();
    }
    s_prev_play_game = Play_Game;

// Snapshot current state and dirty-check against cache
#define DIRTY_BOOL(name, expr)                                                                                         \
    do {                                                                                                               \
        bool _cur = (expr);                                                                                            \
        if (_cur != s_cache.name) {                                                                                    \
            s_cache.name = _cur;                                                                                       \
            s_model_handle.DirtyVariable(#name);                                                                       \
        }                                                                                                              \
    } while (0)

#define DIRTY_INT(name, expr)                                                                                          \
    do {                                                                                                               \
        int _cur = (expr);                                                                                             \
        if (_cur != s_cache.name) {                                                                                    \
            s_cache.name = _cur;                                                                                       \
            s_model_handle.DirtyVariable(#name);                                                                       \
        }                                                                                                              \
    } while (0)

    DIRTY_BOOL(hd_enabled, ModdedStage_IsEnabled());
    DIRTY_BOOL(hd_active, ModdedStage_IsActiveForCurrentStage());
    DIRTY_BOOL(shader_bypass, mods_menu_shader_bypass_enabled);
    DIRTY_BOOL(bezel_enabled, BezelSystem_IsVisible());
    DIRTY_BOOL(debug_hud, show_debug_hud);
    DIRTY_BOOL(render_disabled, ModdedStage_IsRenderingDisabled());
    DIRTY_BOOL(anims_disabled, ModdedStage_IsAnimationsDisabled());
    DIRTY_BOOL(bg_draw_off, Debug_w[DEBUG_BG_DRAW_OFF] != 0);
    DIRTY_BOOL(blue_back, Debug_w[DEBUG_BLUE_BACK] != 0);
    DIRTY_BOOL(hide_shadows, Debug_w[DEBUG_NO_DISP_SHADOW] != 0);
    DIRTY_BOOL(hide_pal_sprites, Debug_w[DEBUG_NO_DISP_SPR_PAL] != 0);
    DIRTY_BOOL(hide_cps3_sprites, Debug_w[DEBUG_NO_DISP_SPR_CP3] != 0);
    DIRTY_BOOL(hide_rgb_sprites, Debug_w[DEBUG_NO_DISP_SPR_RGB] != 0);
    DIRTY_INT(sprite_type_sb, (int)Debug_w[DEBUG_NO_DISP_TYPE_SB]);
    DIRTY_BOOL(freeze_effects, Debug_w[DEBUG_EFF_NOT_MOVE] != 0);
    DIRTY_BOOL(mute_bgm, Debug_w[DEBUG_PUB_BGM_OFF] != 0);
    DIRTY_BOOL(in_game, Play_Game != 0);

#undef DIRTY_BOOL
#undef DIRTY_INT
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_mods_menu_shutdown(void) {
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) {
            ctx->RemoveDataModel("mods");
        }
        s_model_registered = false;
    }
    SDL_Log("[RmlUi Mods] Shut down");
}
