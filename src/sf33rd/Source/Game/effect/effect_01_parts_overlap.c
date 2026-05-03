/**
 * @file eff01.c
 * Effect: Parts / Overlap Effect
 */

#include "sf33rd/Source/Game/effect/effect_01_parts_overlap.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

const s16 parts_colmd_table[2] = { 0x4000, 0x0 };

const s16 parts_colcd_table[14] = {
    0x2000, 0x0, 0x6, 0x2000, 0x4, 0x2020, 0x4, 0x4, 0x0, 0x6, 0x5, 0x4, 0x203C, 0x202A
};

static void get_new_parts_data(State_Other* ewk, PlayerEntity* mwk);
static void set_parts_disp_flag(State_Other* ewk, PlayerEntity* mwk);

void effect_01_move(State_Other* ewk) {
    State* mwk = (State*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.graphic_rom_type = mwk->graphic_rom_type;
        ewk->wu.cg_number = ewk->wu.old_cgnum = 0;
        ewk->wu.blink_timing = mwk->blink_timing;
        ewk->wu.graphic_overlap_index.overlap_col_index[ewk->wu.type] = 0;
        return;

    case 1:
        if (ewk->wu.death_timer == 1 || mwk->olc_work_ix[ewk->wu.type] != ewk->wu.myself) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            return;
        }

        if (mwk->graphic_overlap_index.overlap_col_index[ewk->wu.type] == 0) {
            ewk->wu.graphic_overlap_index.overlap_col_index[ewk->wu.type] = 0;
            return;
        }

        if (!g_state.Game_pause && !g_state.execute_flag) {
            if (ewk->wu.graphic_overlap_index.overlap_col_index[ewk->wu.type] != mwk->graphic_overlap_index.overlap_col_index[ewk->wu.type]) {
                ewk->wu.graphic_overlap_index.overlap_col_index[ewk->wu.type] = ewk->wu.graphic_index = mwk->graphic_overlap_index.overlap_col_index[ewk->wu.type];
                ewk->wu.current_char_type = ewk->wu.graphic_index;

                if (ewk->wu.type == 0 && ((PlayerEntity*)mwk)->player_number == 0 && mwk->facing_flag) {
                    ewk->wu.current_char_type++;
                }

                get_new_parts_data(ewk, (PlayerEntity*)mwk);
            } else if (((PlayerEntity*)mwk)->sa_stop_flag == 0) {
                if (--ewk->wu.cg_ctr == 0) {
                    if (ewk->wu.overlap_char_tbl->parts_nix) {
                        ewk->wu.graphic_index = ewk->wu.overlap_char_tbl->parts_nix;
                    } else {
                        ewk->wu.graphic_index++;
                    }

                    ewk->wu.current_char_type = ewk->wu.graphic_index;
                    get_new_parts_data(ewk, (PlayerEntity*)mwk);
                }
            }

            if (ewk->wu.cg_number == 0) {
                break;
            }

            ewk->wu.position_x = mwk->position_x;
            ewk->wu.position_y = mwk->position_y;
            ewk->wu.position_z = mwk->position_z;
            ewk->wu.facing_flag = mwk->facing_flag;
            ewk->wu.cg_flip = ewk->wu.overlap_char_tbl->parts_flip & 3;

            if (ewk->wu.overlap_char_tbl->parts_flip & 4) {
                ewk->wu.cg_flip ^= mwk->cg_flip;

                if (mwk->cg_flip & 1) {
                    ewk->wu.facing_flag = (ewk->wu.facing_flag + 1) & 1;
                }
            }

            if (ewk->wu.facing_flag) {
                ewk->wu.position_x -= ewk->wu.overlap_char_tbl->parts_hos_x;
            } else {
                ewk->wu.position_x += ewk->wu.overlap_char_tbl->parts_hos_x;
            }

            ewk->wu.position_y += ewk->wu.overlap_char_tbl->parts_hos_y;

            if (ewk->wu.overlap_char_tbl->parts_flip & 4 && mwk->cg_flip & 2) {
                ewk->wu.position_y -= ewk->wu.overlap_char_tbl->parts_hos_y * 2;
            }

            if (ewk->wu.overlap_char_tbl->parts_prio == 2) {
                ewk->wu.position_z -= (ewk->wu.type + 1) * 2;
            } else {
                ewk->wu.position_z += (ewk->wu.type + 1) * 2;
            }
        }

        if (ewk->wu.cg_number == 0) {
            break;
        }

        set_parts_disp_flag(ewk, (PlayerEntity*)mwk);

        if (ewk->wu.overlap_char_tbl->parts_colcd == 0) {
            ewk->wu.my_col_code = mwk->my_col_code;
            ewk->wu.extra_col = mwk->extra_col;
            ewk->wu.extra_col_2 = mwk->extra_col_2;
        }

        if (ewk->wu.overlap_char_tbl->parts_mts) {
            ewk->wu.my_sprite_sheet = 14;
        } else {
            ewk->wu.my_sprite_sheet = mwk->my_sprite_sheet;
        }

        sort_push_request(&ewk->wu);
        break;

    case 2:
    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void get_new_parts_data(State_Other* ewk, PlayerEntity* mwk) {
    ewk->wu.current_char_type = ewk->wu.graphic_index;

    if (ewk->wu.type == 0 && mwk->player_number == 0 && mwk->wu.facing_flag) {
        ewk->wu.current_char_type++;
    }

    ewk->wu.overlap_char_tbl = mwk->wu.overlap_char_tbl + ewk->wu.current_char_type;
    ewk->wu.cg_ctr = ewk->wu.overlap_char_tbl->parts_timer;

    if (ewk->wu.overlap_char_tbl->parts_colmd) {
        if (ewk->wu.overlap_char_tbl->parts_colmd == 1) {
            ewk->wu.my_col_mode = ((State*)mwk->wu.target_adrs)->my_col_mode;
        } else {
            ewk->wu.my_col_mode = parts_colmd_table[ewk->wu.overlap_char_tbl->parts_colmd];
        }
    } else {
        ewk->wu.my_col_mode = mwk->wu.my_col_mode;
    }

    if (ewk->wu.overlap_char_tbl->parts_colcd) {
        ewk->wu.extra_col = 0;
        ewk->wu.extra_col_2 = 0;
        ewk->wu.my_col_code = parts_colcd_table[ewk->wu.overlap_char_tbl->parts_colcd];

        if (!(ewk->wu.my_col_code & 0x2000)) {
            ewk->wu.my_col_code += mwk->wu.my_col_code;
        }
    } else {
        ewk->wu.my_col_code = mwk->wu.my_col_code;
        ewk->wu.my_col_code = mwk->wu.my_col_code;
        ewk->wu.extra_col = mwk->wu.extra_col;
        ewk->wu.extra_col_2 = mwk->wu.extra_col_2;
    }

    ewk->wu.cg_number = ewk->wu.overlap_char_tbl->parts_char;
}

static void set_parts_disp_flag(State_Other* ewk, PlayerEntity* mwk) {
    switch (ewk->wu.overlap_char_tbl->parts_disp) {
    case 1:
        if (mwk->wu.disp_flag) {
            ewk->wu.disp_flag = 1;
            break;
        }

        ewk->wu.disp_flag = 0;
        break;

    case 2:
        if (mwk->wu.disp_flag) {
            ewk->wu.disp_flag = 2;
            break;
        }

        ewk->wu.disp_flag = 0;
        break;

    case 11:
        ewk->wu.disp_flag = 1;
        break;

    case 12:
        ewk->wu.disp_flag = 2;
        break;

    default:
        ewk->wu.disp_flag = mwk->wu.disp_flag;
        break;
    }
}

s32 effect_01_init(State* wk, u8 koolc) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(1)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 1;
    ewk->wu.work_id = 32;
    ewk->wu.type = koolc;
    ewk->wu.my_family = wk->my_family;
    ewk->wu.blink_timing = wk->blink_timing;
    ewk->my_master = wk;
    ewk->master_work_id = wk->work_id;
    ewk->master_id = wk->id;
    ewk->wu.my_sprite_sheet = wk->my_sprite_sheet;
    wk->olc_work_ix[koolc] = ewk->wu.myself;
    return 0;
}
