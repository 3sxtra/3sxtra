/**
 * @file stun.c
 * Stun Gauge Controller
 */

#include "sf33rd/Source/Game/engine/stun.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include <stdbool.h>


/** @brief Initializes the stun gauge display state for both players. */
void stngauge_cont_init() {
    u8 i;

    for (i = 0; i < 2; i++) {
        g_state.sdat[i].cstn = 0;
        g_state.sdat[i].sflag = 0;
        g_state.sdat[i].osflag = 0;
        g_state.sdat[i].g_or_s = 0;
        g_state.sdat[i].stimer = 2;
        g_state.sdat[i].slen = (g_state.piyori_type[i].genkai / 8);
        g_state.sdat[i].proccess_dead = 0;

        if (omop_st_bar_disp[i]) {
            if (!use_rmlui || !rmlui_hud_stun)
                stun_base_put(i, g_state.sdat[i].slen);
        }
    }

    if (!use_rmlui || !rmlui_hud_stun)
        stun_gauge_waku_write(g_state.sdat[0].slen, g_state.sdat[1].slen);
}

/** @brief Per-frame stun gauge update — drives the animated stun bar display. */
void stngauge_cont_main() {
    u8 i;

    if (omop_cockpit != 0) {
        for (i = 0; i < 2; i++) {
            if (g_state.gauge_stop_flag[i] == 0) {
                stngauge_control(i);
            } else {
                if (!use_rmlui || !rmlui_hud_stun)
                    stun_put(i, g_state.sdat[i].cstn);
            }

            if (omop_st_bar_disp[i]) {
                if (!use_rmlui || !rmlui_hud_stun)
                    stun_base_put(i, g_state.sdat[i].slen);
            }
        }

        if (!use_rmlui || !rmlui_hud_stun)
            stun_gauge_waku_write(g_state.sdat[0].slen, g_state.sdat[1].slen);
    }
}

/** @brief Updates a single player's stun gauge animation and flash state. */
void stngauge_control(u8 pl) {
    if (!g_state.sdat[pl].proccess_dead) {
        if (g_state.plw[pl].dead_flag) {
            g_state.sdat[pl].proccess_dead = 1;
            g_state.sdat[pl].cstn = 0;
            return;
        }

        if (((g_state.plw[pl].wu.routine_no[1] == 1) && (g_state.plw[pl].wu.routine_no[2] == 0x19) &&
             (g_state.plw[pl].wu.routine_no[3] != 0)) ||
            (g_state.plw[pl].py->flag == 1)) {
            g_state.sdat[pl].sflag = 1;

            if (g_state.sdat[pl].osflag == 0) {
                g_state.sdat[pl].cstn = g_state.piyori_type[pl].genkai;
            }

            if (!g_state.EXE_flag && !g_state.Game_pause) {
                g_state.sdat[pl].stimer--;
            }

            if (g_state.sdat[pl].g_or_s == 0) {
                if (No_Trans == 0 && (!use_rmlui || !rmlui_hud_stun)) {
                    stun_mark_write(pl, g_state.sdat[pl].slen);
                    stun_put(pl, g_state.sdat[pl].cstn);
                }

                if (g_state.sdat[pl].stimer == 0) {
                    g_state.sdat[pl].g_or_s = 1;
                    g_state.sdat[pl].stimer = 2;
                }
            } else {
                if (No_Trans == 0 && (!use_rmlui || !rmlui_hud_stun)) {
                    stun_put(pl, g_state.sdat[pl].cstn);
                }

                if (g_state.sdat[pl].stimer == 0) {
                    g_state.sdat[pl].g_or_s = 0;
                    g_state.sdat[pl].stimer = 2;
                }
            }

            g_state.sdat[pl].osflag = g_state.sdat[pl].sflag;
            return;
        }

        g_state.sdat[pl].sflag = 0;

        if (g_state.sdat[pl].osflag == 1) {
            g_state.sdat[pl].osflag = g_state.sdat[pl].sflag;
            g_state.sdat[pl].g_or_s = 0;
            g_state.sdat[pl].stimer = 2;
            g_state.sdat[pl].cstn = g_state.plw[pl].py->now.quantity.h;
            g_state.sdat[pl].osflag = g_state.sdat[pl].sflag;

            if (No_Trans == 0) {
                stun_put(pl, g_state.sdat[pl].cstn);
            }
            return;
        }

        if (g_state.sdat[pl].cstn != g_state.plw[pl].py->now.quantity.h) {
            g_state.sdat[pl].cstn = g_state.plw[pl].py->now.quantity.h;
        }

        if (No_Trans == 0) {
            stun_put(pl, g_state.sdat[pl].cstn);
        }
    }
}

/** @brief Clears both players' stun gauge work and display. */
void stngauge_work_clear() {
    u8 i;
    for (i = 0; i < 2; i++) {
        g_state.sdat[i].cstn = 0;
        g_state.sdat[i].sflag = 0;
        g_state.sdat[i].osflag = 0;
        g_state.sdat[i].g_or_s = 0;
        g_state.sdat[i].stimer = 2;
        g_state.sdat[i].proccess_dead = 0;
        stun_put(i, 0);
    }
}
