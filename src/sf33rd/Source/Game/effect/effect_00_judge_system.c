/**
 * @file eff00.c
 * Effect: Judge / System Effect
 */

#include "sf33rd/Source/Game/effect/effect_00_judge_system.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"

const u16 jdb[16] = { 0x8000, 0x80FF, 0xBC00, 0xBCFF, 0x8300, 0x83FF, 0xBF00, 0xBFFF,
                      0xC000, 0xC0FF, 0xFC00, 0xFCFF, 0xC300, 0xC3FF, 0xFF00, 0xFFFF };

static s32 get_dip_modoki(s16 from, s8 fl);
static s32 get_dip_modoki2(s16 from, s8 fl);
static void renewal_table_address(WORK_Other_JUDGE* ewk, State* twk);
static void renewal_table_data(WORK_Other_JUDGE* ewk);

static bool Is_Training_Hitbox_Display_Active() {
    return g_state.Mode_Type == MODE_NORMAL_TRAINING && Is_Training_Hitbox_Display_Enabled();
}

void effect_00_move(WORK_Other_JUDGE* ewk) {
    u16 dip;
    u16 dip2;

    ewk->fade_cja += 2;
    ewk->fade_cja &= 0xFF;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.type = ewk->master_work_id < 16;
        renewal_table_address(ewk, (State*)ewk->my_master);
        ewk->wu.my_priority = ewk->wu.position_z = 1;
        ewk->look_up_flag = 0;
        ewk->curr_ja = 0;
        break;

    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        if (((State*)ewk->my_master)->bbox_work_index != ewk->wu.myself) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        dip = get_dip_modoki(18, ewk->wu.type);
        dip2 = get_dip_modoki2(18, ewk->wu.type);
        ewk->ja_disp_bit = 0;
        ewk->ja_color_bit = 0;

        if (ewk->master_work_id != 1) {
            switch (dip & 0x2000) {
            default:
                goto jump;

            case 0:
                break;
            }
        } else if (dip & 0x1000) {
        jump:
            dip = (dip / 256) & 0xF;
            dip2 = (dip2 / 256) & 0xF;
            ewk->ja_disp_bit = jdb[dip];
            ewk->ja_color_bit = jdb[dip2];
            ewk->curr_ja = Debug_w[DEBUG_CURRENT_BOX_DATA];
        }

        renewal_table_address(ewk, (State*)ewk->my_master);

        if (ewk->wu.type) {
            renewal_table_data(ewk);
        }

        sort_push_request2((State_Other*)ewk);
        break;

    default:
    case 2:
        Release_Effect(&ewk->wu);
        break;
    }
}

static s32 get_dip_modoki(s16 from, s8 fl) {
    s16 rnum = 0;
    bool training_hitbox = Is_Training_Hitbox_Display_Active() && from == DEBUG_DISP_PLAYER_TYPE;

    rnum += ((Debug_w[from] != 0) || training_hitbox) << 12;
    rnum += (Debug_w[from + 5] != 0) << 13;

    if (fl) {
        rnum += ((Debug_w[from + 1] != 0) || training_hitbox) << 8;
        rnum += ((Debug_w[from + 2] != 0) || training_hitbox) << 9;
        rnum += (Debug_w[from + 3] != 0) << 10;
        rnum += (Debug_w[from + 4] != 0) << 11;
    }

    return rnum;
}

static s32 get_dip_modoki2(s16 from, s8 fl) {
    s16 rnum = 0;

    rnum += (Debug_w[from] == 2) << 12;
    rnum += (Debug_w[from + 5] == 2) << 13;

    if (fl) {
        rnum += (Debug_w[from + 1] == 2) << 8;
        rnum += (Debug_w[from + 2] == 2) << 9;
        rnum += (Debug_w[from + 3] == 2) << 10;
        rnum += (Debug_w[from + 4] == 2) << 11;
    }

    return rnum;
}

static void renewal_table_address(WORK_Other_JUDGE* ewk, State* twk) {
    ewk->wu.my_family = twk->my_family;
    ewk->wu.facing_flag = twk->facing_flag;

    if (twk->disp_flag) {
        ewk->wu.disp_flag = 1;
    } else {
        ewk->wu.disp_flag = 0;
    }

    ewk->wu.body_hurtbox = twk->body_hurtbox;
    ewk->wu.hand_hurtbox = twk->hand_hurtbox;
    ewk->wu.catch_box = twk->catch_box;
    ewk->wu.caught_box = twk->caught_box;
    ewk->wu.attack_hitbox = twk->attack_hitbox;
    ewk->wu.pushbox = twk->pushbox;
    ewk->wu.position_x = twk->xyz[0].disp.pos;
    ewk->wu.position_y = twk->xyz[1].disp.pos;
}

static void renewal_table_data(WORK_Other_JUDGE* ewk) {
    u16* mm;
    s16 i;
    s16 j;

    for (mm = (u16*)ewk->wu.body_hurtbox, i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ewk->jx[i][j] = *mm++;
        }
    }

    for (mm = (u16*)ewk->wu.hand_hurtbox, i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ewk->jx[i + 4][j] = *mm++;
        }
    }

    for (mm = (u16*)ewk->wu.catch_box, j = 0; j < 4; j++) {
        ewk->jx[8][j] = *mm++;
    }

    for (mm = (u16*)ewk->wu.caught_box, j = 0; j < 4; j++) {
        ewk->jx[9][j] = *mm++;
    }

    for (mm = (u16*)ewk->wu.attack_hitbox, i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ewk->jx[i + 10][j] = *mm++;
        }
    }

    for (mm = (u16*)ewk->wu.pushbox, j = 0; j < 4; j++) {
        ewk->jx[14][j] = *mm++;
    }
}

s32 effect_00_init(State* wk) {
    WORK_Other_JUDGE* ewk;
    s16 ix;

    if (Debug_w[DEBUG_DISP_PLAYER_TYPE] == 0 && Debug_w[DEBUG_DISP_EFFECT_TYPE] == 0 &&
        !Is_Training_Hitbox_Display_Active()) {
        return 0;
    }

    if ((ix = Acquire_Effect(0)) == -1) {
        return -1;
    }

    ewk = (WORK_Other_JUDGE*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 0;
    ewk->wu.work_id = 128;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->master_work_id = wk->work_id;
    ewk->master_id = wk->id;
    wk->bbox_work_index = ewk->wu.myself;
    return 0;
}
