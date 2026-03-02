/**
 * @file rmlui_char_select.cpp
 * @brief RmlUi Character Select overlay data model.
 *
 * Overlays text elements (timer, character names) onto the existing CPS3
 * character select sprite animations. Suppression of native sprites is
 * done render-side in eff38.c / eff42.c via the rmlui_screen_select toggle.
 */

#include "port/sdl/rmlui_char_select.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "structs.h"
} // extern "C"

// ─── Character name table (SF3:3S roster, index matches My_char) ───
static const char* const s_char_names[21] = { "GILL",  "ALEX",    "RYU",    "YUN",  "DUDLEY", "NECRO", "HUGO",
                                              "IBUKI", "ELENA",   "ORO",    "YANG", "KEN",    "SEAN",  "URIEN",
                                              "GOUKI", "CHUN-LI", "MAKOTO", "Q",    "TWELVE", "REMY",  "AKUMA" };
#define CHAR_NAME_COUNT 21

static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

extern "C" void rmlui_char_select_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("char_select");
    if (!ctor)
        return;

    // Timer countdown (Select_Timer global)
    ctor.BindFunc("sel_timer", [](Rml::Variant& v) { v = (int)Select_Timer; });

    // Character names — read from cursor position through ID_of_Face grid
    ctor.BindFunc("sel_p1_name", [](Rml::Variant& v) {
        int char_id = ID_of_Face[Cursor_Y[0]][Cursor_X[0]];
        if (char_id < 0)
            char_id = 0;
        v = Rml::String(char_name(char_id));
    });
    ctor.BindFunc("sel_p2_name", [](Rml::Variant& v) {
        int char_id = ID_of_Face[Cursor_Y[1]][Cursor_X[1]];
        if (char_id < 0)
            char_id = 0;
        v = Rml::String(char_name(char_id));
    });

    // State flags
    ctor.BindFunc("sel_is_2p", [](Rml::Variant& v) { v = (bool)(Play_Type == 1); });
    ctor.BindFunc("sel_p1_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[0] != 0); });
    ctor.BindFunc("sel_p2_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[1] != 0); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi CharSelect] Data model registered (6 bindings)");
}

extern "C" void rmlui_char_select_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    // All bindings are BindFunc (evaluated each frame), just dirty them
    s_model_handle.DirtyVariable("sel_timer");
    s_model_handle.DirtyVariable("sel_p1_name");
    s_model_handle.DirtyVariable("sel_p2_name");
    s_model_handle.DirtyVariable("sel_is_2p");
    s_model_handle.DirtyVariable("sel_p1_confirmed");
    s_model_handle.DirtyVariable("sel_p2_confirmed");
}

extern "C" void rmlui_char_select_show(void) {
    rmlui_wrapper_show_game_document("char_select");
}

extern "C" void rmlui_char_select_hide(void) {
    rmlui_wrapper_hide_game_document("char_select");
}

extern "C" void rmlui_char_select_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("char_select");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("char_select");
        s_model_registered = false;
    }
}
