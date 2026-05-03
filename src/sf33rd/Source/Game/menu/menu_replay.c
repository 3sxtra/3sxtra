#include "sf33rd/Source/Game/menu/menu_replay.h"
#include "game_state.h"
#include "sf33rd/Source/Game/menu/menu_network.h"
#include "sf33rd/Source/Game/menu/menu_network_constants.h"
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"
#include "sf33rd/Source/Game/menu/menu_save.h"
#include "sf33rd/Source/Game/menu/menu_training.h"
#include "port/init_task.h"
#include "port/menu_task.h"
#include "port/task_api.h"
#include "port/menu_screen.h"
#include "common.h"
#include "main.h"
#include "netplay/discovery.h"
#include "netplay/netplay.h"
#include "netplay/ping_probe.h"
#include "port/config/config.h"
#include "port/rendering/renderer.h"
#include "port/save/native_save.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/input/controller_image_overlay.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect_04_projectile_object.h"
#include "sf33rd/Source/Game/effect/effect_10_ui_screen_check_data.h"
#include "sf33rd/Source/Game/effect/effect_18_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_23_quake.h"
#include "sf33rd/Source/Game/effect/effect_38_quake_base_xy.h"
#include "sf33rd/Source/Game/effect/effect_39_quake.h"
#include "sf33rd/Source/Game/effect/effect_40_position_data.h"
#include "sf33rd/Source/Game/effect/effect_43_game_state.h"
#include "sf33rd/Source/Game/effect/effect_45_debug_game_state.h"
#include "sf33rd/Source/Game/effect/effect_51_brief_system_direction_menu_selected_value_label_renderer.h"
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_61_menu_options.h"
#include "sf33rd/Source/Game/effect/effect_63_quake.h"
#include "sf33rd/Source/Game/effect/effect_64_quake.h"
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h"
#include "sf33rd/Source/Game/effect/effect_75_quake_link.h"
#include "sf33rd/Source/Game/effect/effect_91_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a0_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a3_content_check_system.h"
#include "sf33rd/Source/Game/effect/effect_a8_position_data.h"
#include "sf33rd/Source/Game/effect/effect_c4_menu_ex_data.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_k6_quake.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/io/vm_sub.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/menu/ex_data.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "sf33rd/Source/Game/rendering/color_palette.h"
#include "sf33rd/Source/Game/rendering/memory_texture_control.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/rendering/texture_group.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/ram_control.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/save_manager.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/system_director.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/round_timer.h"
#include "sf33rd/Source/Game/ui/hud_data.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"
#include "structs.h"
#include "port/sdl/rmlui/rmlui_button_config.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_exit_confirm.h"
#include "port/sdl/rmlui/rmlui_extra_option.h"
#include "port/sdl/rmlui/rmlui_game_option.h"
#include "port/sdl/rmlui/rmlui_memory_card.h"
#include "port/sdl/rmlui/rmlui_mode_menu.h"
#include "port/sdl/rmlui/rmlui_leaderboard.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_option_menu.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_replay_picker.h"
#include "port/sdl/rmlui/rmlui_sound_menu.h"
#include "port/sdl/rmlui/rmlui_sysdir.h"
#include "port/sdl/rmlui/rmlui_training_menus.h"
#include "port/sdl/rmlui/rmlui_vs_result.h"
#include "port/sdl/rmlui/rmlui_vs_screen.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include "sf33rd/Source/Game/menu/menu_replay_constants.h"
#include "sf33rd/Source/Game/menu/menu_input_constants.h"

void Wait_Replay_Check(struct _TASK* task_ptr) {
    switch (task_ptr->free[1]) {
    case 0:
        if (g_state.vm_w.Request != 0) {
            break;
        }

        task_ptr->r_no[0] = 0;
        task_ptr->r_no[3] = 0;

        if (g_state.vm_w.Number == 0 && g_state.vm_w.New_File == 0) {
            task_ptr->r_no[2] = 3;
            break;
        }

        task_ptr->r_no[2] = 5;
        break;
    }
}

void Setup_Save_Replay_2nd(struct _TASK* task_ptr, s16 arg1) {
    if (FadeIn(1, 25, 8)) {
        task_ptr->r_no[2]++;
        task_ptr->free[3] = 0;
        g_state.Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(g_state.Menu_Cursor_X[0], 8);
    }
}

void Setup_Replay_Sub(s16 type, MenuHeader char_type, s16 master_player) {
    effect_57_init(type, char_type, 0, REPLAY_Z_HEADER, 2);
    g_state.Order[type] = 1;
    g_state.Order_Dir[type] = 8;
    g_state.Order_Timer[type] = 1;
    effect_66_init(EFF_SLOT_CURSOR_BG, REPLAY_SPRITE_SETUP_BG, master_player, 0, -1, -1, REPLAY_Z_DEPTH_SETUP);
    g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
    g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
}

void Reset_Replay(struct _TASK* task_ptr) {
    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        task_ptr->timer = 10;
        g_state.Game_pause = GAME_PAUSE_ACTIVE;
        break;

    case 1:
        if (--task_ptr->timer != 0) {
            break;
        }

        if (Check_LDREQ_Break() == 0) {
            task_ptr->r_no[1]++;
            Switch_Screen_Init(0);
            break;
        }

        task_ptr->timer = 1;
        break;

    case 2:
        if (!Switch_Screen(0)) {
            break;
        }

        task_ptr->r_no[1]++;
        task_ptr->timer = 2;
        g_state.fsm[2] = GAME_SUBMODE_REPLAY;
        g_state.fsm[3] = 0;
        g_state.seraph_flag = 0;
        g_state.fsm_timer = 10;
        g_state.Cover_Timer = 5;
        effect_work_kill_mod_plcol();
        move_effect_work(6);
        g_state.Suicide[0] = 1;
        g_state.Suicide[6] = 1;
        g_state.judge_flag = 0;
        cpExitTask(TASK_PAUSE);
        break;

    default:
        Switch_Screen(0);

        if (--task_ptr->timer == 0) {
            cpExitTask(TASK_MENU);
        }

        break;
    }
}

void Wait_Replay_Load(void) {}

/** @brief After-replay results screen and menu. */
void After_Replay(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_ix;

    s16 s5;
    s16 s4;
    s16 s3;
    s16 s2;

    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        ToneDown(REPLAY_TONE_NORMAL, 32);
        Menu_Common_Init();
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Cursor_Y[0] = 0;

        for (ix = 0, s5 = char_ix = '8'; ix < 3; ix++, s4 = char_ix++) {
            effect_61_init(0, ix + EFF_SLOT_REPLAY_MARKER, 0, 0, char_ix, ix, REPLAY_Z_DEPTH_MARKER);
            g_state.Order[ix + EFF_SLOT_REPLAY_MARKER] = 3;
            g_state.Order_Timer[ix + EFF_SLOT_REPLAY_MARKER] = 1;
        }

        effect_66_init(EFF_SLOT_CURSOR_BG, REPLAY_SPRITE_AFTER_BG, 0, 0, -1, -1, REPLAY_Z_DEPTH_AFTER);
        g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
        g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        break;

    case 1:
        ToneDown(REPLAY_TONE_NORMAL, 32);
        g_state.Pause_ID = 0;

        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 2, REPLAY_RESULT_MASK) == 0) {
            g_state.Pause_ID = 1;
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 2, REPLAY_RESULT_MASK);
        }

        switch (g_state.IO_Result) {
        case SWK_SOUTH:
            SE_selected();
            task_ptr->r_no[1] = g_state.Menu_Cursor_Y[0] + 2;
            break;

        case SWK_EAST:
            SE_selected();
            task_ptr->r_no[1] = 4;
            break;
        }

        break;

    case 4:
        ToneDown(REPLAY_TONE_NORMAL, 32);
        Back_to_Mode_Select(task_ptr);
        break;

    case 2:
        ToneDown(REPLAY_TONE_NORMAL, 32);
        task_ptr->r_no[1] = 12;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;

    case 12:
        Load_Replay_Sub(task_ptr);
        break;

    case 3:
        task_ptr->free[0] = 0;
        task_ptr->r_no[1] = 5;
        task_ptr->r_no[2] = 0;

    case 5:
        ToneDown(REPLAY_TONE_NORMAL, 32);

        if (Exit_Sub(task_ptr, 0, 6)) {
            g_state.Menu_Suicide[0] = 1;
            g_state.Menu_Suicide[1] = g_state.Menu_Suicide[2] = g_state.Menu_Suicide[3] = 0;
        }

        break;

    case 6:
        ToneDown(REPLAY_TONE_DARK, 32);
        switch (task_ptr->r_no[2]) {
        case 0:
            FadeOut(1, FADE_OPAQUE, 8);
            task_ptr->r_no[2]++;
            task_ptr->timer = 5;
            Menu_Common_Init();
            g_state.Menu_Cursor_X[0] = 0;
            if (!rmlui_menu_replay)
                Setup_BG(1, BG_SLIDE_X_FULL, 0);
            if (!rmlui_menu_replay) {
                effect_57_init(EFF_SLOT_REPLAY_HDR, MENU_HEADER_REPLAY, 0, REPLAY_Z_HEADER, 999);
                g_state.Order[EFF_SLOT_REPLAY_HDR] = 3;
                g_state.Order_Dir[EFF_SLOT_REPLAY_HDR] = 8;
                g_state.Order_Timer[EFF_SLOT_REPLAY_HDR] = 1;
            } else {
                /* Blue background — same pattern as Network_Lobby case 11:
                 * effect_work_init() destroys all old effects, then we create
                 * a fresh blue BG on slot 0x4E. */
                effect_work_init();
                Message_Data->kind_req = NET_BG_MODE_BLUE;
                g_state.Order[0x4E] = 5;
                g_state.Order_Timer[0x4E] = 1;
                g_state.Order_Dir[0x4E] = 1;
                effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);
            }
            if (!rmlui_menu_replay)
                Setup_File_Property(1, REPLAY_FILE_PROPERTY_ALL);
            rmlui_replay_picker_open(1); /* always use RmlUI — ImGui removed */
            if (!rmlui_menu_replay) {
                effect_66_init(EFF_SLOT_CURSOR_BG, REPLAY_SPRITE_PICKER_BG, 0, 0, -1, -1, REPLAY_Z_DEPTH_PICKER);
                g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
                g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
            }
            break;

        case 1:
            Menu_Sub_case1(task_ptr);
            break;

        case 2:
            Setup_Save_Replay_2nd(task_ptr, 1);
            break;

        case 3: {
            int pick_result = rmlui_replay_picker_poll();
            if (pick_result == 0) {
                const char* filename = rmlui_replay_picker_get_filename();
                if (!filename || !filename[0]) {
                    NativeSave_AutoSaveReplay(0); /* 0 = local */
                } else {
                    /* Overwriting an existing replay: delete old, create new */
                    NativeSave_DeleteReplay(filename);
                    NativeSave_AutoSaveReplay(0); /* 0 = local */
                }
            }
            if (pick_result == 1)
                break; /* still active */
        }

            task_ptr->r_no[2]++;
            /* fallthrough */

        case 4:
            Exit_Sub(task_ptr, 0, 7);
            break;
        }

        break;

    case 7:
        FadeOut(1, FADE_OPAQUE, 8);
        g_state.Order[EFF_SLOT_REPLAY_HDR] = 4;
        g_state.Order_Timer[EFF_SLOT_REPLAY_HDR] = 1;
        g_state.Menu_Suicide[0] = 1;
        task_ptr->r_no[1]++;
        break;

    case 8:
        FadeOut(1, FADE_OPAQUE, 8);
        g_state.Menu_Suicide[0] = 0;

        for (ix = 0, s3 = char_ix = '8'; ix < 3; ix++, s2 = char_ix++) {
            effect_61_init(0, ix + EFF_SLOT_REPLAY_MARKER, 0, 0, char_ix, ix, REPLAY_Z_DEPTH_MARKER);
            g_state.Order[ix + EFF_SLOT_REPLAY_MARKER] = 3;
            g_state.Order_Timer[ix + EFF_SLOT_REPLAY_MARKER] = 1;
        }

        effect_66_init(EFF_SLOT_CURSOR_BG, REPLAY_SPRITE_AFTER_BG, 0, 0, -1, -1, REPLAY_Z_DEPTH_AFTER);
        g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
        g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        task_ptr->r_no[1]++;
        FadeInit();

    case 9:
        ToneDown(REPLAY_TONE_NORMAL, 32);

        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[1] = 1;
        }
    }
}

void End_Replay_Menu(struct _TASK* task_ptr) {
    s16 ix;
    s16 ans;

    switch (task_ptr->r_no[1]) {
    case 0:
        if (g_state.Allow_a_battle_f == 0) {
            break;
        }

        task_ptr->r_no[1] += 1;
        g_state.Pause_ID = g_state.Decide_ID;
        g_state.Pause_Down = 1;
        g_state.Game_pause = GAME_PAUSE_ACTIVE;
        effect_A3_init(1, REPLAY_PAUSE_PARAM1, REPLAY_PAUSE_PARAM2, 0, 3, REPLAY_PAUSE_X1, REPLAY_PAUSE_Y1, 1);
        effect_A3_init(1, REPLAY_PAUSE_PARAM1, REPLAY_PAUSE_PARAM2, 1, 3, REPLAY_PAUSE_X2, REPLAY_PAUSE_Y2, 1);
        g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
        g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
        effect_66_init(EFF_SLOT_CURSOR_BG, REPLAY_SPRITE_PAUSE_BG, 2, 7, -1, -1, REPLAY_Z_DEPTH_PAUSE);
        /* fallthrough */

    case 1:
        task_ptr->r_no[1] += 1;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = 0;

        for (ix = 0; ix < 4; ix++) {
            g_state.Menu_Suicide[ix] = 0;
        }

        effect_10_init(0, 0, 0, 4, 0, EFF10_PAUSE_CONTINUE, EFF10_LAYER_EXIT);
        effect_10_init(0, 6, 1, 2, 0, EFF10_PAUSE_BTNCFG, EFF10_LAYER_BTNCFG);
        break;

    case 2:
        MC_Move_Sub(Check_Menu_Lever(g_state.Pause_ID, 0), 0, 1, REPLAY_RESULT_MASK);

        switch (g_state.IO_Result) {
        case SWK_SOUTH:
            switch (g_state.Menu_Cursor_Y[0]) {
            case 0:
                task_ptr->r_no[0] = 0xC;
                task_ptr->r_no[1] = 0;

                for (ix = 0; ix < 4; ix++) {
                    g_state.Menu_Suicide[ix] = 1;
                }

                SE_selected();
                break;

            case 1:
                task_ptr->r_no[1] += 1;
                SE_selected();
                g_state.Menu_Suicide[0] = 1;
                g_state.Menu_Cursor_Y[0] = 1;
                effect_10_init(0, 0, 3, 3, 1, EFF10_PAUSE_CONFIRM, EFF10_LAYER_EXIT);
                effect_10_init(0, 1, 0, 0, 1, EFF10_PAUSE_YES, EFF10_LAYER_BTNCFG);
                effect_10_init(0, 1, 1, 1, 1, EFF10_PAUSE_NO, EFF10_LAYER_BTNCFG);
                break;
            }

            break;
        }

        break;

    case 3:
        ans = ~(g_state.plsw_01[g_state.Pause_ID]) & g_state.plsw_00[g_state.Pause_ID];

        switch (ans) {
        case SWK_UP:
            g_state.Menu_Cursor_Y[0]--;
            if (g_state.Menu_Cursor_Y[0] < 0) {
                g_state.Menu_Cursor_Y[0] = 0;
            } else {
                SE_dir_cursor_move();
            }
            break;

        case SWK_DOWN:
            g_state.Menu_Cursor_Y[0]++;
            if (g_state.Menu_Cursor_Y[0] > 1) {
                g_state.Menu_Cursor_Y[0] = 1;
            } else {
                SE_dir_cursor_move();
            }
            break;

        case SWK_SOUTH: /* Confirm */
        case SWK_EAST:  /* Cancel */
            if (g_state.Menu_Cursor_Y[0] || ans == SWK_EAST) {
                /* User selected NO (cursor 1) or cancelled */
                task_ptr->r_no[1] = 1;
                g_state.Menu_Suicide[3] = 1;
            } else {
                /* User selected YES (cursor 0) - gracefully exit to menu */
                ToneDown(REPLAY_TONE_NORMAL, 32);
                g_state.Replay_Status[0] = 0;
                g_state.Replay_Status[1] = 0;
                Back_to_Mode_Select(task_ptr);
            }
            break;
        }
        break;
    }
}
