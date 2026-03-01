/**
 * @file rmlui_copyright.cpp
 * @brief RmlUi copyright text overlay data model.
 *
 * Replaces Disp_Copyright() which renders Capcom copyright text
 * using SSPutStrPro. Reads the Country variable to pick the right
 * copyright string variant.
 */

#include "port/sdl/rmlui_copyright.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct CopyrightCache {
    int country;
};
static CopyrightCache s_cache = { -1 };

extern "C" void rmlui_copyright_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("copyright");
    if (!ctor)
        return;

    ctor.BindFunc("copyright_line1", [](Rml::Variant& v) {
        switch (Country) {
        case 1:
        case 2:
        case 3:
        case 7:
        case 8:
            v = Rml::String("\xC2\xA9 CAPCOM CO., LTD. 1999, 2004 ALL RIGHTS RESERVED.");
            break;
        case 4:
        case 5:
        case 6:
            v = Rml::String("\xC2\xA9 CAPCOM CO., LTD. 1999, 2004,");
            break;
        default:
            v = Rml::String("");
            break;
        }
    });
    ctor.BindFunc("copyright_line2", [](Rml::Variant& v) {
        switch (Country) {
        case 4:
        case 5:
        case 6:
            v = Rml::String("\xC2\xA9 CAPCOM U.S.A., INC. 1999, 2004 ALL RIGHTS RESERVED.");
            break;
        default:
            v = Rml::String("");
            break;
        }
    });
    ctor.BindFunc("copyright_visible",
                  [](Rml::Variant& v) { v = (bool)(Country >= 1 && Country <= 8 && Country != 0); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    rmlui_wrapper_show_game_document("copyright");
    SDL_Log("[RmlUi Copyright] Data model registered");
}

extern "C" void rmlui_copyright_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    int c = (int)Country;
    if (c != s_cache.country) {
        s_cache.country = c;
        s_model_handle.DirtyVariable("copyright_line1");
        s_model_handle.DirtyVariable("copyright_line2");
        s_model_handle.DirtyVariable("copyright_visible");
    }
}

extern "C" void rmlui_copyright_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("copyright");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("copyright");
        s_model_registered = false;
    }
}
