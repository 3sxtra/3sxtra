/**
 * @file rmlui_sound_menu.cpp
 * @brief RmlUi Sound Test / Screen Adjust data model.
 *
 * Replaces CPS3's effect_57/61/64/A8 objects in Sound_Test() case 0
 * with an RmlUi overlay showing sound mode, BGM/SE levels, BGM type,
 * and sound test controls.
 *
 * Key globals (from workuser.h):
 *   Convert_Buff[3][1][0..7], bgm_level, se_level, sys_w.bgm_type,
 *   Menu_Cursor_Y[]
 */

#include "port/sdl/rmlui_sound_menu.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct SoundCache {
    int  sound_mode;    // 0=stereo, 1=mono
    int  bgm_level;
    int  se_level;
    int  bgm_type;      // 0=arranged, 1=original
    int  cursor_y;
};
static SoundCache s_cache = {};

#define DIRTY_INT(nm, expr) do { \
    int _v = (expr); \
    if (_v != s_cache.nm) { s_cache.nm = _v; s_model_handle.DirtyVariable(#nm); } \
} while(0)

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_sound_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("sound_menu");
    if (!ctor) return;

    ctor.BindFunc("sound_mode", [](Rml::Variant& v){
        v = (int)Convert_Buff[3][1][0];
    });
    ctor.BindFunc("bgm_level", [](Rml::Variant& v){
        v = (int)bgm_level;
    });
    ctor.BindFunc("se_level", [](Rml::Variant& v){
        v = (int)se_level;
    });
    ctor.BindFunc("bgm_type", [](Rml::Variant& v){
        v = (int)sys_w.bgm_type;
    });
    ctor.BindFunc("cursor_y", [](Rml::Variant& v){
        v = (int)Menu_Cursor_Y[0];
    });

    s_model_handle   = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi SoundMenu] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_sound_menu_update(void) {
    if (!s_model_registered || !s_model_handle) return;

    DIRTY_INT(sound_mode, (int)Convert_Buff[3][1][0]);
    DIRTY_INT(bgm_level, (int)bgm_level);
    DIRTY_INT(se_level, (int)se_level);
    DIRTY_INT(bgm_type, (int)sys_w.bgm_type);
    DIRTY_INT(cursor_y, (int)Menu_Cursor_Y[0]);
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_sound_menu_show(void) {
    rmlui_wrapper_show_document("sound_menu");
}

extern "C" void rmlui_sound_menu_hide(void) {
    rmlui_wrapper_hide_document("sound_menu");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_sound_menu_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("sound_menu");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("sound_menu");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
