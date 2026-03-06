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

// ─── Character portrait paths (SF3:3S roster, index matches My_char) ───
static const char* const s_portrait_paths[21] = {
    "",                                          // 0 GILL (no portrait)
    "assets/charactersportraits/alex.png",       // 1 ALEX
    "assets/charactersportraits/ryu.png",        // 2 RYU
    "assets/charactersportraits/yun.png",        // 3 YUN
    "assets/charactersportraits/dudley.png",     // 4 DUDLEY
    "assets/charactersportraits/necro.png",      // 5 NECRO
    "assets/charactersportraits/hugo.png",       // 6 HUGO
    "assets/charactersportraits/ibuki.png",      // 7 IBUKI
    "assets/charactersportraits/elena.png",      // 8 ELENA
    "assets/charactersportraits/oro.png",        // 9 ORO
    "assets/charactersportraits/yang.png",       // 10 YANG
    "assets/charactersportraits/ken.png",        // 11 KEN
    "assets/charactersportraits/sean.png",       // 12 SEAN
    "assets/charactersportraits/urien.png",      // 13 URIEN
    "assets/charactersportraits/akuma.png",      // 14 GOUKI
    "assets/charactersportraits/chunli.png",     // 15 CHUN-LI
    "assets/charactersportraits/makoto.png",     // 16 MAKOTO
    "assets/charactersportraits/q.png",          // 17 Q
    "assets/charactersportraits/twelve.png",     // 18 TWELVE
    "assets/charactersportraits/remy.png",       // 19 REMY
    "assets/charactersportraits/akuma.png",      // 20 AKUMA (Shin)
};

static const char* portrait_path(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_portrait_paths[idx];
    return "";
}

// ─── Super Art name table (SF3:3S roster, index matches My_char) ───
// 3 strings per character. Ordered same as s_char_names.
static const char* const s_sa_names[21][3] = {
    { "METEOR STRIKE", "SERAPHIC WING", "RESURRECTION" },                    // GILL
    { "HYPER BOMB", "BOOMERANG RAID", "STUNGUN HEADSHOT" },                  // ALEX
    { "SHINKUU-HADOU-KEN", "SHIN-SHOURYUU-KEN", "DENJIN-HADOU-KEN" },        // RYU
    { "YOUHOU", "SOU-RAI-REN", "GEN-EI-JIN" },                               // YUN
    { "ROCKET UPPERCUT", "ROLLING THUNDER", "CORKSCREW BLOW" },              // DUDLEY
    { "MAGNETIC STORM", "SLAM DANCE", "ELECTRIC SNAKE" },                    // NECRO
    { "GIGAS BREAKER", "MEGATON PRESS", "HAMMER MOUNTAIN" },                 // HUGO
    { "KASUMI-SUZAKU", "YOROIDOSHI", "YAMI-SHIGURE" },                       // IBUKI
    { "SPINNING BEAT", "BRAVE DANCE", "HEALING" },                           // ELENA
    { "KISHIN-TSUI", "YAGYOU-DAMA", "TENGU-STONE" },                         // ORO
    { "RAI-SHIN-MAHA-KEN", "TENSHIN-SENKYUU-TAI", "SEI-EI-ENBU" },           // YANG
    { "SHOURYUU-REPPA", "SHINRYUU-KEN", "SHIPPUU-JINRAI-KYAKU" },            // KEN
    { "HADOU-BURST", "SHOURYUU-CANNON", "HYPER TORNADO" },                   // SEAN
    { "TYRANT SLAUGHTER", "TEMPORAL THUNDER", "AEGIS REFLECTOR" },           // URIEN
    { "MESSATSU-GOU-HADOU", "MESSATSU-GOU-SHOURYUU", "MESSATSU-GOU-RASEN" }, // GOUKI
    { "KIKOU-SHOU", "HOUYOKU-SEN", "TENSEI-RANKA" },                         // CHUN-LI
    { "SEICHUSEN-GODANZUKI", "ABARE-TOSANAMI", "TANDEN-RENKI" },             // MAKOTO
    { "CRITICAL STRIKE", "DEADLY DOUBLE", "TOTAL DESTRUCTION" },             // Q
    { "X.N.D.L.", "X.F.L.A.T.", "X.C.O.P.Y." },                              // TWELVE
    { "LIGHT OF VIRTUE", "SUPREME RISING", "BLUE NOCTURNE" },                // REMY
    { "KONGOU-KOKURETSU-ZAN",
      "MESSATSU-GOU-SHOURYUU",
      "MESSATSU-GOU-RASEN" } // AKUMA (Shin Gouki uses same SAs usually)
};

static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}

// ─── Stage select: country names indexed by character/stage ID ───
static const char* const s_country_names[22] = { "GREECE",  "U.S.A.", "JAPAN", "CHINA",  "ENGLAND", "RUSSIA",
                                                 "GERMANY", "JAPAN",  "KENYA", "BRAZIL", "CHINA",   "U.S.A.",
                                                 "BRAZIL",  "GREECE", "JAPAN", "CHINA",  "JAPAN",   "???",
                                                 "???",     "FRANCE", "JAPAN", "BRAZIL" };
static const char* const s_flag_paths[22] = { "../flags/gr.png",
                                              "../flags/us.png",
                                              "../flags/jp.png",
                                              "../flags/cn.png",
                                              "../flags/gb.png",
                                              "../flags/ru.png",
                                              "../flags/de.png",
                                              "../flags/jp.png",
                                              "../flags/ke.png",
                                              "../flags/br.png",
                                              "../flags/cn.png",
                                              "../flags/us.png",
                                              "../flags/br.png",
                                              "../flags/gr.png",
                                              "../flags/jp.png",
                                              "../flags/cn.png",
                                              "../flags/jp.png",
                                              "",
                                              "",
                                              "../flags/fr.png",
                                              "../flags/jp.png",
                                              "../flags/br.png" };
static const char* const s_stage_labels[11] = { "FIRST STAGE", "2ND STAGE",  "3RD STAGE",  "4TH STAGE",
                                                "5TH STAGE",   "6TH STAGE",  "7TH STAGE",  "8TH STAGE",
                                                "9TH STAGE",   "10TH STAGE", "FINAL STAGE" };
#define COUNTRY_COUNT 22
#define STAGE_LABEL_COUNT 11

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

    // VS screen active flag: true when VS screen is displaying (Exit_4th spawns VS objects)
    ctor.BindFunc("vs_active", [](Rml::Variant& v) { v = (bool)(Exit_No >= 4); });

    // Timer countdown — Select_Timer is BCD-encoded (0x30 = "30", 0x21 = "21")
    // Always decode — char-select and stage-select timers are mutually exclusive
    // via their own data-if conditions (sel_timer_visible vs stg_visible).
    ctor.BindFunc("sel_timer", [](Rml::Variant& v) {
        int bcd = (int)Select_Timer;
        int tens = (bcd >> 4) & 0xF;
        int ones = bcd & 0xF;
        v = Rml::String(std::to_string(tens)) + Rml::String(std::to_string(ones));
    });

    // Character names — read from cursor position through ID_of_Face grid
    ctor.BindFunc("sel_p1_name", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String(char_name(My_char[0]));
        } else {
            int char_id = ID_of_Face[Cursor_Y[0]][Cursor_X[0]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(char_name(char_id));
        }
    });
    ctor.BindFunc("sel_p2_name", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String(char_name(My_char[1]));
        } else {
            int char_id = ID_of_Face[Cursor_Y[1]][Cursor_X[1]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(char_name(char_id));
        }
    });

    // Character portraits
    ctor.BindFunc("sel_p1_portrait", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String(portrait_path(My_char[0]));
        } else {
            int char_id = ID_of_Face[Cursor_Y[0]][Cursor_X[0]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(portrait_path(char_id));
        }
    });
    ctor.BindFunc("sel_p2_portrait", [](Rml::Variant& v) {
        if (Exit_No >= 1) {
            v = Rml::String(portrait_path(My_char[1]));
        } else {
            int char_id = ID_of_Face[Cursor_Y[1]][Cursor_X[1]];
            if (char_id < 0)
                char_id = 0;
            v = Rml::String(portrait_path(char_id));
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

    ctor.BindFunc("sel_sa_banner_visible", [](Rml::Variant& v) {
        bool p1_picking = (Sel_PL_Complete[0] != 0 && Sel_Arts_Complete[0] == 0 && Exit_No == 0);
        bool p2_picking = (Sel_PL_Complete[1] != 0 && Sel_Arts_Complete[1] == 0 && Exit_No == 0);
        v = (bool)(p1_picking || p2_picking);
    });

    // Global timer visibility flag — hide during VS screen transitions (Exit_No > 0)
    // AND hide during stage select transition (Sel_EM_Complete == 1)
    ctor.BindFunc("sel_timer_visible", [](Rml::Variant& v) {
        if (Exit_No != 0) {
            v = false;
        } else if (G_No[1] == 5) { // Stage Select Screen
            v = (bool)(Sel_EM_Complete[Player_id] == 0);
        } else {
            v = true;
        }
    });

    ctor.BindFunc("sel_p1_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[0] != 0); });
    ctor.BindFunc("sel_p2_confirmed", [](Rml::Variant& v) { v = (bool)(Sel_PL_Complete[1] != 0); });

    ctor.BindFunc("sel_p1_sa_visible", [](Rml::Variant& v) {
        // Visible when player has picked a character but hasn't finished the SA pick
        v = (bool)(Sel_PL_Complete[0] != 0 && Sel_Arts_Complete[0] == 0);
    });
    ctor.BindFunc("sel_p1_sa_active", [](Rml::Variant& v) {
        // Active when they are still picking (not confirmed and not exiting)
        v = (bool)(Sel_Arts_Complete[0] == 0 && Exit_No == 0);
    });
    ctor.BindFunc("sel_p1_sa_current_name", [](Rml::Variant& v) {
        int char_idx = My_char[0];
        int sa_idx = Arts_Y[0];
        if (sa_idx < 0 || sa_idx > 2)
            sa_idx = 0;
        if (char_idx >= 0 && char_idx < CHAR_NAME_COUNT)
            v = Rml::String(s_sa_names[char_idx][sa_idx]);
        else
            v = Rml::String("SA " + std::to_string(sa_idx + 1));
    });
    ctor.BindFunc("sel_p1_sa_current_numeral", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[0];
        if (sa_idx == 0)
            v = Rml::String("I");
        else if (sa_idx == 1)
            v = Rml::String("II");
        else if (sa_idx == 2)
            v = Rml::String("III");
        else
            v = Rml::String("I");
    });
    ctor.BindFunc("sel_p1_sa_index", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[0];
        if (sa_idx < 0 || sa_idx > 2)
            sa_idx = 0;
        v = sa_idx;
    });

    ctor.BindFunc("sel_p2_sa_visible", [](Rml::Variant& v) {
        // Visible when player has picked a character but hasn't finished the SA pick
        v = (bool)(Sel_PL_Complete[1] != 0 && Sel_Arts_Complete[1] == 0);
    });
    ctor.BindFunc("sel_p2_sa_active", [](Rml::Variant& v) { v = (bool)(Sel_Arts_Complete[1] == 0 && Exit_No == 0); });
    ctor.BindFunc("sel_p2_sa_current_name", [](Rml::Variant& v) {
        int char_idx = My_char[1];
        int sa_idx = Arts_Y[1];
        if (sa_idx < 0 || sa_idx > 2)
            sa_idx = 0;
        if (char_idx >= 0 && char_idx < CHAR_NAME_COUNT)
            v = Rml::String(s_sa_names[char_idx][sa_idx]);
        else
            v = Rml::String("SA " + std::to_string(sa_idx + 1));
    });
    ctor.BindFunc("sel_p2_sa_current_numeral", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[1];
        if (sa_idx == 0)
            v = Rml::String("I");
        else if (sa_idx == 1)
            v = Rml::String("II");
        else if (sa_idx == 2)
            v = Rml::String("III");
        else
            v = Rml::String("I");
    });
    ctor.BindFunc("sel_p2_sa_index", [](Rml::Variant& v) {
        int sa_idx = Arts_Y[1];
        if (sa_idx < 0 || sa_idx > 2)
            sa_idx = 0;
        v = sa_idx;
    });

    // ─── Stage select bindings ───
    // stg_visible: show when player has exited char select (Exit_No becomes non-zero)
    // but before they confirm EM. By this point Setup_EM_List() has already run.
    ctor.BindFunc("stg_visible", [](Rml::Variant& v) {
        // No stage select phase in 2P vs mode — hide entirely when both sides are human
        bool is_2p_vs = (plw[0].wu.pl_operator != 0 && plw[1].wu.pl_operator != 0);
        v = (bool)(!is_2p_vs && Exit_No != 0 && Sel_EM_Complete[Player_id] == 0);
    });
    ctor.BindFunc("stg_stage_label", [](Rml::Variant& v) {
        int idx = VS_Index[Player_id];
        if (idx < 0)
            idx = 0;
        if (idx >= STAGE_LABEL_COUNT)
            idx = STAGE_LABEL_COUNT - 1;
        v = Rml::String(s_stage_labels[idx]);
    });
    ctor.BindFunc("stg_em1_country", [](Rml::Variant& v) {
        int id = EM_List[Player_id][0];
        v = Rml::String((id >= 0 && id < COUNTRY_COUNT) ? s_country_names[id] : "???");
    });
    ctor.BindFunc("stg_em2_country", [](Rml::Variant& v) {
        int id = EM_List[Player_id][1];
        v = Rml::String((id >= 0 && id < COUNTRY_COUNT) ? s_country_names[id] : "???");
    });
    ctor.BindFunc("stg_em1_flag", [](Rml::Variant& v) {
        int id = EM_List[Player_id][0];
        v = Rml::String((id >= 0 && id < COUNTRY_COUNT) ? s_flag_paths[id] : "");
    });
    ctor.BindFunc("stg_em2_flag", [](Rml::Variant& v) {
        int id = EM_List[Player_id][1];
        v = Rml::String((id >= 0 && id < COUNTRY_COUNT) ? s_flag_paths[id] : "");
    });
    ctor.BindFunc("stg_em1_name", [](Rml::Variant& v) {
        int id = EM_List[Player_id][0];
        v = Rml::String(char_name(id));
    });
    ctor.BindFunc("stg_em2_name", [](Rml::Variant& v) {
        int id = EM_List[Player_id][1];
        v = Rml::String(char_name(id));
    });
    ctor.BindFunc("stg_sel_top", [](Rml::Variant& v) { v = (bool)(Exit_No != 0 && Temporary_EM[Player_id] == 1); });
    ctor.BindFunc("stg_sel_bot", [](Rml::Variant& v) { v = (bool)(Exit_No != 0 && Temporary_EM[Player_id] == 2); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi CharSelect] Data model registered");
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

    /* NOTE: We no longer detect external hides via rmlui_wrapper_is_game_document_visible().
     * That check was causing rmlui_char_select_visible to be reset to false prematurely
     * during the VS screen transition, ungating native effects like eff79/eff80 SA plates.
     * The flag is now only cleared through rmlui_char_select_hide() or Play_Game != 0. */

    /* Only dirty-update bindings when the overlay is actually visible. */
    if (!rmlui_char_select_visible)
        return;

    // All bindings are BindFunc (evaluated each frame), just dirty them
    s_model_handle.DirtyVariable("sel_phase_select");
    s_model_handle.DirtyVariable("vs_active");
    s_model_handle.DirtyVariable("sel_timer_visible");
    s_model_handle.DirtyVariable("sel_timer");
    s_model_handle.DirtyVariable("sel_banner_visible");
    s_model_handle.DirtyVariable("sel_sa_banner_visible");
    s_model_handle.DirtyVariable("sel_p1_name");
    s_model_handle.DirtyVariable("sel_p2_name");
    s_model_handle.DirtyVariable("sel_p1_portrait");
    s_model_handle.DirtyVariable("sel_p2_portrait");
    s_model_handle.DirtyVariable("sel_p1_solo");
    s_model_handle.DirtyVariable("sel_p2_solo");
    s_model_handle.DirtyVariable("sel_both_active");
    s_model_handle.DirtyVariable("sel_p1_confirmed");
    s_model_handle.DirtyVariable("sel_p2_confirmed");
    s_model_handle.DirtyVariable("sel_p1_sa_visible");
    s_model_handle.DirtyVariable("sel_p1_sa_active");
    s_model_handle.DirtyVariable("sel_p1_sa_current_name");
    s_model_handle.DirtyVariable("sel_p1_sa_current_numeral");
    s_model_handle.DirtyVariable("sel_p1_sa_index");
    s_model_handle.DirtyVariable("sel_p2_sa_visible");
    s_model_handle.DirtyVariable("sel_p2_sa_active");
    s_model_handle.DirtyVariable("sel_p2_sa_current_name");
    s_model_handle.DirtyVariable("sel_p2_sa_current_numeral");
    s_model_handle.DirtyVariable("sel_p2_sa_index");
    s_model_handle.DirtyVariable("stg_visible");
    s_model_handle.DirtyVariable("stg_stage_label");
    s_model_handle.DirtyVariable("stg_em1_country");
    s_model_handle.DirtyVariable("stg_em2_country");
    s_model_handle.DirtyVariable("stg_em1_flag");
    s_model_handle.DirtyVariable("stg_em2_flag");
    s_model_handle.DirtyVariable("stg_em1_name");
    s_model_handle.DirtyVariable("stg_em2_name");
    s_model_handle.DirtyVariable("stg_sel_top");
    s_model_handle.DirtyVariable("stg_sel_bot");
}

extern "C" void rmlui_char_select_show(void) {
    rmlui_wrapper_show_game_document("char_select");
    rmlui_char_select_visible = true;
    /* Force-dirty vs_active so it evaluates correctly on first frame */
    if (s_model_registered && s_model_handle)
        s_model_handle.DirtyVariable("vs_active");
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
