/**
 * @file rmlui_vs_screen.cpp
 * @brief RmlUi VS Screen overlay data model.
 *
 * Overlays text elements (P1/P2 character names, "VS" label)
 * onto the existing CPS3 VS screen sprite animations.
 */

#include "port/sdl/rmlui_vs_screen.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
} // extern "C"

// SF3:3S character roster (indexed by char_no)
static const char* s_char_names[] = { "ALEX",  "YURI",   "RYU",    "KEN",    "SEAN",  "GOUKI", "ORO",
                                      "IBUKI", "MAKOTO", "ELENA",  "DUDLEY", "NECRO", "HUGO",  "URIEN",
                                      "REMY",  "Q",      "CHUNLI", "TWELVE", "YANG",  "GILL" };
static const int s_char_count = sizeof(s_char_names) / sizeof(s_char_names[0]);

static const char* get_char_name(int idx) {
    if (idx >= 0 && idx < s_char_count)
        return s_char_names[idx];
    return "???";
}

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

extern "C" void rmlui_vs_screen_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("vs_screen");
    if (!ctor)
        return;

    ctor.BindFunc("vs_p1_name", [](Rml::Variant& v) { v = Rml::String(get_char_name((int)plw[0].wu.char_index)); });
    ctor.BindFunc("vs_p2_name", [](Rml::Variant& v) { v = Rml::String(get_char_name((int)plw[1].wu.char_index)); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi VSScreen] Data model registered");
}

extern "C" void rmlui_vs_screen_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
}

extern "C" void rmlui_vs_screen_show(void) {
    rmlui_wrapper_show_document("vs_screen");
    if (s_model_handle) {
        s_model_handle.DirtyVariable("vs_p1_name");
        s_model_handle.DirtyVariable("vs_p2_name");
    }
}

extern "C" void rmlui_vs_screen_hide(void) {
    rmlui_wrapper_hide_document("vs_screen");
}

extern "C" void rmlui_vs_screen_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("vs_screen");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("vs_screen");
        s_model_registered = false;
    }
}
