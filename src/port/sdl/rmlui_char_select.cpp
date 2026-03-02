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

// ─── Super Art name table (SF3:3S roster, index matches My_char) ───
// 3 strings per character. Ordered same as s_char_names.
static const char* const s_sa_names[21][3] = {
    { "METEOR STRIKE", "SERAPHIC WING", "RESURRECTION" }, // GILL
    { "HYPER BOMB", "BOOMERANG RAID", "STUNGUN HEADSHOT" }, // ALEX
    { "SHINKUU-HADOU-KEN", "SHIN-SHOURYUU-KEN", "DENJIN-HADOU-KEN" }, // RYU
    { "YOUHOU", "SOU-RAI-REN", "GEN-EI-JIN" }, // YUN
    { "ROCKET UPPERCUT", "ROLLING THUNDER", "CORKSCREW BLOW" }, // DUDLEY
    { "MAGNETIC STORM", "SLAM DANCE", "ELECTRIC SNAKE" }, // NECRO
    { "GIGAS BREAKER", "MEGATON PRESS", "HAMMER MOUNTAIN" }, // HUGO
    { "KASUMI-SUZAKU", "YOROIDOSHI", "YAMI-SHIGURE" }, // IBUKI
    { "SPINNING BEAT", "BRAVE DANCE", "HEALING" }, // ELENA
    { "KISHIN-TSUI", "YAGYOU-DAMA", "TENGU-STONE" }, // ORO
    { "RAI-SHIN-MAHA-KEN", "TENSHIN-SENKYUU-TAI", "SEI-EI-ENBU" }, // YANG
    { "SHOURYUU-REPPA", "SHINRYUU-KEN", "SHIPPUU-JINRAI-KYAKU" }, // KEN
    { "HADOU-BURST", "SHOURYUU-CANNON", "HYPER TORNADO" }, // SEAN
    { "TYRANT SLAUGHTER", "TEMPORAL THUNDER", "AEGIS REFLECTOR" }, // URIEN
    { "MESSATSU-GOU-HADOU", "MESSATSU-GOU-SHOURYUU", "MESSATSU-GOU-RASEN" }, // GOUKI
    { "KIKOU-SHOU", "HOUYOKU-SEN", "TENSEI-RANKA" }, // CHUN-LI
    { "SEICHUSEN-GODANZUKI", "ABARE-TOSANAMI", "TANDEN-RENKI" }, // MAKOTO
    { "CRITICAL STRIKE", "DEADLY DOUBLE", "TOTAL DESTRUCTION" }, // Q
    { "X.N.D.L.", "X.F.L.A.T.", "X.C.O.P.Y." }, // TWELVE
    { "LIGHT OF VIRTUE", "SUPREME RISING", "BLUE NOCTURNE" }, // REMY
    { "KONGOU-KOKURETSU-ZAN", "MESSATSU-GOU-SHOURYUU", "MESSATSU-GOU-RASEN" } // AKUMA (Shin Gouki uses same SAs usually)
};

static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
bool rmlui_char_select_visible = false;

extern "C" void rmlui_char_select_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("char_select");
    if (!ctor)
        return;

    // Phase flag: true during selection, false during VS screen transition
    ctor.BindFunc("sel_phase_select", [](Rml::Variant& v) { v = (bool)(Exit_No == 0); });

    // Timer countdown — Select_Timer is BCD-encoded (0x30 = "30", 0x21 = "21")
    ctor.BindFunc("sel_timer", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String("");
        } else {
            int bcd = (int)Select_Timer;
            int tens = (bcd >> 4) & 0xF;
            int ones = bcd & 0xF;
            v = Rml::String(std::to_string(tens)) + Rml::String(std::to_string(ones));
        }
    });

    // Character names — read from cursor position through ID_of_Face grid
    ctor.BindFunc("sel_p1_name", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String("");
        } else {
            int char_id = ID_of_Face[Cursor_Y[0]][Cursor_X[0]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(char_name(char_id));
        }
    });
    ctor.BindFunc("sel_p2_name", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String("");
        } else {
            int char_id = ID_of_Face[Cursor_Y[1]][Cursor_X[1]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(char_name(char_id));
        }
    });

    // Player visibility flags — three scenarios
    // Select_Status[0] == 3 means both players; otherwise single player (Aborigine side)
    // Only bind single/dual when players are not in the exit sequence
    ctor.BindFunc("sel_p1_solo", [](Rml::Variant& v) {
        v = (bool)(plw[0].wu.pl_operator != 0 && plw[1].wu.pl_operator == 0 && Exit_No == 0);
    });
    ctor.BindFunc("sel_p2_solo", [](Rml::Variant& v) {
        v = (bool)(plw[0].wu.pl_operator == 0 && plw[1].wu.pl_operator != 0 && Exit_No == 0);
    });
    ctor.BindFunc("sel_both_active", [](Rml::Variant& v) {
        v = (bool)(plw[0].wu.pl_operator != 0 && plw[1].wu.pl_operator != 0 && Exit_No == 0);
    });
    
    ctor.BindFunc("sel_banner_visible", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = false;
            return;
        }
        bool is_p1_solo = (plw[0].wu.pl_operator != 0 && plw[1].wu.pl_operator == 0);
        bool is_p2_solo = (plw[0].wu.pl_operator == 0 && plw[1].wu.pl_operator != 0);
        
        if (is_p1_solo) {
            v = (bool)(Sel_PL_Complete[0] == 0);
        } else if (is_p2_solo) {
            v = (bool)(Sel_PL_Complete[1] == 0);
        } else {
            v = false;
        }
    });

    ctor.BindFunc("sel_p1_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[0] != 0); });
    ctor.BindFunc("sel_p2_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[1] != 0); });

    ctor.BindFunc("sel_p1_sa_visible", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            // During the VS screen transition, show if they completed selection
            v = (bool)(Sel_Arts_Complete[0] != 0);
        } else {
            // During select screen, show when they picked character but not finished art
            v = (bool)(Sel_PL_Complete[0] != 0 && Sel_Arts_Complete[0] == 0);
        }
    });
    ctor.BindFunc("sel_p1_sa_active", [](Rml::Variant& v) {
        // Active when they are still picking (not confirmed and not exiting)
        v = (bool)(Sel_Arts_Complete[0] == 0 && Exit_No == 0);
    });
    ctor.BindFunc("sel_p1_sa_current_name", [](Rml::Variant& v) {
        int char_idx = My_char[0];
        int sa_idx = Arts_Y[0];
        if (sa_idx < 0 || sa_idx > 2) sa_idx = 0;
        if (char_idx >= 0 && char_idx < CHAR_NAME_COUNT) v = Rml::String(s_sa_names[char_idx][sa_idx]);
        else v = Rml::String("SA " + std::to_string(sa_idx + 1));
    });
    ctor.BindFunc("sel_p1_sa_current_numeral", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[0];
        if (sa_idx == 0) v = Rml::String("I");
        else if (sa_idx == 1) v = Rml::String("II");
        else if (sa_idx == 2) v = Rml::String("III");
        else v = Rml::String("I");
    });

    ctor.BindFunc("sel_p2_sa_visible", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = (bool)(Sel_Arts_Complete[1] != 0);
        } else {
            v = (bool)(Sel_PL_Complete[1] != 0 && Sel_Arts_Complete[1] == 0);
        }
    });
    ctor.BindFunc("sel_p2_sa_active", [](Rml::Variant& v) {
        v = (bool)(Sel_Arts_Complete[1] == 0 && Exit_No == 0);
    });
    ctor.BindFunc("sel_p2_sa_current_name", [](Rml::Variant& v) {
        int char_idx = My_char[1];
        int sa_idx = Arts_Y[1];
        if (sa_idx < 0 || sa_idx > 2) sa_idx = 0;
        if (char_idx >= 0 && char_idx < CHAR_NAME_COUNT) v = Rml::String(s_sa_names[char_idx][sa_idx]);
        else v = Rml::String("SA " + std::to_string(sa_idx + 1));
    });
    ctor.BindFunc("sel_p2_sa_current_numeral", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[1];
        if (sa_idx == 0) v = Rml::String("I");
        else if (sa_idx == 1) v = Rml::String("II");
        else if (sa_idx == 2) v = Rml::String("III");
        else v = Rml::String("I");
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi CharSelect] Data model registered (6 bindings)");
}

extern "C" void rmlui_char_select_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    /* Auto-hide: if the overlay is visible but the game has left the char
     * select screen (Play_Game becomes non-zero when the fight starts),
     * hide the overlay.  This covers exit paths that don't call hide(). */
    if (rmlui_char_select_visible && Play_Game != 0) {
        rmlui_char_select_hide();
        return;
    }

    /* Detect external hides (e.g. rmlui_wrapper_hide_all_game_documents)
     * that bypass rmlui_char_select_hide() and don't reset our flag. */
    if (rmlui_char_select_visible &&
        !rmlui_wrapper_is_game_document_visible("char_select")) {
        rmlui_char_select_visible = false;
        return;
    }

    /* Only dirty-update bindings when the overlay is actually visible. */
    if (!rmlui_char_select_visible)
        return;

    // All bindings are BindFunc (evaluated each frame), just dirty them
    s_model_handle.DirtyVariable("sel_phase_select");
    s_model_handle.DirtyVariable("sel_timer");
    s_model_handle.DirtyVariable("sel_banner_visible");
    s_model_handle.DirtyVariable("sel_p1_name");
    s_model_handle.DirtyVariable("sel_p2_name");
    s_model_handle.DirtyVariable("sel_p1_solo");
    s_model_handle.DirtyVariable("sel_p2_solo");
    s_model_handle.DirtyVariable("sel_both_active");
    s_model_handle.DirtyVariable("sel_p1_confirmed");
    s_model_handle.DirtyVariable("sel_p2_confirmed");
    s_model_handle.DirtyVariable("sel_p1_sa_visible");
    s_model_handle.DirtyVariable("sel_p1_sa_active");
    s_model_handle.DirtyVariable("sel_p1_sa_current_name");
    s_model_handle.DirtyVariable("sel_p1_sa_current_numeral");
    s_model_handle.DirtyVariable("sel_p2_sa_visible");
    s_model_handle.DirtyVariable("sel_p2_sa_active");
    s_model_handle.DirtyVariable("sel_p2_sa_current_name");
    s_model_handle.DirtyVariable("sel_p2_sa_current_numeral");
}

extern "C" void rmlui_char_select_show(void) {
    rmlui_wrapper_show_game_document("char_select");
    rmlui_char_select_visible = true;
}

extern "C" void rmlui_char_select_hide(void) {
    rmlui_wrapper_hide_game_document("char_select");
    rmlui_char_select_visible = false;
}

extern "C" void rmlui_char_select_shutdown(void) {
    rmlui_char_select_visible = false;
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("char_select");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("char_select");
        s_model_registered = false;
    }
}
