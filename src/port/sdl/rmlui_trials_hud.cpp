/**
 * @file rmlui_trials_hud.cpp
 * @brief RmlUi trial mode HUD data model.
 *
 * Replaces the SSPutStrPro_Scale calls in trials_draw() with an RmlUi
 * document showing the step list with color-coded progress, "COMPLETE!"
 * flash, and "MAX GAUGE" alert.
 *
 * Key globals: g_trials_state, Mode_Type
 */

#include "port/sdl/rmlui_trials_hud.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/training/training_state.h"
#include "sf33rd/Source/Game/training/trials.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct TrialsHudCache {
    int current_step;
    int trial_index;
    bool completed;
    bool failed;
    bool is_active;
    bool gauge_max;
};
static TrialsHudCache s_cache = {};

#define TDIRTY(nm) s_model_handle.DirtyVariable(#nm)

// ─── Init ────────────────────────────────────────────────────
extern "C" void rmlui_trials_hud_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("trials_hud");
    if (!ctor)
        return;

    ctor.BindFunc("trial_active",
                  [](Rml::Variant& v) { v = (bool)(Mode_Type == MODE_TRIALS && g_trials_state.is_active); });
    ctor.BindFunc("trial_header", [](Rml::Variant& v) {
        // Simplified: "TRIAL: <charname> N/M (L/R skip)"
        // We build this from the global state
        char buf[128];
        buf[0] = '\0';
        // Access char name via the trial state
        extern const char* trials_get_current_char_name(void);
        const char* cname = trials_get_current_char_name();
        if (cname) {
            extern int trials_get_current_total(void);
            snprintf(buf,
                     sizeof(buf),
                     "TRIAL: %s %d/%d (L/R skip)",
                     cname,
                     g_trials_state.current_trial_index + 1,
                     trials_get_current_total());
        }
        v = Rml::String(buf);
    });
    ctor.BindFunc("trial_step", [](Rml::Variant& v) { v = (int)g_trials_state.current_step; });
    ctor.BindFunc("trial_completed", [](Rml::Variant& v) { v = (bool)g_trials_state.completed; });
    ctor.BindFunc("trial_failed", [](Rml::Variant& v) { v = (bool)g_trials_state.failed; });
    ctor.BindFunc("trial_gauge_max", [](Rml::Variant& v) {
        // Check if current trial has gauge_max
        extern bool trials_current_has_gauge_max(void);
        v = trials_current_has_gauge_max();
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi TrialsHud] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────
extern "C" void rmlui_trials_hud_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    bool active = (Mode_Type == MODE_TRIALS && g_trials_state.is_active);
    if (active != s_cache.is_active) {
        s_cache.is_active = active;
        TDIRTY(trial_active);
        if (active)
            rmlui_wrapper_show_document("trials_hud");
        else
            rmlui_wrapper_hide_document("trials_hud");
    }

    if (!active)
        return;

    int step = (int)g_trials_state.current_step;
    if (step != s_cache.current_step) {
        s_cache.current_step = step;
        TDIRTY(trial_step);
        TDIRTY(trial_header);
    }

    int idx = g_trials_state.current_trial_index;
    if (idx != s_cache.trial_index) {
        s_cache.trial_index = idx;
        TDIRTY(trial_header);
        TDIRTY(trial_gauge_max);
    }

    bool comp = g_trials_state.completed;
    if (comp != s_cache.completed) {
        s_cache.completed = comp;
        TDIRTY(trial_completed);
    }

    bool fail = g_trials_state.failed;
    if (fail != s_cache.failed) {
        s_cache.failed = fail;
        TDIRTY(trial_failed);
    }
}

#undef TDIRTY

// ─── Shutdown ────────────────────────────────────────────────
extern "C" void rmlui_trials_hud_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("trials_hud");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("trials_hud");
        s_model_registered = false;
    }
}
