/**
 * @file stun.c
 * Stun Gauge Controller
 */

#include "sf33rd/Source/Game/engine/stun.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/* Phase 3 RmlUi bypass */
#include <stdbool.h>
#include "port/sdl/rmlui_phase3_toggles.h"
extern bool use_rmlui;

SDAT sdat[2];

/** @brief Initializes the stun gauge display state for both players. */
void stngauge_cont_init() {
    u8 i;

    for (i = 0; i < 2; i++) {
        sdat[i].cstn = 0;
        sdat[i].sflag = 0;
        sdat[i].osflag = 0;
        sdat[i].g_or_s = 0;
        sdat[i].stimer = 2;
        sdat[i].slen = (piyori_type[i].genkai / 8);
        sdat[i].proccess_dead = 0;

        if (omop_st_bar_disp[i]) {
            if (!use_rmlui || !rmlui_hud_stun)
                stun_base_put(i, sdat[i].slen);
        }
    }

    if (!use_rmlui || !rmlui_hud_stun)
        stun_gauge_waku_write(sdat[0].slen, sdat[1].slen);
}

/** @brief Per-frame stun gauge update — drives the animated stun bar display. */
void stngauge_cont_main() {
    u8 i;

    if (omop_cockpit != 0) {
        for (i = 0; i < 2; i++) {
            if (gauge_stop_flag[i] == 0) {
                stngauge_control(i);
            } else {
                if (!use_rmlui || !rmlui_hud_stun)
                    stun_put(i, sdat[i].cstn);
            }

            if (omop_st_bar_disp[i]) {
                if (!use_rmlui || !rmlui_hud_stun)
                    stun_base_put(i, sdat[i].slen);
            }
        }

        if (!use_rmlui || !rmlui_hud_stun)
            stun_gauge_waku_write(sdat[0].slen, sdat[1].slen);
    }
}

/** @brief Updates a single player's stun gauge animation and flash state. */
void stngauge_control(u8 pl) {
    if (!sdat[pl].proccess_dead) {
        if (plw[pl].dead_flag) {
            sdat[pl].proccess_dead = 1;
            sdat[pl].cstn = 0;
            return;
        }

        if (((plw[pl].wu.routine_no[1] == 1) && (plw[pl].wu.routine_no[2] == 0x19) &&
             (plw[pl].wu.routine_no[3] != 0)) ||
            (plw[pl].py->flag == 1)) {
            sdat[pl].sflag = 1;

            if (sdat[pl].osflag == 0) {
                sdat[pl].cstn = piyori_type[pl].genkai;
            }

            if (!EXE_flag && !Game_pause) {
                sdat[pl].stimer--;
            }

            if (sdat[pl].g_or_s == 0) {
                if (No_Trans == 0 && (!use_rmlui || !rmlui_hud_stun)) {
                    stun_mark_write(pl, sdat[pl].slen);
                    stun_put(pl, sdat[pl].cstn);
                }

                if (sdat[pl].stimer == 0) {
                    sdat[pl].g_or_s = 1;
                    sdat[pl].stimer = 2;
                }
            } else {
                if (No_Trans == 0 && (!use_rmlui || !rmlui_hud_stun)) {
                    stun_put(pl, sdat[pl].cstn);
                }

                if (sdat[pl].stimer == 0) {
                    sdat[pl].g_or_s = 0;
                    sdat[pl].stimer = 2;
                }
            }

            sdat[pl].osflag = sdat[pl].sflag;
            return;
        }

        sdat[pl].sflag = 0;

        if (sdat[pl].osflag == 1) {
            sdat[pl].osflag = sdat[pl].sflag;
            sdat[pl].g_or_s = 0;
            sdat[pl].stimer = 2;
            sdat[pl].cstn = plw[pl].py->now.quantity.h;
            sdat[pl].osflag = sdat[pl].sflag;

            if (No_Trans == 0) {
                stun_put(pl, sdat[pl].cstn);
            }
            return;
        }

        if (sdat[pl].cstn != plw[pl].py->now.quantity.h) {
            sdat[pl].cstn = plw[pl].py->now.quantity.h;
        }

        if (No_Trans == 0) {
            stun_put(pl, sdat[pl].cstn);
        }
    }
}

/** @brief Clears both players' stun gauge work and display. */
void stngauge_work_clear() {
    u8 i;
    for (i = 0; i < 2; i++) {
        sdat[i].cstn = 0;
        sdat[i].sflag = 0;
        sdat[i].osflag = 0;
        sdat[i].g_or_s = 0;
        sdat[i].stimer = 2;
        sdat[i].proccess_dead = 0;
        stun_put(i, 0);
    }
}
