/**
 * @file effe3.c
 * Effect: Gauge / Player Control Effect
 */

#include "sf33rd/Source/Game/effect/effe3.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/spgauge.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"

void effect_e3_move(WORK_Other* ewk) {
    PLW* mwk = (PLW*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if ((mwk->wu.effect_e3_index != ewk->wu.myself || ewk->wu.dead_f != 0) ||
            (g_state.Mode_Type != MODE_NORMAL_TRAINING && g_state.Mode_Type != MODE_PARRY_TRAINING &&
             g_state.Mode_Type != MODE_TRIALS)) {
            ewk->wu.routine_no[0] = 2;
            break;
        }

        if (mwk->init_effect_e3_flag == 0) {
            break;
        }

        mwk->init_effect_e3_flag = 0;

        if (g_state.Mode_Type != MODE_NORMAL_TRAINING) {
            break;
        }

        if (mwk->wu.id == g_state.New_Challenger && Training[0].contents[0][0][0] != 4) {
            vib_sel[mwk->wu.id] = 0;
        }

        if (Training[0].contents[0][1][3] == 2) {
            vib_sel[mwk->wu.id] = 0;
        }

        ewk->wu.direction = 0;

        if (Training[0].contents[0][0][0] == 3 || Training[0].contents[0][0][0] == 4) {
            ewk->wu.direction = 1;
        }

        omop_vital_ix[mwk->wu.id] = 1;

        if (Training[0].contents[0][1][3] == 0) {
            omop_vital_ix[mwk->wu.id] = 3;
        }

        ewk->wu.damage_vitality = 0;
        ewk->wu.vitality = 0;
        ewk->wu.dir_timer = 0;

        if (mwk->wu.id == g_state.New_Challenger) {
            mwk->py->now.quantity.h = 0;

            if (ewk->wu.direction == 0) {
                switch (Training[0].contents[0][0][1]) {
                case 0:
                case 1:
                case 6:
                    mwk->spmv_ng_flag &= 0xFFFFFFEF;
                    mwk->spmv_ng_flag |= 0xC0;
                    break;

                case 2:
                case 4:
                    mwk->spmv_ng_flag |= 0x80;
                    mwk->spmv_ng_flag &= 0xFFFFFFBF;
                    mwk->spmv_ng_flag &= 0xFFFFFFEF;
                    break;

                case 3:
                case 5:
                    mwk->spmv_ng_flag |= 0x40;
                    mwk->spmv_ng_flag &= 0xFFFFFF7F;
                    mwk->spmv_ng_flag &= 0xFFFFF0FF;
                    break;
                }
            } else {
                mwk->spmv_ng_flag |= 0xC0;
                mwk->spmv_ng_flag &= 0xFFFFFFEF;
                mwk->spmv_ng_flag &= 0xFFFFF0FF;
            }
        }

        switch (Training[0].contents[0][1][0]) {
        case 1:
            mwk->special_move_disabled_flag2 &= 0xFFFBFFFF;
            mwk->special_move_disabled_flag2 |= 0x90000;
            demo_set_sa_full(mwk->sa);
            tr_spgauge_cont_init2(mwk->wu.id);
            break;

        case 3:
            mwk->special_move_disabled_flag2 &= 0xFFF7FFFF;
            mwk->special_move_disabled_flag2 |= 0x50000;
            demo_set_sa_full(mwk->sa);
            tr_spgauge_cont_init2(mwk->wu.id);
            break;

        case 2:
            mwk->special_move_disabled_flag2 &= 0xFFFEFFFF;
            mwk->special_move_disabled_flag2 |= 0xC0000;
            clear_super_arts_point(mwk);
            tr_spgauge_cont_init(mwk->wu.id);
            break;

        case 0:
            mwk->special_move_disabled_flag2 |= 0xD0000;
            clear_super_arts_point(mwk);
            tr_spgauge_cont_init(mwk->wu.id);
            break;
        }

        ewk->wu.routine_no[0]++;
        omop_spmv_ng_table[mwk->wu.id] = mwk->spmv_ng_flag;
        omop_spmv_ng_table2[mwk->wu.id] = mwk->special_move_disabled_flag2;
        /* fallthrough */

    case 1:
        if ((mwk->wu.effect_e3_index != ewk->wu.myself || ewk->wu.dead_f != 0) ||
            (g_state.Mode_Type != MODE_NORMAL_TRAINING && g_state.Mode_Type != MODE_PARRY_TRAINING &&
             g_state.Mode_Type != MODE_TRIALS)) {
            ewk->wu.routine_no[0] = 2;
            break;
        }

        if (mwk->init_effect_e3_flag == 1) {
            ewk->wu.routine_no[0] = 0;
            mwk->spmv_ng_flag = ewk->master_special_move_disabled_flag;
            mwk->special_move_disabled_flag2 = ewk->master_special_move_disabled_flag2;
            break;
        }

        if (g_state.New_Challenger != mwk->wu.id) {
            break;
        }

        /* PR #153: No-Stun training setting — zero the stun gauge every frame */
        if (Training[0].contents[0][1][3] == 2) {
            mwk->py->now.quantity.h = 0;
        }

        if (ewk->wu.direction == 0) {
            mwk->cp->move_state_flags[7] = 2;
        }

        break;

    case 2:
    default:
        push_effect_work(&ewk->wu);
        break;
    }
}

s32 effect_e3_init(PLW* wk) {
    WORK_Other* ewk;
    s16 ix;

    if ((ix = pull_effect_work(3)) == -1) {
        return -1;
    }

    ewk = (WORK_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 143;
    ewk->wu.work_id = 16;
    ewk->my_master = wk;
    ewk->master_work_id = wk->wu.work_id;
    ewk->master_id = wk->wu.id;
    ewk->master_player = wk->player_number;
    ewk->master_special_move_disabled_flag = wk->spmv_ng_flag;
    ewk->master_special_move_disabled_flag2 = wk->special_move_disabled_flag2;
    wk->init_effect_e3_flag = 1;
    wk->wu.effect_e3_index = ewk->wu.myself;
    return 0;
}
