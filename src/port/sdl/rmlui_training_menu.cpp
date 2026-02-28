/**
 * @file rmlui_training_menu.cpp
 * @brief RmlUi training options overlay — data model + config persistence.
 *
 * Mirrors the ImGui training_menu.cpp functionality using RmlUi data bindings.
 * Each TrainingMenuSettings boolean is bound via BindFunc() with setters that
 * persist to the config file, exactly matching the ImGui version's behavior.
 */
#include "port/sdl/rmlui_training_menu.h"
#include "port/config.h"
#include "port/sdl/rmlui_wrapper.h"
#include "port/sdl/training_menu.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

// -------------------------------------------------------------------
// Data model state
// -------------------------------------------------------------------
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Cached snapshot for dirty detection
struct TrainingSnapshot {
    bool hitboxes;
    bool pushboxes;
    bool hurtboxes;
    bool attackboxes;
    bool throwboxes;
    bool advantage;
    bool stun;
    bool inputs;
    bool frame_meter;
};
static TrainingSnapshot s_cache = {};

// -------------------------------------------------------------------
// Init — register data model
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi Training] No context available");
        return;
    }

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("training");
    if (!ctor) {
        SDL_Log("[RmlUi Training] Failed to create data model");
        return;
    }

    // --- Master Hitboxes Toggle ---
    ctor.BindFunc(
        "hitboxes",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_hitboxes; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_hitboxes = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_HITBOXES, g_training_menu_settings.show_hitboxes);
            Config_Save();
        });

    // --- Sub-toggles (only active when hitboxes master is on) ---
    ctor.BindFunc(
        "pushboxes",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_pushboxes; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_pushboxes = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_PUSHBOXES, g_training_menu_settings.show_pushboxes);
            Config_Save();
        });
    ctor.BindFunc(
        "hurtboxes",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_hurtboxes; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_hurtboxes = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_HURTBOXES, g_training_menu_settings.show_hurtboxes);
            Config_Save();
        });
    ctor.BindFunc(
        "attackboxes",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_attackboxes; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_attackboxes = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_ATTACKBOXES, g_training_menu_settings.show_attackboxes);
            Config_Save();
        });
    ctor.BindFunc(
        "throwboxes",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_throwboxes; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_throwboxes = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_THROWBOXES, g_training_menu_settings.show_throwboxes);
            Config_Save();
        });

    // --- Standalone toggles ---
    ctor.BindFunc(
        "advantage",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_advantage; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_advantage = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_ADVANTAGE, g_training_menu_settings.show_advantage);
            Config_Save();
        });
    ctor.BindFunc(
        "stun",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_stun; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_stun = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_STUN, g_training_menu_settings.show_stun);
            Config_Save();
        });
    ctor.BindFunc(
        "inputs",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_inputs; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_inputs = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_INPUTS, g_training_menu_settings.show_inputs);
            Config_Save();
        });
    ctor.BindFunc(
        "frame_meter",
        [](Rml::Variant& v) { v = g_training_menu_settings.show_frame_meter; },
        [](const Rml::Variant& v) {
            g_training_menu_settings.show_frame_meter = v.Get<bool>();
            Config_SetBool(CFG_KEY_TRAINING_FRAME_METER, g_training_menu_settings.show_frame_meter);
            Config_Save();
        });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi Training] Data model registered (9 bindings)");
}

// -------------------------------------------------------------------
// Per-frame update — dirty check
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

#define DIRTY_BOOL(name, field)                                                                                        \
    do {                                                                                                               \
        bool _cur = g_training_menu_settings.field;                                                                    \
        if (_cur != s_cache.name) {                                                                                    \
            s_cache.name = _cur;                                                                                       \
            s_model_handle.DirtyVariable(#name);                                                                       \
        }                                                                                                              \
    } while (0)

    DIRTY_BOOL(hitboxes, show_hitboxes);
    DIRTY_BOOL(pushboxes, show_pushboxes);
    DIRTY_BOOL(hurtboxes, show_hurtboxes);
    DIRTY_BOOL(attackboxes, show_attackboxes);
    DIRTY_BOOL(throwboxes, show_throwboxes);
    DIRTY_BOOL(advantage, show_advantage);
    DIRTY_BOOL(stun, show_stun);
    DIRTY_BOOL(inputs, show_inputs);
    DIRTY_BOOL(frame_meter, show_frame_meter);

#undef DIRTY_BOOL
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_shutdown(void) {
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("training");
        s_model_registered = false;
    }
    SDL_Log("[RmlUi Training] Shut down");
}
