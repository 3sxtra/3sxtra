/**
 * @file rmlui_training_hud.cpp
 * @brief Always-on RmlUi training HUD overlay.
 *
 * Provides data-bound text fields that Lua populates each frame via
 * engine.set_hud_text(field, value).  The RML document is loaded once
 * at init and remains visible whenever training mode is active.
 *
 * Tier 1 fields (text overlays):
 *   p1_stun, p2_stun        — combo stun accumulation
 *   p1_life, p2_life        — HP / max HP
 *   p1_meter, p2_meter      — super gauge / max
 *   p1_advantage, p2_advantage — frame advantage with sign
 *   advantage_class          — "positive" / "negative" / "neutral"
 */
#include "rmlui_training_hud.h"
#include "rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstring>

extern "C" {
#include "sf33rd/Source/Game/training/training_state.h"
}

// -------------------------------------------------------------------
// HUD text buffer — written by Lua, read by data model
// -------------------------------------------------------------------
struct HudTextFields {
    Rml::String p1_stun, p2_stun;
    Rml::String p1_life, p2_life;
    Rml::String p1_meter, p2_meter;
    Rml::String p1_advantage, p2_advantage;
    Rml::String advantage_class; // "positive" / "negative" / "neutral"
};

static HudTextFields s_fields;
static HudTextFields s_cache; // for dirty detection
static Rml::DataModelHandle s_model;
static bool s_registered = false;
static bool s_hud_visible = false; // ⚡ on-demand: tracks effie_hud document visibility

// -------------------------------------------------------------------
// Lua bridge: set_hud_text(field, value)
// -------------------------------------------------------------------
extern "C" void rmlui_training_hud_set_text(const char* field, const char* value) {
    if (!field || !value)
        return;

    // ⚡ Lazy show: first Lua write activates the HUD document.
    // Avoids showing at boot when training mode is not active.
    if (!s_hud_visible && s_registered) {
        rmlui_wrapper_show_document("effie_hud");
        s_hud_visible = true;
    }

    // Map field name to struct member
    if (strcmp(field, "p1_stun") == 0)
        s_fields.p1_stun = value;
    else if (strcmp(field, "p2_stun") == 0)
        s_fields.p2_stun = value;
    else if (strcmp(field, "p1_life") == 0)
        s_fields.p1_life = value;
    else if (strcmp(field, "p2_life") == 0)
        s_fields.p2_life = value;
    else if (strcmp(field, "p1_meter") == 0)
        s_fields.p1_meter = value;
    else if (strcmp(field, "p2_meter") == 0)
        s_fields.p2_meter = value;
    else if (strcmp(field, "p1_advantage") == 0)
        s_fields.p1_advantage = value;
    else if (strcmp(field, "p2_advantage") == 0)
        s_fields.p2_advantage = value;
    else if (strcmp(field, "advantage_class") == 0)
        s_fields.advantage_class = value;
}

extern "C" void rmlui_training_hud_set_gauge(const char* field, float fill) {
    // Tier 2 — stub for now
    (void)field;
    (void)fill;
}

// -------------------------------------------------------------------
// Init
// -------------------------------------------------------------------
extern "C" void rmlui_training_hud_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("training_hud");
    if (!ctor) {
        SDL_Log("[Training HUD] Failed to create data model");
        return;
    }

    // Bind all text fields as read-only getters
#define BIND_TEXT(name) ctor.BindFunc(#name, [](Rml::Variant& v) { v = s_fields.name; })

    BIND_TEXT(p1_stun);
    BIND_TEXT(p2_stun);
    BIND_TEXT(p1_life);
    BIND_TEXT(p2_life);
    BIND_TEXT(p1_meter);
    BIND_TEXT(p2_meter);
    BIND_TEXT(p1_advantage);
    BIND_TEXT(p2_advantage);
    BIND_TEXT(advantage_class);
#undef BIND_TEXT

    s_model = ctor.GetModelHandle();
    s_registered = true;

    /* ⚡ Document is NOT shown at init — it will be lazily shown on the
       first rmlui_training_hud_set_text() call from Lua.  This avoids
       setting s_any_window_visible=true at boot, which would force the
       GL3 render pipeline to run every frame on Pi4. */

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Training HUD] Data model registered (effie_hud deferred)");
}

// -------------------------------------------------------------------
// Per-frame update — dirty check and push
// -------------------------------------------------------------------
extern "C" void rmlui_training_hud_update(void) {
    if (!s_registered || !s_model)
        return;

    // ⚡ Skip all 9 string comparisons when HUD is not shown
    if (!s_hud_visible)
        return;

#define DIRTY_TEXT(name)                                                                                               \
    if (s_fields.name != s_cache.name) {                                                                               \
        s_cache.name = s_fields.name;                                                                                  \
        s_model.DirtyVariable(#name);                                                                                  \
    }

    DIRTY_TEXT(p1_stun);
    DIRTY_TEXT(p2_stun);
    DIRTY_TEXT(p1_life);
    DIRTY_TEXT(p2_life);
    DIRTY_TEXT(p1_meter);
    DIRTY_TEXT(p2_meter);
    DIRTY_TEXT(p1_advantage);
    DIRTY_TEXT(p2_advantage);
    DIRTY_TEXT(advantage_class);
#undef DIRTY_TEXT
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_training_hud_shutdown(void) {
    if (s_hud_visible) {
        rmlui_wrapper_hide_document("effie_hud");
        s_hud_visible = false;
    }
    if (s_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("training_hud");
        s_registered = false;
    }
}
