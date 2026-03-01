/**
 * @file rmlui_name_entry.cpp
 * @brief RmlUi name-entry / ranking screen data model.
 *
 * Replaces the CPS3 rendered name entry grid (SSPutDec, naming_set,
 * scfont_sqput) with an RmlUi overlay showing the 3-character entry
 * grid, cursor position, and ranking label.
 *
 * Key globals: name_wk[], Name_Input_f, naming_cnt[], Name_00[],
 *              Rank_In[][], E_Number[][]
 */

#include "port/sdl/rmlui_name_entry.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/n_input.h"
#include "sf33rd/Source/Game/system/work_sys.h"
} // extern "C"

// Character table for display (matches name_code_tbl indices in n_input.c)
static const char s_name_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789. <END";

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct NameEntryCache {
    bool active;
    int pl_id;
    int cursor_index;
    int char_codes[4];
    int rank_in;
};
static NameEntryCache s_cache = {};

#define NEDIRTY(nm) s_model_handle.DirtyVariable(#nm)

static char char_for_code(int code) {
    if (code >= 0 && code < 33)
        return s_name_chars[code];
    if (code == 44)
        return ' '; // space/blank
    if (code == 45)
        return '<'; // backspace
    if (code == 46)
        return '>'; // END
    return '?';
}

extern "C" void rmlui_name_entry_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("name_entry");
    if (!ctor)
        return;

    ctor.BindFunc("ne_active", [](Rml::Variant& v) {
        // Active when either player is in name entry state (E_Number[pl][0] == 2)
        v = (bool)(E_Number[0][0] == 2 || E_Number[1][0] == 2);
    });
    ctor.BindFunc("ne_rank", [](Rml::Variant& v) {
        int pl = (E_Number[0][0] == 2) ? 0 : 1;
        v = (int)(name_wk[pl].rank_in + 1);
    });
    ctor.BindFunc("ne_char0", [](Rml::Variant& v) {
        int pl = (E_Number[0][0] == 2) ? 0 : 1;
        char buf[2] = { char_for_code(name_wk[pl].code[0]), '\0' };
        v = Rml::String(buf);
    });
    ctor.BindFunc("ne_char1", [](Rml::Variant& v) {
        int pl = (E_Number[0][0] == 2) ? 0 : 1;
        char buf[2] = { char_for_code(name_wk[pl].code[1]), '\0' };
        v = Rml::String(buf);
    });
    ctor.BindFunc("ne_char2", [](Rml::Variant& v) {
        int pl = (E_Number[0][0] == 2) ? 0 : 1;
        char buf[2] = { char_for_code(name_wk[pl].code[2]), '\0' };
        v = Rml::String(buf);
    });
    ctor.BindFunc("ne_cursor", [](Rml::Variant& v) {
        int pl = (E_Number[0][0] == 2) ? 0 : 1;
        v = (int)name_wk[pl].index;
    });
    ctor.BindFunc("ne_player", [](Rml::Variant& v) { v = (int)((E_Number[0][0] == 2) ? 1 : 2); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi NameEntry] Data model registered");
}

extern "C" void rmlui_name_entry_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    bool active = (E_Number[0][0] == 2 || E_Number[1][0] == 2);
    if (active != s_cache.active) {
        s_cache.active = active;
        NEDIRTY(ne_active);
        if (active)
            rmlui_wrapper_show_game_document("name_entry");
        else
            rmlui_wrapper_hide_game_document("name_entry");
    }

    if (!active)
        return;

    int pl = (E_Number[0][0] == 2) ? 0 : 1;
    if (pl != s_cache.pl_id) {
        s_cache.pl_id = pl;
        NEDIRTY(ne_player);
        NEDIRTY(ne_rank);
    }

    int ci = (int)name_wk[pl].index;
    if (ci != s_cache.cursor_index) {
        s_cache.cursor_index = ci;
        NEDIRTY(ne_cursor);
    }

    // Always dirty the character codes (they change rapidly)
    NEDIRTY(ne_char0);
    NEDIRTY(ne_char1);
    NEDIRTY(ne_char2);
    NEDIRTY(ne_rank);
}

#undef NEDIRTY

extern "C" void rmlui_name_entry_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("name_entry");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("name_entry");
        s_model_registered = false;
    }
}
