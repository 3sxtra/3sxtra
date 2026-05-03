/**
 * @file effe0.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_e0_simple_animation.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/screen/character_select_data.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

static void Setup_Char_E0(State_Other* ewk);

void effect_E0_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (Ck_Range_Out_S(ewk, ewk->wu.my_family - 1, 64)) {
            return;
        }

        ewk->wu.disp_flag = 1;

        if (ewk->wu.damage_vitality == 1) {
            ewk->wu.routine_no[0] = 4;
        } else {
            ewk->wu.routine_no[0]++;
        }

        break;

    case 1:
        if (g_state.Sel_EM_Complete[g_state.Player_id]) {
            if (g_state.VS_Index[g_state.Player_id] >= 8) {
                ewk->wu.routine_no[0] = 4;
            } else {
                ewk->wu.routine_no[0] = 2;
            }

            ewk->wu.dir_timer = 1;
        } else if (g_state.Moving_Plate[g_state.Player_id] != 0 && ewk->wu.damage_vitality == 0) {
            if (--g_state.Moving_Plate_Counter[g_state.Player_id] == 0) {
                g_state.Moving_Plate[g_state.Player_id] = 0;
            }

            Setup_Char_E0(ewk);
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        }

        break;

    case 2:
        if (--ewk->wu.dir_timer != 0) {
            break;
        }

        ewk->wu.routine_no[0]++;
        ewk->wu.dir_timer = 20;

        if (g_state.Temporary_EM[g_state.Player_id] == ewk->wu.direction) {
            ewk->wu.char_index = ((ewk->wu.direction - 1) * 4) + 38;
        } else {
            ewk->wu.char_index = ((ewk->wu.direction - 1) * 4) + 37;
        }

        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 3:
        if (g_state.Exec_Wipe == 0) {
            char_move(&ewk->wu);
        }

        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.routine_no[0]++;
            g_state.Sel_EM_Complete[g_state.Player_id] |= ~0x7F;
            ewk->wu.char_index = ((ewk->wu.direction - 1) * 4) + 35;
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        }

        break;

    case 4:
        if (Ck_Range_Out_S(ewk, ewk->wu.my_family - 1, 64)) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            return;
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
        /* Show during stage select (g_state.Exit_No != 0); gate during char select only */
        if (!rmlui_char_select_visible || g_state.Exit_No != 0)
            sort_push_request4(&ewk->wu);
        return;

    default:
        Release_Effect(&ewk->wu);
        return;
    }

    if (ewk->wu.damage_vitality == 0 && g_state.Exec_Wipe == 0) {
        char_move(&ewk->wu);
    }

    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
    /* Show during stage select (g_state.Exit_No != 0); gate during char select only */
    if (!rmlui_char_select_visible || g_state.Exit_No != 0)
        sort_push_request4(&ewk->wu);
}

static void Setup_Char_E0(State_Other* ewk) {
    ewk->wu.char_index = ((ewk->wu.direction - 1) * 4) + 35;
    ewk->wu.dir_step = 0;

    if (ewk->wu.direction == g_state.Temporary_EM[g_state.Player_id]) {
        ewk->wu.char_index++;
    }
}

s32 effect_E0_init(s16 Direction, s16 damage_vitality, s16 Pos_Type) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 140;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 0x2090;
    ewk->wu.my_family = 4;
    *ewk->wu.char_table = _sel_pl_char_table;
    ewk->wu.damage_vitality = damage_vitality;
    ewk->wu.direction = Direction;
    ewk->wu.my_sprite_sheet = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.xyz[0].disp.pos =
        g_state.Offset_BG_X[3] + g_state.bg_w.bgw[3].wxy[0].disp.pos + Plate_Pos_Data_E0[Pos_Type][0];
    ewk->wu.xyz[1].disp.pos = g_state.bg_w.bgw[3].wxy[1].disp.pos + Plate_Pos_Data_E0[Pos_Type][1];
    ewk->wu.position_z = 17;

    if (Direction == 2) {
        ewk->wu.xyz[1].disp.pos -= 104;
        ewk->wu.position_z++;
    }

    if (damage_vitality == 1) {
        ewk->wu.xyz[0].disp.pos += 3;
        ewk->wu.xyz[1].disp.pos--;
        ewk->wu.char_index = 17;
        ewk->wu.dir_step = Direction - 1;
    } else {
        Setup_Char_E0(ewk);
    }

    set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
    return 0;
}
