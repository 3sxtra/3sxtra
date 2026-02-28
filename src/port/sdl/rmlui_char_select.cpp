/**
 * @file rmlui_char_select.cpp
 * @brief RmlUi Character Select overlay data model.
 *
 * Overlays text elements (timer, character names, SA labels) onto the
 * existing CPS3 character select sprite animations. Does NOT replace
 * the portrait sprites — only the text rendered via effect_* calls.
 */

#include "port/sdl/rmlui_char_select.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "structs.h"
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

extern "C" void rmlui_char_select_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("char_select");
    if (!ctor) return;

    // Timer countdown (Select_Timer global)
    ctor.BindFunc("sel_timer", [](Rml::Variant& v){
        v = (int)Select_Timer;
    });

    s_model_handle   = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi CharSelect] Data model registered");
}

extern "C" void rmlui_char_select_update(void) {
    if (!s_model_registered || !s_model_handle) return;
    // Timer updates every frame
    s_model_handle.DirtyVariable("sel_timer");
}

extern "C" void rmlui_char_select_show(void) {
    rmlui_wrapper_show_document("char_select");
}

extern "C" void rmlui_char_select_hide(void) {
    rmlui_wrapper_hide_document("char_select");
}

extern "C" void rmlui_char_select_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("char_select");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("char_select");
        s_model_registered = false;
    }
}
