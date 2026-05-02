/**
 * @file bg_rewrite.c
 * Background tile rewrite setup (stage + ending).
 * Split from bg.c — see SYSTEM_MODERNIZATION.md #37.
 */

#include "sf33rd/Source/Game/stage/bg_rewrite.h"
#include "game_state.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/ending/end_maps.h"
#include "common.h"
#include "structs.h"

/** @brief Set up background tile rewriting tables for the current stage. */
void Bg_Kakikae_Set() {
    u8 i;
    const bgrw_data_tbl_elem* rwtbl_ptr;
    s8 rw;

    switch (g_state.bg_w.stage) {
    case 3:
        g_state.tokusyu_stage = 1;
        g_state.stage_flash = 0;
        g_state.stage_ftimer = 0;
        g_state.rw_dat->rwd_ptr = g_state.rw_dat->brw_ptr = (s16*)rw30;
        g_state.rw_dat->rw_cnt = 2;

        for (i = 0; i < 13; i++) {
            g_state.rw_gbix[i] = stage03rw_data_tbl[i];
        }

        g_state.rw3col_ptr = (u32*)rw30col;

        for (i = 0; i < 4; i++) {
            rw = bgrw_on[g_state.bg_w.stage][i];

            rwtbl_ptr = &bgrw_data_tbl[rw];
            g_state.rw_dat[i + 1].bg_num = rwtbl_ptr->bg_num;
            g_state.rw_dat[i + 1].rwgbix = rwtbl_ptr->rwgbix;
            g_state.rw_dat[i + 1].rwd_ptr = g_state.rw_dat[i + 1].brw_ptr = rwtbl_ptr->rw_ptr;
            g_state.rw_dat[i + 1].rw_cnt = *g_state.rw_dat[i + 1].rwd_ptr++;
            g_state.rw_dat[i + 1].gbix = *g_state.rw_dat[i + 1].rwd_ptr++;
        }
        break;

    case 10:
        g_state.tokusyu_stage = 2;
        g_state.yang_ix = 0;
        g_state.yang_ix_plus = 0;
        g_state.yang_timer = 4;
        break;

    case 19:
        g_state.tokusyu_stage = 3;
        g_state.stage_flash = 0;
        g_state.stage_ftimer = 2;
        g_state.rw_dat->rwd_ptr = g_state.rw_dat->brw_ptr = (s16*)rw190;
        g_state.rw_dat->rw_cnt = 2;

        for (i = 0; i < 4; i++) {
            g_state.rw_gbix[i] = stage19rw_data_tbl[i];
        }

        rw = bgrw_on[g_state.bg_w.stage][0];

        rwtbl_ptr = &bgrw_data_tbl[rw];
        g_state.rw_dat[1].bg_num = rwtbl_ptr->bg_num;
        g_state.rw_dat[1].rwgbix = rwtbl_ptr->rwgbix;
        g_state.rw_dat[1].rwd_ptr = g_state.rw_dat[1].brw_ptr = rwtbl_ptr->rw_ptr;
        g_state.rw_dat[1].rw_cnt = *g_state.rw_dat[1].rwd_ptr++;
        g_state.rw_dat[1].gbix = *g_state.rw_dat[1].rwd_ptr++;
        break;

    default:
        if (g_state.bg_w.stage == 7) {
            g_state.tokusyu_stage = 4;
        } else {
            g_state.tokusyu_stage = 0;
        }

        g_state.rw_num = 0;

        for (i = 0; i < 4; i++) {
            g_state.rw_bg_flag[i] = 0;
        }

        for (i = 0; i < 8; i++) {
            rw = bgrw_on[g_state.bg_w.stage][i];

            if (rw == -1) {
                break;
            }

            g_state.rw_num += 1;

            rwtbl_ptr = &bgrw_data_tbl[rw];
            g_state.rw_dat[i].bg_num = rwtbl_ptr->bg_num;
            g_state.rw_bg_flag[g_state.rw_dat[i].bg_num] = 1;
            g_state.rw_dat[i].rwgbix = rwtbl_ptr->rwgbix;
            g_state.rw_dat[i].rwd_ptr = g_state.rw_dat[i].brw_ptr = rwtbl_ptr->rw_ptr;
            g_state.rw_dat[i].rw_cnt = *g_state.rw_dat[i].rwd_ptr++;
            g_state.rw_dat[i].gbix = *g_state.rw_dat[i].rwd_ptr++;
        }

        break;
    }
}

/** @brief Set up ending-specific background tile rewrite tables. */
void Ed_Kakikae_Set(s16 type) {
    u8 i;
    s8 rw;

    g_state.rw_num = 0;

    for (i = 0; i < 4; i++) {
        g_state.rw_bg_flag[i] = 0;
    }

    switch (type) {
    case 14:
        for (i = 0; i < 20; i++) {
            const gedrw_data* gedrw_data_ptr = &gedrw_data_tbl[i];
            g_state.rw_dat[i].rwgbix = gedrw_data_ptr->rwgbix;
            g_state.rw_dat[i].rwd_ptr = g_state.rw_dat[i].brw_ptr = gedrw_data_ptr->rw_ptr;
        }

        break;

    case 15:
        for (i = 0; i < 16; i++) {
            const cedrw_data* cedrw_data_ptr = &cedrw_data_tbl[i];
            g_state.rw_dat[i].rwgbix = cedrw_data_ptr->rwgbix;
            g_state.rw_dat[i].rwd_ptr = g_state.rw_dat[i].brw_ptr = cedrw_data_ptr->rw_ptr;
        }

        break;

    default:
        if (edrw_num[type][0] != -1) {
            rw = edrw_num[type][0];

            for (i = 0; i < edrw_num[type][1]; i++) {
                const edrw_data* edrw_data_ptr = &edrw_data_tbl[rw + i];
                g_state.rw_num += 1;
                g_state.rw_dat[i].bg_num = edrw_data_ptr->bg_num;
                g_state.rw_bg_flag[g_state.rw_dat[i].bg_num] = 1;
                g_state.rw_dat[i].rwgbix = edrw_data_ptr->rwgbix;
                g_state.rw_dat[i].rwd_ptr = g_state.rw_dat[i].brw_ptr = edrw_data_ptr->rw_ptr;
                g_state.rw_dat[i].rw_cnt = *g_state.rw_dat[i].rwd_ptr++;
                g_state.rw_dat[i].gbix = *g_state.rw_dat[i].rwd_ptr++;
            }
        }

        break;
    }
}
