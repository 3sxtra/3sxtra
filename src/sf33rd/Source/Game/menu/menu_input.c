/**
 * @file menu_input.c
 * @brief Menu input handling, cursor movement, and selection logic.
 *
 * Contains functions that handle controller input, cursor navigation,
 * and selection confirmation.  Split from menu.c for maintainability.
 */

#include "common.h"
#include "main.h"
#include "netplay/netplay.h"
#include "port/menu_screen.h"
#include "port/save/native_save.h"
#include "port/sdl/app/sdl_app.h"
#include "sf33rd/Source/Game/menu/menu_input_constants.h"
#include "port/sdl/input/controller_image_overlay.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff04.h"
#include "sf33rd/Source/Game/effect/eff10.h"
#include "sf33rd/Source/Game/effect/eff18.h"
#include "sf33rd/Source/Game/effect/eff23.h"
#include "sf33rd/Source/Game/effect/eff38.h"
#include "sf33rd/Source/Game/effect/eff39.h"
#include "sf33rd/Source/Game/effect/eff40.h"
#include "sf33rd/Source/Game/effect/eff43.h"
#include "sf33rd/Source/Game/effect/eff45.h"
#include "sf33rd/Source/Game/effect/eff51.h"
#include "sf33rd/Source/Game/effect/eff57.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff61.h"
#include "sf33rd/Source/Game/effect/eff63.h"
#include "sf33rd/Source/Game/effect/eff64.h"
#include "sf33rd/Source/Game/effect/eff66.h"
#include "sf33rd/Source/Game/effect/eff75.h"
#include "sf33rd/Source/Game/effect/eff91.h"
#include "sf33rd/Source/Game/effect/effa0.h"
#include "sf33rd/Source/Game/effect/effa3.h"
#include "sf33rd/Source/Game/effect/effa8.h"
#include "sf33rd/Source/Game/effect/effc4.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effk6.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/io/vm_sub.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/menu/ex_data.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/saver.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/training_dummy.h"
#include "sf33rd/Source/Game/ui/count.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_button_config.h"
#include "port/sdl/rmlui/rmlui_game_option.h"
#include "port/sdl/rmlui/rmlui_memory_card.h"
#include "port/sdl/rmlui/rmlui_option_menu.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

static void apply_training_hitbox_display(bool force_off);

/** @brief System Direction cursor move handler (up/down). */
void System_Dir_Move_Sub(s16 PL_id) {
    u16 sw = ~plsw_01[PL_id] & plsw_00[PL_id]; // potential macro
    sw = Check_Menu_Lever(PL_id, 0);
    MC_Move_Sub(sw, 0, DIR_CURSOR_MAX, 0xFF);
    System_Dir_Move_Sub_LR(sw, 0);
    Direction_Working[1] = Convert_Buff[3][0][0];
    Direction_Working[4] = Convert_Buff[3][0][0];
    Direction_Working[5] = Convert_Buff[3][0][0];
}

/** @brief System Direction cursor move handler (left/right toggle). */
void System_Dir_Move_Sub_LR(u16 sw, s16 cursor_id) {
    if (Menu_Cursor_Y[cursor_id] != 0) {
        return;
    }

    switch (sw) {
    case SWK_LEFT:
        Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] -= 1;

        if (Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] < 0) {
            Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] = 1;
        }

        SE_dir_cursor_move();
        return;

    case SWK_RIGHT:
        Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] += 1;

        if (Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] > 1) {
            Convert_Buff[3][cursor_id][Menu_Cursor_Y[cursor_id]] = 0;
        }

        SE_dir_cursor_move();
        return;
    }
}

/** @brief Direction menu cursor move handler (up/down). */
void Dir_Move_Sub(struct _TASK* task_ptr, s16 PL_id) {
    u16 sw;
    u16 ix;

    plsw_00[0] = PLsw[0][0];
    plsw_01[0] = PLsw[0][1];
    plsw_00[1] = PLsw[1][0];
    plsw_01[1] = PLsw[1][1];

    for (ix = 0; ix < 2; ix++) {
        plsw_00[ix] &= INPUT_MASK_NO_PAUSE;
        plsw_01[ix] &= INPUT_MASK_NO_PAUSE;
    }

    sw = Check_Menu_Lever(PL_id, 0);
    Dir_Move_Sub2(sw);

    if (task_ptr->r_no[1] == MENU_RNO1_EXTRA_OPTION) {
        Ex_Move_Sub_LR(sw, PL_id);
        return;
    }

    Dir_Move_Sub_LR(sw, PL_id);
}

/** @brief Direction menu cursor move handler (up/down with wrap). */
u16 Dir_Move_Sub2(u16 sw) {
    if (Menu_Cursor_Move > 0) {
        return 0;
    }

    switch (sw) {
    case SWK_UP:
        Menu_Cursor_Y[0] -= 1;

        if (Menu_Cursor_Y[0] < 0) {
            Menu_Cursor_Y[0] = Menu_Max;
        }

        SE_cursor_move();
        return IO_Result = SWK_UP;

    case SWK_DOWN:
        Menu_Cursor_Y[0] += 1;

        if (Menu_Cursor_Y[0] > Menu_Max) {
            Menu_Cursor_Y[0] = 0;
        }

        SE_cursor_move();
        return IO_Result = SWK_DOWN;

    case SWK_WEST:
        return IO_Result = SWK_WEST;

    case SWK_NORTH:
        return IO_Result = SWK_NORTH;

    case SWK_RIGHT_SHOULDER:
        return IO_Result = SWK_RIGHT_SHOULDER;

    case SWK_LEFT_SHOULDER:
        return IO_Result = SWK_LEFT_SHOULDER;

    case SWK_SOUTH:
        return IO_Result = SWK_SOUTH;

    case SWK_EAST:
        return IO_Result = SWK_EAST;

    case SWK_RIGHT_TRIGGER:
        return IO_Result = SWK_RIGHT_TRIGGER;

    case SWK_LEFT_TRIGGER:
        return IO_Result = SWK_LEFT_TRIGGER;

    case SWK_START:
        return IO_Result = SWK_START;

    default:
        return IO_Result = 0;
    }
}

/** @brief Direction menu left/right value toggle handler. */
void Dir_Move_Sub_LR(u16 sw, s16 /* unused */) {
    u8 last_pos = system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]];

    switch (sw) {
    case SWK_LEFT:
        SE_dir_cursor_move();
        system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] -= 1;

        if (Menu_Cursor_Y[0] == Menu_Max) {
            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] < 0) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = 0;
                IO_Result = SWK_LEFT_SHOULDER;
                return;
            }

            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] != last_pos) {
                Message_Data->order = 1;
                Message_Data->request = system_dir[1].contents[Menu_Page][Menu_Max] + MSG_DIR_PAGE_OFFSET;
                Message_Data->timer = 2;
            }
        } else {
            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] < 0) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = Dir_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]];
            }
        }

        return;

    case SWK_RIGHT:
        SE_dir_cursor_move();
        system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] += 1;

        if (Menu_Cursor_Y[0] == Menu_Max) {
            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] > 2) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = 2;
                IO_Result = SWK_RIGHT_TRIGGER;
                return;
            }

            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] > 2) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = 2;
            }

            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] != last_pos) {
                Message_Data->order = 1;
                Message_Data->request = system_dir[1].contents[Menu_Page][Menu_Max] + MSG_DIR_PAGE_OFFSET;
                Message_Data->timer = 2;
            }
        } else {
            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] > Dir_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]]) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = 0;
            }
        }

        return;

    case SWK_SOUTH:
        SE_dir_cursor_move();

        if (Menu_Cursor_Y[0] == Menu_Max) {
            return;
        } else {
            system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] += 1;

            if (system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] > Dir_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]]) {
                system_dir[1].contents[Menu_Page][Menu_Cursor_Y[0]] = 0;
            }
        }

        return;
    }
}

/** @brief Transition to next Direction page (save â†’ load). */
void Setup_Next_Page(struct _TASK* task_ptr, u8 /* unused */) {
    s16 ix;
    s16 disp_index;
    s16 mode_type;

    s16 unused_s3;

    Menu_Page_Buff = Menu_Page;
    effect_work_init();
    Menu_Common_Init();
    Menu_Cursor_Y[0] = 0;
    Order[EFF_SLOT_HEADER] = 5;
    Order_Timer[EFF_SLOT_HEADER] = 1;

    if (task_ptr->r_no[1] == MENU_RNO1_EXTRA_OPTION) {
        mode_type = 1;
        Menu_Max = Ex_Page_Data[Menu_Page];
        save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page][Menu_Max] = 1;
        Order_Dir[EFF_SLOT_HEADER] = 1;
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_OPTION_MENU, 0, MENU_Z_HEADER, 0);
        Order[EFF_SLOT_EX_HEADER] = 3;
        Order_Dir[EFF_SLOT_EX_HEADER] = 8;
        Order_Timer[EFF_SLOT_EX_HEADER] = 1;
        if (!use_rmlui || !rmlui_menu_extra_option)
            effect_57_init(EFF_SLOT_EX_HEADER, MENU_HEADER_EXTRA_OPTION, 0, MENU_Z_EX_HEADER, 2);
        effect_66_init(EFF_SLOT_DIR_ROW1, EFF_SPRITE_EX_SOUND, 2, 0, MENU_Z_LABEL_LIST, ORDERDIR_SUB_LABEL, 0);
        Order[EFF_SLOT_DIR_ROW1] = 3;
        Order_Timer[EFF_SLOT_DIR_ROW1] = 1;
        effect_66_init(EFF_SLOT_DIR_ROW2, EFF_SPRITE_EX_DISP, 2, 0, MENU_Z_PAGE_NUM, (s16)Menu_Page + 1, 0);
        Order[EFF_SLOT_DIR_ROW2] = 3;
        Order_Timer[EFF_SLOT_DIR_ROW2] = 1;

        if ((msgExtraTbl[0]->msgNum[Menu_Cursor_Y[0] + Menu_Page * 8]) == 1) {
            Message_Data->pos_y = MSG_POS_Y_SINGLE;
        } else {
            Message_Data->pos_y = MSG_POS_Y_MULTI;
        }

        Message_Data->request = Ex_Account_Data[Menu_Page] + Menu_Cursor_Y[0];
    } else {
        mode_type = 0;
        Menu_Max = Page_Data[Menu_Page];
        system_dir[1].contents[Menu_Page][Menu_Max] = 1;
        Order[EFF_SLOT_HEADER] = 5;
        Order_Dir[EFF_SLOT_HEADER] = 3;
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, MENU_Z_HEADER, 0); /* blue BG — unconditional */

        if (!use_rmlui || !rmlui_menu_sysdir) {
            effect_66_init(EFF_SLOT_DIR_GAME, EFF_SPRITE_DIR_GAME, 2, 0, MENU_Z_LABEL_LIST, ORDERDIR_TITLE_LABEL, 0);
            Order[EFF_SLOT_DIR_GAME] = 3;
            Order_Timer[EFF_SLOT_DIR_GAME] = 1;
            effect_66_init(EFF_SLOT_DIR_ROW1, EFF_SPRITE_DIR_DIFF, 2, 0, MENU_Z_LABEL_LIST, ORDERDIR_SUB_LABEL, 0);
            Order[EFF_SLOT_DIR_ROW1] = 3;
            Order_Timer[EFF_SLOT_DIR_ROW1] = 1;
            effect_66_init(EFF_SLOT_DIR_ROW2, EFF_SPRITE_DIR_ROUND, 2, 0, MENU_Z_PAGE_NUM, (s16)Menu_Page + 1, 0);
            Order[EFF_SLOT_DIR_ROW2] = 3;
            Order_Timer[EFF_SLOT_DIR_ROW2] = 1;
        }

        if ((msgSysDirTbl[0]->msgNum[Menu_Page * DIR_PAGE_STRIDE + Menu_Cursor_Y[0] * 2 + 1]) == 1) {
            Message_Data->pos_y = MSG_POS_Y_SINGLE;
        } else {
            Message_Data->pos_y = MSG_POS_Y_MULTI;
        }

        disp_index = Menu_Page * DIR_PAGE_STRIDE;
        Message_Data->request = disp_index + 1;
    }

    Menu_Cursor_Y[0] = 0;

    if (!use_rmlui || !rmlui_menu_sysdir || mode_type == 1) {
        effect_66_init(EFF_SLOT_CURSOR_BG, EFF_SPRITE_PAGE_IND, 2, 0, -1, -1, EFF66_ZORDER_PAUSE);
        Order[EFF_SLOT_CURSOR_BG] = 3;
        Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        Message_Data->order = 0;
        Message_Data->timer = 1;
        Message_Data->pos_x = 0;
        Message_Data->pos_z = MENU_Z_HEADER;
        effect_45_init(0, 0, 2);

        for (ix = 0; ix < Menu_Max; ix++, unused_s3 = disp_index += 2) {
            if (mode_type == 0) {
                effect_18_init(disp_index, ix, 0, 2);
                effect_51_init(ix, ix, 2);
            } else {
                effect_C4_init(0, ix, ix, 2);

                if (Menu_Page != 0 || ix != (Menu_Max - 1)) {
                    effect_C4_init(1, ix, ix, 2);
                }
            }
        }

        effect_40_init(mode_type, 0, EFF40_SEL_0, 0, 2, 1);
        effect_40_init(mode_type, 1, EFF40_SEL_1, 0, 2, 1);
        effect_40_init(mode_type, 2, EFF40_SEL_2, 0, 2, 0);
        effect_40_init(mode_type, 3, EFF40_SEL_3, 0, 2, 2);
    }
}

/** @brief Save current Direction settings to memory card. */
void Save_Direction(struct _TASK* task_ptr) {
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Cursor_X[0] = 0;
        Setup_BG(1, BG_SLIDE_X_FULL, 0);
        if (!(use_rmlui && rmlui_menu_sysdir))
            Setup_Replay_Sub(EFF_SLOT_DIR_HDR, MENU_HEADER_SYSTEM_DIRECTION, 2);
        Setup_File_Property(2, 0);
        Clear_Flash_Init(4);
        Message_Data->kind_req = 5;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            NativeSave_SaveDirection();
        }

        break;

    case 2:
        Setup_Save_Replay_2nd(task_ptr, 2);
        break;

    case 3:
        /* Synchronous — always done */
        IO_Result = SWK_EAST;
        Load_Replay_MC_Sub(task_ptr, 0);
        break;
    }
}

/** @brief Load Direction settings from memory card. */
void Load_Direction(struct _TASK* task_ptr) {
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Cursor_X[0] = 0;
        Setup_BG(1, BG_SLIDE_X_FULL, 0);
        if (!(use_rmlui && rmlui_menu_sysdir))
            Setup_Replay_Sub(EFF_SLOT_DIR_HDR, MENU_HEADER_SYSTEM_DIRECTION, 2);
        Setup_File_Property(2, 0);
        Clear_Flash_Init(4);
        Message_Data->kind_req = 5;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            NativeSave_LoadDirection();
        }

        break;

    case 2:
        if (FadeIn(1, FADE_SPEED_SLOW, 8) != 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->free[3] = 0;
            Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(0, 8);
        }

        break;

    case 3:
        /* Synchronous — always done */
        IO_Result = SWK_EAST;
        Load_Replay_MC_Sub(task_ptr, 0);
        break;
    }
}

/** @brief Load Replay sub-routine â€” handle file list and selection. */
void Load_Replay_Sub(struct _TASK* task_ptr) {
    s32 ix;

    switch (task_ptr->r_no[3]) {
    case 0:
        task_ptr->r_no[3] += 1;
        Rep_Game_Infor[REPLAY_INFO_SLOT] = Replay_w.game_infor;
        cpExitTask(TASK_ENTRY);
        Play_Mode = 3;
        break;

    case 1:
        task_ptr->r_no[3] += 1;
        FadeInit();
        FadeOut(0, FADE_OPAQUE, 8);
        Play_Type = 1;
        Mode_Type = MODE_REPLAY;
        Present_Mode = 3;
        Bonus_Game_Flag = 0;

        for (ix = 0; ix < 2; ix++) {
            plw[ix].wu.pl_operator = Replay_w.game_infor.player_infor[ix].player_type;
            Operator_Status[ix] = Replay_w.game_infor.player_infor[ix].player_type;
            My_char[ix] = Replay_w.game_infor.player_infor[ix].my_char;
            Super_Arts[ix] = Replay_w.game_infor.player_infor[ix].sa;
            Player_Color[ix] = Replay_w.game_infor.player_infor[ix].color;
            Vital_Handicap[3][ix] = Replay_w.game_infor.Vital_Handicap[ix];
        }

        Direction_Working[3] = Replay_w.game_infor.Direction_Working;
        bg_w.stage = Replay_w.game_infor.stage;
        bg_w.area = 0;
        save_w[SAVEW_REPLAY].Time_Limit = Replay_w.mini_save_w.Time_Limit;
        save_w[SAVEW_REPLAY].Battle_Number[0] = Replay_w.mini_save_w.Battle_Number[0];
        save_w[SAVEW_REPLAY].Battle_Number[1] = Replay_w.mini_save_w.Battle_Number[1];
        save_w[SAVEW_REPLAY].Damage_Level = Replay_w.mini_save_w.Damage_Level;
        save_w[SAVEW_REPLAY].extra_option = Replay_w.mini_save_w.extra_option;
        system_dir[3] = Replay_w.system_dir;
        save_w[SAVEW_REPLAY].extra_option = Replay_w.mini_save_w.extra_option;
        save_w[SAVEW_REPLAY].Pad_Infor[0] = Replay_w.mini_save_w.Pad_Infor[0];
        save_w[SAVEW_REPLAY].Pad_Infor[1] = Replay_w.mini_save_w.Pad_Infor[1];
        save_w[SAVEW_REPLAY].Pad_Infor[0].Vibration = 0;
        save_w[SAVEW_REPLAY].Pad_Infor[1].Vibration = 0;
        cpExitTask(TASK_SAVER);
        break;

    case 2:
        FadeOut(0, FADE_OPAQUE, 8);
        task_ptr->r_no[3] += 1;
        task_ptr->timer = REPLAY_LOAD_TIMER;
        System_all_clear_Level_B();
        pulpul_stop();
        init_pulpul_work();
        bg_etc_write(2);
        bg_w.bgw[0].wxy[0].disp.pos += BG_SLIDE_X_FULL;
        Setup_BG(0, bg_w.bgw[0].wxy[0].disp.pos, bg_w.bgw[0].wxy[1].disp.pos);
        effect_38_init(0, EFF_SLOT_P1_PORTRAIT, My_char[0], 1, 0);
        Order[EFF_SLOT_P1_PORTRAIT] = 3;
        Order_Timer[EFF_SLOT_P1_PORTRAIT] = 1;
        effect_38_init(1, EFF_SLOT_P2_PORTRAIT, My_char[1], 1, 0);
        Order[EFF_SLOT_P2_PORTRAIT] = 3;
        Order_Timer[EFF_SLOT_P2_PORTRAIT] = 1;
        effect_K6_init(0, EFF_SLOT_P1_SA, EFF_SLOT_P1_SA, 0);
        Order[EFF_SLOT_P1_SA] = 3;
        Order_Timer[EFF_SLOT_P1_SA] = 1;
        effect_K6_init(1, EFF_SLOT_P2_SA, EFF_SLOT_P1_SA, 0);
        Order[EFF_SLOT_P2_SA] = 3;
        Order_Timer[EFF_SLOT_P2_SA] = 1;
        effect_39_init(0, EFF_SLOT_P1_NAME, My_char[0], 0, 0);
        Order[EFF_SLOT_P1_NAME] = 3;
        Order_Timer[EFF_SLOT_P1_NAME] = 1;
        effect_39_init(1, EFF_SLOT_P2_NAME, My_char[1], 0, 0);
        Order[EFF_SLOT_P2_NAME] = 3;
        Order_Timer[EFF_SLOT_P2_NAME] = 1;
        effect_K6_init(0, EFF_SLOT_P1_WIN, EFF_SLOT_P1_WIN, 0);
        Order[EFF_SLOT_P1_WIN] = 3;
        Order_Timer[EFF_SLOT_P1_WIN] = 1;
        effect_K6_init(1, EFF_SLOT_P2_WIN, EFF_SLOT_P1_WIN, 0);
        Order[EFF_SLOT_P2_WIN] = 3;
        Order_Timer[EFF_SLOT_P2_WIN] = 1;
        effect_43_init(2, 0);
        effect_75_init(EFF_SLOT_STAGE_NAME, 3, 0);
        Order[EFF_SLOT_STAGE_NAME] = 3;
        Order_Timer[EFF_SLOT_STAGE_NAME] = 1;
        Order_Dir[EFF_SLOT_STAGE_NAME] = 5;
        break;

    case 3:
        FadeOut(0, FADE_OPAQUE, 8);

        if (--task_ptr->timer <= 0) {
            task_ptr->r_no[3] += 1;
            bgPalCodeOffset[0] = PAL_OFFSET_REPLAY;
            BGM_Request(51);
            Purge_memory_of_kind_of_key(PURGE_KEY_REPLAY);
            Push_LDREQ_Queue_Player(0, My_char[0]);
            Push_LDREQ_Queue_Player(1, My_char[1]);
            Push_LDREQ_Queue_BG((u16)bg_w.stage);
        }

        break;

    case 4:
        if (FadeIn(0, 4, 8) != 0) {
            task_ptr->r_no[3] += 1;
        }

        break;

    case 5:
        if ((Check_PL_Load() != 0) && (Check_LDREQ_Queue_BG((u16)bg_w.stage) != 0) && (adx_now_playend() != 0) &&
            (sndCheckVTransStatus(0) != 0)) {
            task_ptr->r_no[3] += 1;
            Switch_Screen_Init(0);
            init_omop();
        }

        break;

    case 6:
        if (Switch_Screen(0) != 0) {
            Game01_Sub();
            Cover_Timer = 5;
            appear_type = APPEAR_TYPE_ANIMATED;
            set_hitmark_color();
            Purge_texcash_of_list(3);
            Make_texcash_of_list(3);
            G_No[1] = 2;
            G_No[2] = 0;
            G_No[3] = 0;
            E_No[0] = 4;
            E_No[1] = 0;
            E_No[2] = 0;
            E_No[3] = 0;

            if (plw->wu.pl_operator != 0) {
                Sel_Arts_Complete[0] = -1;
            }

            if (plw[1].wu.pl_operator != 0) {
                Sel_Arts_Complete[1] = -1;
            }

            task_ptr->r_no[2] = 0;
            cpExitTask(TASK_MENU);
        }

        break;

    default:
        break;
    }
}

/** @brief Memory-card replay load sub-routine with error handling. */
s32 Load_Replay_MC_Sub(struct _TASK* task_ptr, s16 PL_id) {
    u16 sw = IO_Result;

    switch (sw) {
    case SWK_SOUTH:
        if ((Menu_Cursor_X[0] == -1) || (vm_w.Connect[Menu_Cursor_X[0]] == 0)) {
            break;
        }

        Pause_ID = PL_id;
        vm_w.Drive = (u8)Menu_Cursor_X[0];

        if (VM_Access_Request(6, Menu_Cursor_X[0]) == 0) {
            break;
        }

        SE_selected();
        task_ptr->free[1] = 0;
        task_ptr->free[2] = 0;
        task_ptr->r_no[0] = 3;
        return 1;

    case SWK_EAST:
        if (task_ptr->r_no[1] == 6) {
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 1;
            task_ptr->r_no[1] = 1;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Order[EFF_SLOT_REPLAY_HDR] = 4;
            Order_Timer[EFF_SLOT_REPLAY_HDR] = 4;
        } else {
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 0;
            Menu_Suicide[2] = 1;
            task_ptr->r_no[1] = 5;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Order[EFF_SLOT_DIR_HDR] = 4;
            Order_Timer[EFF_SLOT_DIR_HDR] = 4;
        }

        break;
    }

    return 0;
}

/** @brief Game Options cursor sub-handler (up/down). */
u16 Game_Option_Sub(s16 PL_id) {
    u16 sw;
    u16 ret;

    sw = ~plsw_01[PL_id] & plsw_00[PL_id];
    sw = Check_Menu_Lever(PL_id, 0);
    ret = MC_Move_Sub(sw, 0, GAME_OPTION_MAX, 0xFF);
    ret |= GO_Move_Sub_LR(sw, 0);
    ret &= RESULT_MASK;
    return ret;
}

const u8 Game_Option_Index_Data[10] = { 7, 3, 3, 3, 3, 1, 1, 1, 1, 1 };

/** @brief Game Options left/right value toggle handler. */
u16 GO_Move_Sub_LR(u16 sw, s16 cursor_id) {
    if (Menu_Cursor_Y[cursor_id] > 9) {
        return 0;
    }

    switch (sw) {
    case SWK_LEFT:
        Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] -= 1;

        if (Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] < 0) {
            Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] = Game_Option_Index_Data[Menu_Cursor_Y[cursor_id]];
        }

        SE_dir_cursor_move();
        return SWK_LEFT;

    case SWK_RIGHT:
        Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] += 1;

        if (Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] > Game_Option_Index_Data[Menu_Cursor_Y[cursor_id]]) {
            Convert_Buff[0][cursor_id][Menu_Cursor_Y[cursor_id]] = 0;
        }

        SE_dir_cursor_move();
        return SWK_RIGHT;

    default:
        return 0;
    }
}

/** @brief Button Config cursor sub-handler (up/down). */
void Button_Config_Sub(s16 PL_id) {
    u16 sw = ~plsw_01[PL_id] & plsw_00[PL_id];
    sw = Check_Menu_Lever(PL_id, 0);
    MC_Move_Sub(sw, PL_id, BUTTON_CONFIG_MAX, 0xFF);
    Button_Move_Sub_LR(sw, PL_id);

    if (ppwork[0].ok_dev == 0) {
        Convert_Buff[1][0][8] = 0;
    }

    if (ppwork[1].ok_dev == 0) {
        Convert_Buff[1][1][8] = 0;
    }
}

/** @brief Button Config left/right value toggle handler. */
void Button_Move_Sub_LR(u16 sw, s16 cursor_id) {
    s16 max;

    switch (Menu_Cursor_Y[cursor_id]) {
    case 8:
        max = 1;
        break;

    case 9:
    case 10:
        max = 0;
        break;

    default:
        max = 11;
        break;
    }

    if (max == 0) {
        return;
    }

    switch (sw) {
    case SWK_LEFT:
        Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] -= 1;

        if (Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] < 0) {
            Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] = max;
        }

        if (Menu_Cursor_Y[cursor_id] == 8) {
            if (Convert_Buff[1][cursor_id][8]) {
                pp_vib_on(cursor_id);
            } else {
                pulpul_stop2(cursor_id);
            }
        }

        SE_dir_cursor_move();
        break;

    case SWK_RIGHT:
        Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] += 1;

        if (Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] > max) {
            Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] = 0;
        }

        if ((Menu_Cursor_Y[cursor_id] == 8) && (Convert_Buff[1][cursor_id][Menu_Cursor_Y[cursor_id]] == 1)) {
            pp_vib_on(cursor_id);
        }

        SE_dir_cursor_move();
        break;
    }
}

/** @brief Check for button-config exit (confirm / cancel / default). */
void Button_Exit_Check(struct _TASK* task_ptr, s16 PL_id) {
    switch (IO_Result) {
    case SWK_EAST:
    case SWK_SOUTH:
        break;

    default:
        return;
    }

    switch (task_ptr->r_no[1]) {
    case 9:
        if (Menu_Cursor_Y[0] == 11 || IO_Result == SWK_EAST) {
            SE_selected();
            if (use_rmlui && rmlui_menu_game_option)
                rmlui_game_option_hide();
            Return_Option_Mode_Sub(task_ptr);
            Order[EFF_SLOT_GAME_OPT] = 4;
            Order_Timer[EFF_SLOT_GAME_OPT] = 4;
            return;
        }

        if (Menu_Cursor_Y[0] == 10) {
            SE_selected();
            save_w[SAVEW_ARCADE].Difficulty = Game_Default_Data.Difficulty;
            save_w[SAVEW_ARCADE].Time_Limit = Game_Default_Data.Time_Limit;
            save_w[SAVEW_ARCADE].Battle_Number[0] = Game_Default_Data.Battle_Number[0];
            save_w[SAVEW_ARCADE].Battle_Number[1] = Game_Default_Data.Battle_Number[1];
            save_w[SAVEW_ARCADE].Damage_Level = Game_Default_Data.Damage_Level;
            save_w[SAVEW_ARCADE].GuardCheck = Game_Default_Data.GuardCheck;
            save_w[SAVEW_ARCADE].AnalogStick = Game_Default_Data.AnalogStick;
            save_w[SAVEW_ARCADE].Handicap = Game_Default_Data.Handicap;
            save_w[SAVEW_ARCADE].Partner_Type[0] = Game_Default_Data.Partner_Type[0];
            save_w[SAVEW_ARCADE].Partner_Type[1] = Game_Default_Data.Partner_Type[1];
            Copy_Save_w();
            return;
        }

        break;

    case 10:
        if ((Menu_Cursor_Y[PL_id] == 10) || (IO_Result == SWK_EAST)) {
            SE_selected();
            ControllerImageOverlay_Shutdown();
            if (use_rmlui && rmlui_menu_button_config)
                rmlui_button_config_hide();
            Return_Option_Mode_Sub(task_ptr);
            Order[EFF_SLOT_BTN_CFG] = 4;
            Order_Timer[EFF_SLOT_BTN_CFG] = 4;
            return;
        }

        if (Menu_Cursor_Y[PL_id] == 9) {
            SE_selected();
            Setup_IO_ConvDataDefault(PL_id);
            Save_Game_Data();
            return;
        }

        break;

    case 13:
        if (IO_Result == SWK_EAST) {
            SE_selected();
            if (use_rmlui && rmlui_menu_memory_card)
                rmlui_memory_card_hide();
            Return_Option_Mode_Sub(task_ptr);
            Order[EFF_SLOT_MEM_CARD] = 4;
            Order_Timer[EFF_SLOT_MEM_CARD] = 4;
            return;
        }

        switch (Menu_Cursor_Y[0]) {
        case 3:
            SE_selected();
            if (use_rmlui && rmlui_menu_memory_card)
                rmlui_memory_card_hide();
            Return_Option_Mode_Sub(task_ptr);
            Order[EFF_SLOT_MEM_CARD] = 4;
            Order_Timer[EFF_SLOT_MEM_CARD] = 4;
            break;

        case 0:
            SE_selected();
            task_ptr->r_no[2] = 4;
            task_ptr->r_no[3] = 0;
            break;

        case 1:
            SE_selected();
            task_ptr->r_no[2] = 5;
            task_ptr->r_no[3] = 0;
            break;

        case 2:
            task_ptr->r_no[2] = 6;
            task_ptr->r_no[3] = 0;
            break;
        }

        break;
    }
}

/** @brief Return to options sub-menu from a settings screen. */
void Return_Option_Mode_Sub(struct _TASK* task_ptr) {
    Menu_Suicide[1] = 0;
    Menu_Suicide[2] = 1;
    task_ptr->r_no[1] = 7;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->free[0] = 0;
    Cursor_Y_Pos[0][2] = Menu_Cursor_Y[0];
    Cursor_Y_Pos[1][2] = Menu_Cursor_Y[1];
    if (use_rmlui && rmlui_menu_option)
        rmlui_option_menu_show();
}

/** @brief Screen Adjust cursor sub-handler (up/down). */
void Screen_Adjust_Sub(s16 PL_id) {
    u16 sw;
    sw = ~plsw_01[PL_id] & plsw_00[PL_id];
    sw = Check_Menu_Lever(PL_id, 0);
    MC_Move_Sub(sw, 0, SCREEN_ADJUST_MAX, 0xFF);
    Screen_Move_Sub_LR(sw);
    Convert_Buff[2][0][0] = X_Adjust_Buff[2] & 0xFF;
    Convert_Buff[2][0][1] = Y_Adjust_Buff[2] & 0xFF;
    Convert_Buff[2][0][2] = dspwhPack(Disp_Size_H, Disp_Size_V);
    save_w[SAVEW_ARCADE].Screen_Size = dspwhPack(Disp_Size_H, Disp_Size_V);
    Convert_Buff[2][0][3] = sys_w.screen_mode;
    save_w[SAVEW_ARCADE].Screen_Mode = sys_w.screen_mode;
}

/** @brief Check for screen-adjust exit (confirm / cancel). */
void Screen_Exit_Check(struct _TASK* task_ptr, s16 PL_id) {
    switch (IO_Result) {
    case SWK_EAST:
    case SWK_SOUTH:
        break;

    default:
        return;
    }

    if (Menu_Cursor_Y[0] == 6 || IO_Result == SWK_EAST) {
        SE_selected();
        Menu_Suicide[1] = 0;
        Menu_Suicide[2] = 1;
        X_Adjust = X_Adjust_Buff[2];
        Y_Adjust = Y_Adjust_Buff[2];
        Return_Option_Mode_Sub(task_ptr);

        if (task_ptr->r_no[0] == 1) {
            task_ptr->r_no[1] = 1;
        } else {
            task_ptr->r_no[1] = 7;
            Order[EFF_SLOT_SCREEN_ADJ] = 4;
            Order_Timer[EFF_SLOT_SCREEN_ADJ] = 4;
        }

        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;
        return;
    }

    if (Menu_Cursor_Y[PL_id] == 5) {
        SE_selected();
        X_Adjust_Buff[2] = 0;
        Y_Adjust_Buff[2] = 0;
        Disp_Size_H = 100;
        Disp_Size_V = 100;
        sys_w.screen_mode = 1;
    }
}

/** @brief Screen Adjust left/right value change handler. */
void Screen_Move_Sub_LR(u16 sw) {
    s16 flag = 0;

    if (sw == SWK_LEFT) {
        switch (Menu_Cursor_Y[0]) {
        case 0:
            X_Adjust_Buff[2] -= 2;

            if (X_Adjust_Buff[2] < -10) {
                X_Adjust_Buff[2] = -10;
            } else {
                flag = 1;
            }

            break;

        case 1:
            Y_Adjust_Buff[2] -= 2;

            if (Y_Adjust_Buff[2] < -10) {
                Y_Adjust_Buff[2] = -10;
            } else {
                flag = 1;
            }

            break;

        case 2:
            Disp_Size_H -= 2;

            if (Disp_Size_H < 94) {
                Disp_Size_H = 94;
            } else {
                flag = 1;
            }

            break;

        case 3:
            Disp_Size_V -= 2;

            if (Disp_Size_V < 94) {
                Disp_Size_V = 94;
            } else {
                flag = 1;
            }

            break;

        case 4:
            sys_w.screen_mode = (sys_w.screen_mode + 1) & 1;
            flag = 1;
            break;
        }
    } else if (sw == SWK_RIGHT) {
        switch (Menu_Cursor_Y[0]) {
        case 0:
            X_Adjust_Buff[2] += 2;

            if (X_Adjust_Buff[2] > 10) {
                X_Adjust_Buff[2] = 10;
            } else {
                flag = 1;
            }

            break;

        case 1:
            Y_Adjust_Buff[2] += 2;

            if (Y_Adjust_Buff[2] > 10) {
                Y_Adjust_Buff[2] = 10;
            } else {
                flag = 1;
            }

            break;

        case 2:
            Disp_Size_H += 2;

            if (Disp_Size_H > 100) {
                Disp_Size_H = 100;
            } else {
                flag = 1;
            }

            break;

        case 3:
            Disp_Size_V += 2;

            if (Disp_Size_V > 100) {
                Disp_Size_V = 100;
            } else {
                flag = 1;
            }

            break;

        case 4:
            sys_w.screen_mode = (sys_w.screen_mode + 1) & 1;
            flag = 1;
            break;
        }
    }

    if (flag) {
        SE_dir_cursor_move();
    }

    X_Adjust = X_Adjust_Buff[0] = X_Adjust_Buff[1] = X_Adjust_Buff[2];
    Y_Adjust = Y_Adjust_Buff[0] = Y_Adjust_Buff[1] = Y_Adjust_Buff[2];
}

/** @brief Set sound mode (mono / stereo). */
void Setup_Sound_Mode(u8 last_mode) {
    if (last_mode == Convert_Buff[3][1][0]) {
        return;
    }

    sys_w.sound_mode = Convert_Buff[3][1][0];
    setupSoundMode();
    SsBgmHalfVolume(0);
}

/** @brief Sound Test cursor sub-handler (up/down). */
u16 Sound_Cursor_Sub(s16 PL_id) {
    u16 sw;
    u16 ret;

    sw = ~plsw_01[PL_id] & plsw_00[PL_id];
    sw = Check_Menu_Lever(PL_id, 0);
    ret = MC_Move_Sub(sw, 0, SOUND_TEST_MAX, 0xFF);
    ret |= SD_Move_Sub_LR(sw);
    ret &= RESULT_MASK;
    return ret;
}

const u8 Sound_Data_Max[3][6] = { { 1, 0, 0, 1, 0, 66 }, { 1, 15, 15, 1, 0, 66 }, { 0, 15, 15, 0, 0, 0 } };

/** @brief Sound Test left/right value change handler. */
u16 SD_Move_Sub_LR(u16 sw) {
    u16 rnum;
    s16 max;
    s8 last_cursor;

    rnum = 0;

    if (Menu_Cursor_Y[0] == 4 || Menu_Cursor_Y[0] == 6) {
        return 0;
    }

    last_cursor = Convert_Buff[3][1][Menu_Cursor_Y[0]];

    switch (sw) {
    case SWK_LEFT:
        max = Sound_Data_Max[0][Menu_Cursor_Y[0]];

        while (1) {
            Convert_Buff[3][1][Menu_Cursor_Y[0]] -= 1;

            if (Convert_Buff[3][1][Menu_Cursor_Y[0]] < 0) {
                Convert_Buff[3][1][Menu_Cursor_Y[0]] = max;
            }

            if ((Menu_Cursor_Y[0] != 5) || (bgmSkipCheck(Convert_Buff[3][1][5] + 1) == 0)) {
                break;
            }
        }

        if (last_cursor != Convert_Buff[3][1][Menu_Cursor_Y[0]]) {
            rnum = 4;
        }

        break;

    case SWK_RIGHT:
        max = Sound_Data_Max[1][Menu_Cursor_Y[0]];

        while (1) {
            Convert_Buff[3][1][Menu_Cursor_Y[0]] += 1;

            if (Convert_Buff[3][1][Menu_Cursor_Y[0]] > max) {
                Convert_Buff[3][1][Menu_Cursor_Y[0]] = Sound_Data_Max[2][Menu_Cursor_Y[0]];
            }

            if ((Menu_Cursor_Y[0] != 5) || (bgmSkipCheck(Convert_Buff[3][1][5] + 1) == 0)) {
                break;
            }
        }

        if (last_cursor != Convert_Buff[3][1][Menu_Cursor_Y[0]]) {
            rnum = 8;
        }

        break;
    }

    if (rnum) {
        SE_dir_cursor_move();
    }

    return rnum;
}

/** @brief Save / Load sub-menu within Memory Card. */
void Save_Load_Menu(struct _TASK* task_ptr) {
    s16 ix;

    Menu_Cursor_X[1] = Menu_Cursor_X[0];

    switch (task_ptr->r_no[3]) {
    case 0:
        task_ptr->r_no[3] += 1;
        task_ptr->timer = 5;

        if (task_ptr->r_no[2] == 5) {
            NativeSave_LoadOptions();
        } else {
            NativeSave_SaveOptions();
        }

        Menu_Common_Init();
        Menu_Suicide[3] = 0;
        Target_BG_X[1] = bg_w.bgw[1].wxy[0].disp.pos + BG_SLIDE_X_MEDIUM;
        Offset_BG_X[1] = 0;
        Target_BG_X[2] = bg_w.bgw[2].wxy[0].disp.pos + BG_SLIDE_X_FULL;
        Offset_BG_X[2] = 0;
        bg_w.bgw[2].speed_x = BG_SPEED_FORWARD;
        Next_Step = 0;
        bg_mvxy.a[0].sp = BG_MOVE_FORWARD;
        bg_mvxy.d[0].sp = 0;
        effect_58_init(EFF58_SCROLL_TYPE, 1, 1);
        effect_58_init(0, 1, 2);
        Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(0, 8);
        Message_Data->kind_req = 5;
        break;

    case 1:
        if (Next_Step) {
            task_ptr->r_no[3] += 1;
            task_ptr->free[3] = 0;
        }

        break;

    case 2:
        task_ptr->r_no[3] += 1;
        Menu_Cursor_X[1] = Menu_Cursor_X[0] + 8;
        /* fallthrough */

    case 3:
        /* Synchronous — always proceed */
        Go_Back_MC(task_ptr);

        break;

    case 4:
        if (Next_Step) {
            task_ptr->r_no[2] = 3;
            task_ptr->r_no[3] = 0;

            for (ix = 0; ix < 4; ix++) {
                Message_Data[ix].order = 3;
            }

            Order[EFF_SLOT_SAVE_DONE] = 3;
            Order_Timer[EFF_SLOT_SAVE_DONE] = 1;
        }

        break;

    default:
        Exit_Sub(task_ptr, 1, Menu_Cursor_Y[0] + 7);
        break;
    }
}

/** @brief Return from Memory Card sub-menu. */
void Go_Back_MC(struct _TASK* task_ptr) {
    task_ptr->r_no[3] = 4;
    Menu_Cursor_Y[0] = task_ptr->r_no[2] - 4;
    Target_BG_X[1] = bg_w.bgw[1].wxy[0].disp.pos - BG_SLIDE_X_MEDIUM;
    Offset_BG_X[1] = 0;
    Target_BG_X[2] = bg_w.bgw[2].wxy[0].disp.pos - BG_SLIDE_X_FULL;
    Offset_BG_X[2] = 0;
    bg_w.bgw[2].speed_x = BG_SPEED_BACKWARD;
    Next_Step = 0;
    bg_mvxy.a[0].sp = BG_MOVE_BACKWARD;
    bg_mvxy.d[0].sp = 0;
    effect_58_init(EFF58_SCROLL_TYPE, 1, 1);
    effect_58_init(0, 1, 2);
}

/** @brief Memory Card cursor sub-handler (up/down). */
void Memory_Card_Sub(s16 PL_id) {
    u16 sw;

    sw = ~plsw_01[PL_id] & plsw_00[PL_id];
    sw = Check_Menu_Lever(PL_id, 0);
    MC_Move_Sub(sw, 0, MEMORY_CARD_MAX, 0xFF);

    if ((Menu_Cursor_Y[0] == 2) && !(IO_Result & SWK_EAST)) {
        IO_Result = 0;
    }

    Memory_Card_Move_Sub_LR(sw, 0);

    if (Convert_Buff[3][0][2] == 0) {
        CurrentSave()->Auto_Save = 0;
    }
}

/** @brief Memory Card left/right value toggle handler. */
u16 Memory_Card_Move_Sub_LR(u16 sw, s16 cursor_id) {
    s32 ret;
    s32 idx;
    s32 val;

    idx = Menu_Cursor_Y[cursor_id];

    if (idx != 2) {
        return 0;
    }

    val = Convert_Buff[3][cursor_id][idx];

    switch (sw) {
    case SWK_LEFT:
        val -= 1;

        if (val < 0) {
            val = 1;
        }

        SE_dir_cursor_move();
        ret = SWK_LEFT;
        break;

    case SWK_RIGHT:
        val += 1;

        if (val > 1) {
            val = 0;
        }

        SE_dir_cursor_move();
        ret = SWK_RIGHT;
        break;

    default:
        ret = 0;
        break;
    }

    Convert_Buff[3][cursor_id][idx] = val;

    if ((ret != 0) && (val == 1)) {
        IO_Result = SWK_SOUTH;
        Forbid_Reset = 1;
    }

    return ret;
}

/** @brief Generic menu cursor move sub-routine (up/down with cancel). */
u16 MC_Move_Sub(u16 sw, s16 cursor_id, s16 menu_max, s16 cansel_menu) {
    if (Menu_Cursor_Move > 0) {
        return 0;
    }

    switch (sw) {
    case SWK_UP:
        Menu_Cursor_Y[cursor_id] -= 1;

        if (Menu_Cursor_Y[cursor_id] < 0) {
            Menu_Cursor_Y[cursor_id] = menu_max;
        }

        if ((cansel_menu == Menu_Cursor_Y[cursor_id]) && (Connect_Status == 0)) {
            Menu_Cursor_Y[cursor_id] -= 1;
        }

        SE_cursor_move();
        return IO_Result = SWK_UP;

    case SWK_DOWN:
        Menu_Cursor_Y[cursor_id] += 1;

        if (Menu_Cursor_Y[cursor_id] > menu_max) {
            Menu_Cursor_Y[cursor_id] = 0;
        }

        if ((cansel_menu == Menu_Cursor_Y[cursor_id]) && (Connect_Status == 0)) {
            Menu_Cursor_Y[cursor_id] += 1;
        }

        SE_cursor_move();
        return IO_Result = SWK_DOWN;

    case SWK_WEST:
        return IO_Result = SWK_WEST;

    case SWK_SOUTH:
        return IO_Result = SWK_SOUTH;

    case SWK_EAST:
        return IO_Result = SWK_EAST;

    case SWK_RIGHT_TRIGGER:
        return IO_Result = SWK_RIGHT_TRIGGER;

    case SWK_START:
        return IO_Result = SWK_START;

    default:
        return IO_Result = 0;

    case SWK_NORTH:
        return IO_Result = SWK_NORTH;

    case SWK_RIGHT_SHOULDER:
        return IO_Result = SWK_RIGHT_SHOULDER;

    case SWK_LEFT_SHOULDER:
        return IO_Result = SWK_LEFT_SHOULDER;

    case SWK_LEFT_TRIGGER:
        return IO_Result = SWK_LEFT_TRIGGER;
    }
}

/** @brief In-game pause menu (button config, exit, etc). */
void Menu_Select(struct _TASK* task_ptr) {
    s16 ix;

    if (Check_Pad_in_Pause(task_ptr) != 0) {
        return;
    }

    switch (task_ptr->r_no[2]) {
    case 0:
        Pause_1st_Sub(task_ptr);
        break;

    case 1:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = Cursor_Y_Pos[0][0];
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 0;
        Menu_Suicide[2] = 0;
        effect_10_init(0, 0, 0, 0, 0, EFF10_PAUSE_CONTINUE, EFF10_LAYER_CONTINUE);
        effect_10_init(0, 0, 2, 2, 0, EFF10_PAUSE_BTNCFG, EFF10_LAYER_BTNCFG);

        switch (Mode_Type) {
        case MODE_VERSUS:
            effect_10_init(0, 0, 1, 5, 0, EFF10_PAUSE_EXIT_VS, EFF10_LAYER_EXIT);
            break;

        case MODE_REPLAY:
            effect_10_init(0, 0, 1, 4, 0, EFF10_PAUSE_EXIT_RPL, EFF10_LAYER_EXIT);
            break;

        default:
            effect_10_init(0, 0, 1, 1, 0, EFF10_PAUSE_EXIT_DEF, EFF10_LAYER_EXIT);
            break;
        }

        break;

    case 2:
        IO_Result = MC_Move_Sub(Check_Menu_Lever(Pause_ID, 0), 0, 2, 0xFF);
        switch (IO_Result) {

        case SWK_EAST:
            task_ptr->r_no[2] = 0;
            Menu_Suicide[0] = 1;
            SE_selected();
            break;

        case SWK_SOUTH:
            switch (Menu_Cursor_Y[0]) {

            case 0:
                task_ptr->r_no[2] = 0;
                Menu_Suicide[0] = 1;
                SE_selected();
                break;

            case 1:
                SE_selected();

                switch (Mode_Type) {
                case MODE_VERSUS:
                    task_ptr->r_no[1] = 3;
                    task_ptr->r_no[2] = 0;
                    task_ptr->r_no[3] = 0;

                    for (ix = 0; ix < 4; ix++) {
                        Menu_Suicide[ix] = 1;
                    }

                    cpExitTask(TASK_SAVER);
                    cpExitTask(TASK_PAUSE);
                    BGM_Stop();
                    break;

                case MODE_REPLAY:
                    task_ptr->r_no[0] = MENU_RNO0_REPLAY_EXIT;
                    task_ptr->r_no[1] = 0;
                    break;

                default:
                    Menu_Suicide[0] = 1;
                    Menu_Suicide[1] = 1;
                    Menu_Suicide[2] = 1;
                    Menu_Suicide[3] = 0;
                    task_ptr->r_no[1]++;
                    task_ptr->r_no[2] = 0;
                    Pause_KillFlash();
                    break;
                }

                break;

            case 2:
                task_ptr->r_no[2]++;
                Menu_Suicide[0] = 1;
                Menu_Cursor_Y[0] = 1;
                effect_10_init(0, 0, 3, 3, 1, EFF10_PAUSE_CONFIRM, EFF10_LAYER_CONFIRM);
                effect_10_init(0, 1, 0, 0, 1, EFF10_PAUSE_YES, EFF10_LAYER_YESNO);
                effect_10_init(0, 1, 1, 1, 1, EFF10_PAUSE_NO, EFF10_LAYER_YESNO);
                SE_selected();
                break;
            }

            break;
        }

        break;

    case 3:
        Yes_No_Cursor_Move_Sub(task_ptr);
        break;
    }
}

/** @brief Yes/No cursor move sub-routine for confirmation dialogs. */
s32 Yes_No_Cursor_Move_Sub(struct _TASK* task_ptr) {
    u16 sw = ~(plsw_01[Pause_ID]) & plsw_00[Pause_ID];

    switch (sw) {
    case SWK_LEFT:
        Menu_Cursor_Y[0]--;

        if (Menu_Cursor_Y[0] < 0) {
            Menu_Cursor_Y[0] = 0;
        } else {
            SE_dir_cursor_move();
        }

        break;

    case SWK_RIGHT:
        Menu_Cursor_Y[0]++;

        if (Menu_Cursor_Y[0] > 1) {
            Menu_Cursor_Y[0] = 1;
        } else {
            SE_dir_cursor_move();
        }

        break;

    case SWK_EAST:
    case SWK_SOUTH:
        if (Menu_Cursor_Y[0] || sw == SWK_EAST) {
            task_ptr->r_no[2] = 1;
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 1;
            Cursor_Y_Pos[0][0] = 2;
            return 1;
        }

        Soft_Reset_Sub();
        return -1;
    }

    return 0;
}

/** @brief Button Config in-game (during pause). */
void Button_Config_in_Game(struct _TASK* task_ptr) {
    if (Check_Pad_in_Pause(task_ptr) != 0) {
        Order[EFF_SLOT_CURSOR_BG] = 3;
        Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        effect_66_init(EFF_SLOT_CURSOR_BG, 9, 2, 7, -1, -1, EFF66_ZORDER_INGAME);
        return;
    }

    switch (task_ptr->r_no[2]) {
    case 0:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Copy_Key_Disp_Work();
        Setup_Button_Sub(6, 5, 3);
        ControllerImageOverlay_Init();
        Order[EFF_SLOT_CURSOR_BG] = 3;
        Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        effect_66_init(EFF_SLOT_PAUSE_BTN1, 0xA, 3, 7, -1, -1, EFF66_ZORDER_INGAME_COL);
        Order[EFF_SLOT_PAUSE_BTN1] = 3;
        Order_Timer[EFF_SLOT_PAUSE_BTN1] = 1;
        effect_66_init(EFF_SLOT_PAUSE_BTN2, 0xB, 3, 7, -1, -1, EFF66_ZORDER_INGAME_COL);
        Order[EFF_SLOT_PAUSE_BTN2] = 3;
        Order_Timer[EFF_SLOT_PAUSE_BTN2] = 1;
        break;

    case 1:
        Button_Config_Sub(0);
        Button_Exit_Check_in_Game(task_ptr, 0);
        Button_Config_Sub(1);
        Button_Exit_Check_in_Game(task_ptr, 1);
        Save_Game_Data();
        break;
    }
}

/** @brief Check for button-config exit during in-game pause. */
void Button_Exit_Check_in_Game(struct _TASK* task_ptr, s16 PL_id) {
    if (IO_Result & SWK_EAST) {
        goto ten;
    }

    if (!(IO_Result & SWK_SOUTH)) {
        return;
    }

    if (Menu_Cursor_Y[PL_id] == 10) {
    ten:
        SE_selected();
        ControllerImageOverlay_Shutdown();
        Return_Pause_Sub(task_ptr);
        return;
    }

    if (Menu_Cursor_Y[PL_id] == 9) {
        SE_selected();
        Setup_IO_ConvDataDefault(PL_id);
    }
}

/** @brief Return from pause sub-menu to game. */
void Return_Pause_Sub(struct _TASK* task_ptr) {
    Menu_Suicide[0] = 0;
    Menu_Suicide[1] = 0;
    Menu_Suicide[2] = 0;
    Menu_Suicide[3] = 1;
    Pause_SetFlashPhase(2);
    Pause_SetFlashTimer(1);
    task_ptr->r_no[1] = 1;
    task_ptr->r_no[2] = 1;
    Cursor_Y_Pos[0][0] = 1;
    Order[138] = 3;
    Order_Timer[138] = 1;
    effect_66_init(138, 9, 2, 7, -1, -1, EFF66_ZORDER_INGAME);
}

/** @brief Check if any pad input occurred during pause. */
s32 Check_Pad_in_Pause(struct _TASK* task_ptr) {
    if (Interface_Type[Pause_ID] == 0) {
        task_ptr->r_no[1] = 4;
        Pause_SetFlashPhase(4);
        Menu_Suicide[0] = 1;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Suicide[3] = 1;
        return 1;
    }

    return 0;
}

/** @brief Pad come-out stub (no-op). */
void Pad_Come_Out(struct _TASK* /* unused */) {}

/** @brief VS Result selection sub-routine (continue / save / exit). */
s32 VS_Result_Select_Sub(struct _TASK* task_ptr, s16 PL_id) {
    u16 sw = Check_Menu_Lever(PL_id, 0);

    if (Menu_Cursor_X[PL_id] == 0) {
        After_VS_Move_Sub(sw, PL_id, 2);

        if (VS_Result_Move_Sub(task_ptr, PL_id) != 0) {
            Pause_ID = PL_id;
            return 1;
        }
    } else if (sw == SWK_EAST) {
        IO_Result = SWK_EAST;
        VS_Result_Move_Sub(task_ptr, PL_id);
    }

    return 0;
}

/** @brief Post-VS cursor move sub-routine. */
u16 After_VS_Move_Sub(u16 sw, s16 cursor_id, s16 menu_max) {
    s16 skip;

    if (plw[0].wu.pl_operator == 0 || plw[1].wu.pl_operator == 0) {
        skip = 1;
    } else {
        skip = 99;
    }
    if (Debug_w[DEBUG_CPU_REPLAY_TEST]) {
        skip = 99;
    }

    switch (sw) {
    case SWK_UP:
        Menu_Cursor_Y[cursor_id]--;

        if (Menu_Cursor_Y[cursor_id] < 0) {
            Menu_Cursor_Y[cursor_id] = menu_max;
        }

        if (Menu_Cursor_Y[cursor_id] == skip) {
            Menu_Cursor_Y[cursor_id] = 0;
        }

        SE_cursor_move();
        return IO_Result = SWK_UP;

    case SWK_DOWN:
        Menu_Cursor_Y[cursor_id]++;

        if (Menu_Cursor_Y[cursor_id] > menu_max) {
            Menu_Cursor_Y[cursor_id] = 0;
        }

        if (Menu_Cursor_Y[cursor_id] == skip) {
            Menu_Cursor_Y[cursor_id] = 2;
        }

        SE_cursor_move();
        return IO_Result = SWK_DOWN;

    case SWK_WEST:
        return IO_Result = SWK_WEST;

    case SWK_SOUTH:
        return IO_Result = SWK_SOUTH;

    case SWK_EAST:
        return IO_Result = SWK_EAST;

    case SWK_RIGHT_TRIGGER:
        return IO_Result = SWK_RIGHT_TRIGGER;

    case SWK_START:
        return IO_Result = SWK_START;

    default:
        return IO_Result = 0;

    case SWK_NORTH:
        return IO_Result = SWK_NORTH;

    case SWK_RIGHT_SHOULDER:
        return IO_Result = SWK_RIGHT_SHOULDER;

    case SWK_LEFT_SHOULDER:
        return IO_Result = SWK_LEFT_SHOULDER;

    case SWK_LEFT_TRIGGER:
        return IO_Result = SWK_LEFT_TRIGGER;
    }
}

/** @brief VS Result move sub-routine (navigate result list). */
s32 VS_Result_Move_Sub(struct _TASK* task_ptr, s16 PL_id) {
    switch (IO_Result) {
    case SWK_SOUTH:
        switch (Menu_Cursor_Y[PL_id]) {
        case 0:
            SE_selected();
            Menu_Cursor_X[PL_id] = 1;

            if (!Menu_Cursor_X[PL_id ^ 1]) {
                break;
            }

            task_ptr->r_no[2] = 6;
            task_ptr->r_no[3] = 0;
            task_ptr->timer = 15;
            return 1;

        case 1:
            SE_selected();
            task_ptr->r_no[2] = 5;
            task_ptr->r_no[3] = 0;
            task_ptr->timer = 15;
            return 1;

        case 2:
            SE_selected();
            task_ptr->r_no[2] = 7;
            task_ptr->r_no[3] = 0;
            task_ptr->timer = 15;
            return 1;
        }

        break;

    case SWK_EAST:
        SE_selected();

        if (Menu_Cursor_X[PL_id]) {
            Menu_Cursor_X[PL_id] = 0;
            break;
        }

        if (Menu_Cursor_Y[PL_id] == 2) {
            task_ptr->r_no[2] = 99;
            return 1;
        }

        Menu_Cursor_Y[PL_id] = 2;
        break;
    }

    return 0;
}

/** @brief Save Replay step 1 â€” prepare save operation. */
void Setup_Save_Replay_1st(struct _TASK* task_ptr) {
    FadeOut(1, FADE_OPAQUE, 8);
    task_ptr->r_no[2]++;
    task_ptr->timer = 5;
    Menu_Common_Init();
    Menu_Cursor_X[0] = 0;
    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 0;
    Menu_Suicide[2] = 0;
    Menu_Suicide[3] = 0;
    Setup_BG(1, 512, 0);
    /* Native replay setup skipped — RmlUI replay picker always used */
    Setup_File_Property(1, FADE_OPAQUE);
    Clear_Flash_Init(4);
}

/** @brief Return to VS Result screen after replay-save. */
void Return_VS_Result_Sub(struct _TASK* task_ptr) {
    Menu_Suicide[0] = 0;
    Menu_Suicide[1] = 1;
    task_ptr->r_no[1] = 16;
    task_ptr->r_no[2] = 1;
    task_ptr->r_no[3] = 0;
    task_ptr->free[0] = 0;
    Order[110] = 4;
    Order_Timer[110] = 1;
}

/** @brief Memory-card replay save sub-routine with error handling. */
s32 Save_Replay_MC_Sub(struct _TASK* task_ptr, s16 /* unused */) {
    switch (IO_Result) {
    case SWK_SOUTH:
        SE_selected();

        if (Menu_Cursor_X[0] == -1) {
            break;
        }

        if (vm_w.Connect[Menu_Cursor_X[0]] == 0) {
            break;
        }

        vm_w.Drive = (u8)Menu_Cursor_X[0];

        if (VM_Access_Request(6, Menu_Cursor_X[0]) == 0) {
            break;
        }

        task_ptr->free[1] = 0;
        task_ptr->free[2] = 0;
        task_ptr->r_no[0] = 3;
        return 1;

    case SWK_EAST:
        if (Mode_Type == 5) {
            Back_to_Mode_Select(task_ptr);
        } else {
            Exit_Replay_Save(task_ptr);
        }

        return 1;
    }

    return 0;
}

/** @brief Exit replay save and return to VS result. */
void Exit_Replay_Save(struct _TASK* task_ptr) {
    if (task_ptr->r_no[1] == 17) {
        Return_VS_Result_Sub(task_ptr);
        return;
    }

    Menu_Suicide[0] = 0;
    Menu_Suicide[1] = 0;
    Menu_Suicide[2] = 1;
    task_ptr->r_no[1] = 5;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->free[0] = 0;
    Order[112] = 4;
    Order_Timer[112] = 4;
}

/** @brief Mark a player as the decider for menu choices. */
void Decide_PL(s16 PL_id) {
    plw[PL_id].wu.pl_operator = 1;
    Operator_Status[PL_id] = 1;
    Champion = PL_id;
    plw[PL_id ^ 1].wu.pl_operator = 0;
    Operator_Status[PL_id ^ 1] = 0;

    if (Continue_Coin[PL_id] == 0) {
        grade_check_work_1st_init(PL_id, 0);
    }
}

/** @brief Determine which player controls menus in training. */
void Control_Player_Tr() {
    switch (control_pl_rno) {
    case 0:
        if (control_player) {
            p2sw_0 = 0;
            break;
        }

        p1sw_0 = 0;
        break;

    case 1:
        if (control_player) {
            p2sw_0 = 2;
            break;
        }

        p1sw_0 = 2;
        break;

    case 2:
        if (control_player) {
            p2sw_0 = 1;
            break;
        }

        p1sw_0 = 1;
        break;
    }
}

/** @brief Transition to next branch in training menu. */
void Next_Be_Tr_Menu(struct _TASK* task_ptr) {
    s16 ix;

    apply_training_hitbox_display(true);
    task_ptr->r_no[0] = 11;
    task_ptr->r_no[1] = 0;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    Allow_a_battle_f = 0;

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 1;
    }

    SsBgmHalfVolume(0);
}

/** @brief Check if training-pause should terminate. */
s32 Check_Pause_Term_Tr(s16 PL_id) {
    if (Mode_Type == MODE_PARRY_TRAINING) {
        if (PL_id == Champion) {
            return 1;
        }

        return 0;
    }

    if (PL_id == Champion) {
        return 1;
    }

    if (Training->contents[0][1][3] == 2) {
        return 0;
    }

    if (Training->contents[0][0][0] == 4) {
        return 1;
    }

    return 0;
}

/** @brief Check controller input for training-mode pause. */
s32 Pause_Check_Tr(s16 PL_id) {
    u16 sw;

    if (plw[PL_id].wu.pl_operator == 0) {
        return 0;
    }

    sw = ~(PLsw[PL_id][1]) & PLsw[PL_id][0];

    if (sw & SWK_START) {
        Pause_ID = PL_id;
        return 1;
    }

    if (Interface_Type[PL_id] == 0) {
        Pause_ID = PL_id;
        return 2;
    }

    return 0;
}

/** @brief Set up training-mode pause screen. */
void Setup_Tr_Pause(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 2;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->free[0] = 60;
    Cursor_Y_Pos[0][0] = 0;
    Disp_Attack_Data = 0;
    Game_pause = GAME_PAUSE_ACTIVE;
    Pause_Down = 1;
    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 1;
    Menu_Suicide[2] = 0;
    Order[138] = 3;
    Order_Timer[138] = 1;
    effect_66_init(138, 9, 2, 7, -1, -1, EFF66_ZORDER_INGAME);
    SsBgmHalfVolume(1);
    spu_all_off();
}

/** @brief Normal-training pause menu handler. */
s32 Pause_in_Normal_Tr(struct _TASK* task_ptr) {
    s16 ix;
    u16 sw;

    Control_Player_Tr();

    switch (task_ptr->r_no[2]) {
    case 0:
        return Pause_1st_Sub(task_ptr);

    case 1:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = Cursor_Y_Pos[0][0];

        for (ix = 0; ix < 4; ix++) {
            Menu_Suicide[ix] = 0;
        }

        effect_10_init(0, 6, 0, 0, 0, 20, 12);
        effect_10_init(0, 6, 1, 1, 0, 18, 14);
        effect_10_init(0, 6, 2, 2, 0, 22, 16);
        break;

    case 2:
        if (Pause_Down) {
            IO_Result = MC_Move_Sub(Check_Menu_Lever(Pause_ID, 0), 0, 2, 0xFF);
        } else {
            sw = ~PLsw[Pause_ID][1] & PLsw[Pause_ID][0];

            if (sw & SWK_ATTACKS) {
                IO_Result = SWK_WEST;
            } else {
                return 3;
            }
        }

        switch (IO_Result) {
        case SWK_EAST:
            task_ptr->r_no[2] = 0;
            Menu_Suicide[0] = 1;
            SE_selected();
            break;

        case SWK_SOUTH:
            switch (Menu_Cursor_Y[0]) {
            case 0:
                task_ptr->r_no[2] = 0;
                Menu_Suicide[0] = 1;
                SE_selected();
                break;

            case 1:
                Cursor_Y_Pos[0][0] = 0;
                return 2;

            case 2:
                task_ptr->r_no[2]++;
                SE_selected();
                Menu_Suicide[0] = 1;
                Menu_Cursor_Y[0] = 1;
                effect_10_init(0, 0, 3, 6, 1, 17, 12);
                effect_10_init(0, 1, 0, 0, 1, 20, 15);
                effect_10_init(0, 1, 1, 1, 1, 26, 15);
                break;
            }

            break;
        }

        break;

    case 3:
        sw = ~plsw_01[Pause_ID] & plsw_00[Pause_ID];

        if (Pause_Down) {
            Yes_No_Cursor_Move_Sub(task_ptr);
        }

        break;
    }

    return 0;
}

/** @brief Pause step 1 sub-routine â€” select/cancel handling. */
s32 Pause_1st_Sub(struct _TASK* task_ptr) {
    u16 sw = ~plsw_01[Pause_ID] & plsw_00[Pause_ID];

    if (Pause_Down && (!use_rmlui || !rmlui_screen_pause)) {
        SSPutStr2(17, 12, 9, "PRESS   BUTTON");
        dispButtonImage2(BTN_IMG_POS_X, BTN_IMG_POS_Y, 1, EFF10_PAUSE_CONFIRM, EFF10_LAYER_YESNO, 0, 4);
        SSPutStr2(18, 14, 9, "TO PAUSE MENU");
    }

    if (sw & SWK_START) {
        if (((Mode_Type == MODE_NORMAL_TRAINING) || (Mode_Type == MODE_PARRY_TRAINING) || (Mode_Type == MODE_TRIALS)) &&
            (Check_Pause_Term_Tr(Pause_ID ^ 1) != 0) && plw[Pause_ID ^ 1].wu.pl_operator &&
            (Interface_Type[Pause_ID ^ 1] == 0)) {
            Pause_ID = Pause_ID ^ 1;
            return 0;
        }

        task_ptr->r_no[2] = MENU_RNO2_PAUSE_EXIT;
        Exit_Menu = 1;
        SE_selected();
        return 1;
    }

    if (sw & SWK_SOUTH) {
        task_ptr->r_no[2] += 1;
        Cursor_Y_Pos[0][0] = 0;
        SE_selected();
    }

    return 0;
}

/** @brief Set up Normal Training data for the given index. */
void Setup_NTr_Data(s16 ix) {
    switch (ix) {
    case 0:
        Play_Mode = 0;
        Replay_Status[0] = 0;
        Replay_Status[1] = 0;
        CurrentSave()->Time_Limit = -1;
        CurrentSave()->Damage_Level = Training[2].contents[0][1][2];
        Training[0] = Training[2];
        break;

    case 1:
        Record_Data_Tr = 1;
        Play_Mode = 1;
        Replay_Status[0] = 1;
        Replay_Status[1] = 1;
        CurrentSave()->Time_Limit = 60;
        CurrentSave()->Damage_Level = Training[2].contents[0][1][2];
        Training[0] = Training[2];
        Training[1] = Training[2];
        break;

    case 2:
        Play_Mode = 3;
        Replay_Status[0] = 3;
        Replay_Status[1] = 3;
        CurrentSave()->Time_Limit = 60;
        CurrentSave()->Damage_Level = Training[1].contents[0][1][2];
        Training[0] = Training[1];
        break;
    }

    apply_training_hitbox_display(false);
    sync_dummy_settings_from_menu();
}

/** @brief Check and skip replay at the given index. */
void Check_Skip_Replay(s16 ix) {
    if (Menu_Cursor_Y[0] != ix) {
        return;
    }

    if (Record_Data_Tr != 0) {
        return;
    }

    if (Menu_Cursor_Y[0] >= Menu_Cursor_Y[1]) {
        Menu_Cursor_Y[0]++;
        return;
    }

    Menu_Cursor_Y[0]--;
    Check_Skip_Recording();
}

/** @brief Check and skip recording input. */
void Check_Skip_Recording() {
    if (Menu_Cursor_Y[0] != 1) {
        return;
    }

    if (Training->contents[0][0][0] != 3) {
        return;
    }

    if (Menu_Cursor_Y[0] >= Menu_Cursor_Y[1]) {
        Menu_Cursor_Y[0]++;
        Check_Skip_Replay(2);
        return;
    }

    Menu_Cursor_Y[0]--;
}

/** @brief Yes/No cursor handler for exiting training mode. */
void Yes_No_Cursor_Exit_Training(struct _TASK* task_ptr, s16 cursor_id) {
    u16 sw = ~(plsw_01[Decide_ID]) & plsw_00[Decide_ID];

    switch (sw) {
    case SWK_LEFT:
        Menu_Cursor_Y[0]--;

        if (Menu_Cursor_Y[0] < 0) {
            Menu_Cursor_Y[0] = 0;
            break;
        }

        SE_dir_cursor_move();
        break;

    case SWK_RIGHT:
        Menu_Cursor_Y[0]++;

        if (Menu_Cursor_Y[0] > 1) {
            Menu_Cursor_Y[0] = 1;
            break;
        }

        SE_dir_cursor_move();
        break;

    case SWK_EAST:
    case SWK_SOUTH:
        SE_selected();

        if (Menu_Cursor_Y[0] || sw == SWK_EAST) {
            task_ptr->r_no[2] = 0;
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 1;
            Cursor_Y_Pos[0][0] = cursor_id;
            break;
        }

        Soft_Reset_Sub();
        break;
    }
}

/** @brief Button Config during training mode. */
void Button_Config_Tr(struct _TASK* task_ptr) {
    switch (task_ptr->r_no[2]) {
    case 0:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 5;
        Copy_Key_Disp_Work();
        Setup_Button_Sub(6, 5, 1);
        ControllerImageOverlay_Init();
        pp_operator_check_flag(0);
        break;

    case 1:
        Button_Config_Sub(0);
        Button_Exit_Check_in_Tr(task_ptr, 0);
        Button_Config_Sub(1);
        Button_Exit_Check_in_Tr(task_ptr, 1);
        Save_Game_Data();
        break;
    }
}

/** @brief Check for button-config exit during training mode. */
void Button_Exit_Check_in_Tr(struct _TASK* task_ptr, s16 PL_id) {
    if (IO_Result & SWK_EAST) {
        goto ten;
    }

    if (!(IO_Result & SWK_SOUTH)) {
        return;
    }

    if (Menu_Cursor_Y[PL_id] == 10) {
    ten:
        SE_selected();
        ControllerImageOverlay_Shutdown();
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;

        if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS) {
            task_ptr->r_no[1] = 1;
        } else {
            task_ptr->r_no[1] = 2;
        }

        pp_operator_check_flag(1);
        return;
    }

    if (Menu_Cursor_Y[PL_id] == 9) {
        SE_selected();
        Setup_IO_ConvDataDefault(PL_id);
    }
}

/** @brief Dummy cursor move sub-routine (up/down). */
void Dummy_Move_Sub(struct _TASK* task_ptr, s16 PL_id, s16 id, s16 type, s16 max) {
    u16 sw = ~(plsw_01[PL_id]) & plsw_00[PL_id];

    sw = Check_Menu_Lever(PL_id, 0);
    MC_Move_Sub(sw, 0, max, 0xFF);
    Dummy_Move_Sub_LR(sw, id, type, 0);

    if (IO_Result & SWK_EAST) {
        task_ptr->r_no[2]++;
        return;
    }

    if (IO_Result & SWK_SOUTH && Menu_Cursor_Y[0] == max) {
        task_ptr->r_no[2]++;
    }
}

const u8 Menu_Max_Data_Tr[2][2][6] = { { { 4, 3, 4, 6, 6, 0 }, { 3, 2, 3, 7, 0, 0 } },
                                       { { 2, 3, 1, 3, 0, 0 }, { 0, 0, 0, 0, 0, 0 } } };

static bool is_data_plus_hitboxes_option_selected() {
    return Training[0].contents[0][1][1] == 2;
}

static void apply_training_hitbox_display(bool force_off) {
    if (force_off || Mode_Type != MODE_NORMAL_TRAINING || !is_data_plus_hitboxes_option_selected()) {
        Set_Training_Hitbox_Display(false);
    } else {
        Set_Training_Hitbox_Display(true);
    }
}

void sync_dummy_settings_from_menu() {
    /* When the Lua dummy is active, settings come from the RmlUI bridge
       instead of the fragile Training[] array.  Skip the cast chain. */
    if (g_lua_dummy_active)
        return;

    /* Original CPS3 Dummy Setting menu items (indices 1-4 of page 0):
       0 = Action (handled natively), 1 = Block, 2 = Parry,
       3 = Stun Escape, 4 = Wakeup.
       Extended settings (tech throw, fast wakeup, guard direction,
       wakeup reversal) live exclusively in the RmlUI layer. */
    g_dummy_settings.block_type = (DummyBlockType)Training[2].contents[0][0][1];
    g_dummy_settings.parry_type = (DummyParryType)Training[2].contents[0][0][2];
    g_dummy_settings.stun_mash = (DummyMashType)Training[2].contents[0][0][3];
    g_dummy_settings.wakeup_mash = (DummyMashType)Training[2].contents[0][0][4];
}

/** @brief Dummy cursor move left/right value toggle handler. */
void Dummy_Move_Sub_LR(u16 sw, s16 id, s16 type, s16 cursor_id) {
    s16 max = Menu_Max_Data_Tr[id][type][Menu_Cursor_Y[cursor_id]];

    if (max == 0) {
        return;
    }

    switch (sw) {
    case SWK_LEFT:
        Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]]--;

        if (Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] < 0) {
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] = max;
        }

        if (Interface_Type[Champion ^ 1] == 0 && id == 0 && type == 0 && Menu_Cursor_Y[cursor_id] == 0 &&
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] == 4) {
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] = 3;
        }

        SE_dir_cursor_move();
        break;

    case SWK_RIGHT:
        Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]]++;

        if (Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] > max) {
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] = 0;
        }

        if (Interface_Type[Champion ^ 1] == 0 && id == 0 && type == 0 && Menu_Cursor_Y[cursor_id] == 0 &&
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] == 4) {
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] = 0;
        }

        SE_dir_cursor_move();
        break;

    default:
        if (Interface_Type[Champion ^ 1] == 0 && id == 0 && type == 0 && Menu_Cursor_Y[cursor_id] == 0 &&
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] == 4) {
            Training[2].contents[id][type][Menu_Cursor_Y[cursor_id]] = 0;
        }

        break;
    }

    sync_dummy_settings_from_menu();
}

/** @brief Training init sub-routine â€” reset state before entering. */
void Training_Init_Sub(struct _TASK* task_ptr) {
    s16 ix;

    task_ptr->r_no[2]++;
    Menu_Common_Init();
    Menu_Cursor_Y[0] = Training_Cursor;

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
    }
}

/** @brief Training exit sub-routine â€” clean up state on leaving. */
void Training_Exit_Sub(struct _TASK* task_ptr) {
    task_ptr->r_no[2]++;
    Menu_Suicide[0] = 1;
    Menu_Cursor_Y[0] = 1;
    effect_10_init(0, 0, 3, 6, 1, 17, 12);
    effect_10_init(0, 1, 0, 0, 1, 20, 15);
    effect_10_init(0, 1, 1, 1, 1, 26, 15);
}

/** @brief Reset training option settings to defaults. */
void Default_Training_Option() {
    Training->contents[0][1][0] = 0;
    Training->contents[0][1][1] = 0;
    Training->contents[0][1][2] = save_w->Damage_Level;
    Training->contents[0][1][3] = save_w->Difficulty;
    CurrentSave()->Damage_Level = save_w->Damage_Level;
    CurrentSave()->Difficulty = save_w->Difficulty;
    Training[2] = Training[0];
    Disp_Attack_Data = 0;
}

/** @brief Return to Mode Select from a sub-menu. */
void Back_to_Mode_Select(struct _TASK* task_ptr) {
    s16 ix;

    FadeOut(1, FADE_OPAQUE, 8);
    G_No[0] = 2;
    G_No[1] = 12;
    G_No[2] = 0;
    G_No[3] = 0;
    E_No[0] = 1;
    E_No[1] = 2;
    E_No[2] = 2;
    E_No[3] = 0;
    System_all_clear_Level_B();
    MenuScreen_ExitToLegacy(task_ptr);
    Menu_Init(task_ptr);

    for (ix = 0; ix < 4; ix++) {
        task_ptr->r_no[ix] = 0;
    }

    BGM_Request_Code_Check(BGM_CODE_MODE_SELECT);

    Mode_Type = MODE_ARCADE;
    Play_Mode = 0;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;
}

/** @brief Extra Option left/right value toggle handler. */
void Ex_Move_Sub_LR(u16 sw, s16 PL_id) {
    u8 last_pos = CurrentSave()->extra_option.contents[Menu_Page][Menu_Cursor_Y[0]];

    switch (sw) {
    case SWK_LEFT:
        if (Menu_Page_Buff != 0 || Menu_Cursor_Y[0] != 4) {
            SE_dir_cursor_move();
        }

        save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]]--;

        if (Menu_Cursor_Y[0] == Menu_Max) {
            if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] < 0) {
                save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] = 0;
                IO_Result = SWK_LEFT_SHOULDER;
                break;
            }

            if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] != last_pos) {
                Message_Data->order = 1;
                Message_Data->request = save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Max] + MSG_EX_PAGE_OFFSET;
                Message_Data->timer = 2;
            }
        } else if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] < 0) {
            save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] =
                Ex_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]];
        }

        return;

    case SWK_RIGHT:
        if (Menu_Page_Buff != 0 || Menu_Cursor_Y[0] != 4) {
            SE_dir_cursor_move();
        }

        save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]]++;

        if (Menu_Cursor_Y[0] == Menu_Max) {
            if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] > 2) {
                save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] = 2;
                IO_Result = SWK_RIGHT_TRIGGER;
                return;
            }

            if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] > 2) {
                save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] = 2;
            }

            if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] != last_pos) {
                Message_Data->order = 1;
                Message_Data->request = save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Max] + MSG_EX_PAGE_OFFSET;
                Message_Data->timer = 2;
            }
        } else if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] >
                   Ex_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]]) {
            save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] = 0;
        }

        return;

    case SWK_RIGHT_TRIGGER:
        if (Interface_Type[PL_id] == 2) {
            break;
        }

    case SWK_SOUTH:
        if (Menu_Page_Buff != 0 || Menu_Cursor_Y[0] != 4) {
            SE_dir_cursor_move();
        }

        if (Menu_Cursor_Y[0] == Menu_Max) {
            break;
        }

        save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]]++;

        if (save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] >
            Ex_Menu_Max_Data[Menu_Page][Menu_Cursor_Y[0]]) {
            save_w[SAVEW_ARCADE].extra_option.contents[Menu_Page_Buff][Menu_Cursor_Y[0]] = 0;
        }

        return;
    }
}
