/**
 * @file vital.c
 * Vitality Bars
 */

#include "sf33rd/Source/Game/engine/vital.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/system_director.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include <stdbool.h>

/** @brief Initializes the vitality bar display state for both players. */
void vital_cont_init() {
    u8 i;

    for (i = 0; i < 2; i++) {
        g_state.vit[i].cyerw = 0xA0;
        g_state.vit[i].cred = 0xA0;
        g_state.vit[i].ored = 0xA0;
        g_state.vit[i].colnum = 1;
        g_state.gauge_stop_flag[i] = 0;
        g_state.vital_stop_flag[i] = 0;
    }
}

/** @brief Per-frame vitality bar update — drives the animated health bar drain. */
void vital_cont_main() {
    if (omop_cockpit != 0) {
        if (!g_state.execute_flag && !g_state.Game_pause) {
            if (g_state.vital_stop_flag[0] == 0 && g_state.gauge_stop_flag[0] == 0) {
                vital_control(0);
            }

            if (g_state.vital_stop_flag[1] == 0 && g_state.gauge_stop_flag[1] == 0) {
                vital_control(1);
            }
        } else {
            if (!use_rmlui || !rmlui_hud_health) {
                vital_parts_allwrite(0);
                vital_parts_allwrite(1);
            }
        }
    }
}

/** @brief Updates a single player's vitality bar animation and color state. */
void vital_control(u8 pl) {
    if (g_state.plw[pl].wu.vital_new < 0xA1) {
        if ((g_state.vit[pl].cyerw == g_state.plw[pl].wu.vital_new) &&
            (g_state.vit[pl].cred == g_state.plw[pl].wu.vital_new) &&
            (g_state.vit[pl].ored != (g_state.plw[pl].wu.vital_new + 1))) {
            if (No_Trans == 0) {
                if (!use_rmlui || !rmlui_hud_health)
                    vital_parts_allwrite(pl);
            }
            return;
        }

        if (g_state.vit[pl].cred < g_state.plw[pl].wu.vital_new) {
            g_state.vit[pl].cred = g_state.plw[pl].wu.vital_new;
        }

        g_state.vit[pl].cyerw = g_state.plw[pl].wu.vital_new;

        if (g_state.plw[pl].wu.vital_new < 0) {
            g_state.vit[pl].cyerw = 0;
        }

        if (g_state.plw[pl].wu.vital_new == 0xA0) {
            g_state.vit[pl].colnum = 1;
        } else if (g_state.plw[pl].wu.vital_new < 0x31) {
            g_state.vit[pl].colnum = 3;
        } else {
            g_state.vit[pl].colnum = 2;
        }

        if (No_Trans == 0) {
            if (!use_rmlui || !rmlui_hud_health)
                vital_parts_allwrite(pl);
        }

        g_state.vit[pl].ored = g_state.vit[pl].cred;
        g_state.vit[pl].cred--;

        if (g_state.vit[pl].cred < g_state.plw[pl].wu.vital_new) {
            g_state.vit[pl].cred = g_state.plw[pl].wu.vital_new;
        }
    }
}

/** @brief Writes all vitality bar sprite parts for one player to the render queue. */
void vital_parts_allwrite(u8 Pl_Num) {
    scfont_sqput((Pl_Num * 27), 2, 1, 0, Pl_Num, (Pl_Num + 30), 21, 1, TopHUDVitalPriority);

    if (omop_vt_bar_disp[Pl_Num] == 0) {
        silver_vital_put(Pl_Num);
        return;
    }

    vital_put(Pl_Num, g_state.vit[Pl_Num].colnum, g_state.vit[Pl_Num].cyerw, 0, TopHUDPriority);
    vital_put(Pl_Num, 1, g_state.vit[Pl_Num].cred, 1, TopHUDShadowPriority);
    vital_base_put(Pl_Num);
}
