/**
 * @file menu.c
 * @brief Game menus â€” mode select, options, training, replays, and VS results.
 *
 * Contains the full menu state machine driven by `Menu_Task()` and its
 * 14-entry jump table: title screen, mode select, game options, button
 * config, screen adjust, sound test, save/load, system direction,
 * extra options, training sub-menus, replay save/load, and VS results.
 *
 * Part of the menu module.
 */

#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/menu/menu_network.h"
#include "sf33rd/Source/Game/menu/menu_save.h"
#include "sf33rd/Source/Game/menu/menu_replay.h"
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
#include "port/ui/replay_picker.h"
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
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
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
#include "sf33rd/Source/Game/ui/count.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

/* RmlUi Phase 3 bypass — per-component toggles + menu replacements */
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

// forward decls
static void Suspend_Menu();
/* Legacy screen functions removed — migrated to MenuScreen registry (ms_*.c):
 *   Mode_Select, Option_Select, Training_Mode, System_Direction,
 *   Load_Replay, toSelectGame, Game_Option, Button_Config,
 *   Sound_Test, Memory_Card, Extra_Option, VS_Result,
 *   Save_Replay, Direction_Menu */
void Setup_VS_Mode(struct _TASK* task_ptr);

void jmpRebootProgram();

void Menu_in_Sub(struct _TASK* task_ptr);
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine);
s32 Menu_Sub_case1(struct _TASK* task_ptr);

enum MenuState {
    MENU_STATE_AFTER_TITLE = 0,
    MENU_STATE_IN_GAME = 1,
    MENU_STATE_WAIT_LOAD_SAVE = 2,
    MENU_STATE_WAIT_REPLAY_CHECK = 3,
    MENU_STATE_DISP_AUTO_SAVE = 4,
    MENU_STATE_SUSPEND_MENU = 5,
    MENU_STATE_WAIT_REPLAY_LOAD = 6,
    MENU_STATE_TRAINING_MENU = 7,
    MENU_STATE_AFTER_REPLAY = 8,
    MENU_STATE_DISP_AUTO_SAVE2 = 9,
    MENU_STATE_WAIT_PAUSE_IN_TR = 10,
    MENU_STATE_RESET_TRAINING = 11,
    MENU_STATE_RESET_REPLAY = 12,
    MENU_STATE_END_REPLAY_MENU = 13
};




// sbss
u8 r_no_plus;
u8 control_player;
u8 control_pl_rno;

// rodata

void After_Title(struct _TASK* task_ptr);
void In_Game(struct _TASK* task_ptr);

/** @brief Top-level menu task â€” pad setup, then dispatch via jump table. */
void Menu_Task(struct _TASK* task_ptr) {
    if (nowSoftReset()) {
        return;
    }

    if (Interface_Type[0] == 0 || Interface_Type[1] == 0) {
        Connect_Status = 0;
    } else {
        Connect_Status = 1;
    }

    Setup_Pad_or_Stick();
    IO_Result = 0;

    if (task_ptr->r_no[0] >= MENU_JMP_COUNT) {
        return;
    }

    switch (task_ptr->r_no[0]) {
    case MENU_STATE_AFTER_TITLE:       After_Title(task_ptr); break;
    case MENU_STATE_IN_GAME:           In_Game(task_ptr); break;
    case MENU_STATE_WAIT_LOAD_SAVE:    Wait_Load_Save(task_ptr); break;
    case MENU_STATE_WAIT_REPLAY_CHECK: Wait_Replay_Check(task_ptr); break;
    case MENU_STATE_DISP_AUTO_SAVE:    Disp_Auto_Save(task_ptr); break;
    case MENU_STATE_SUSPEND_MENU:      Suspend_Menu(); break;
    case MENU_STATE_WAIT_REPLAY_LOAD:  Wait_Replay_Load(); break;
    case MENU_STATE_TRAINING_MENU:     Training_Menu(task_ptr); break;
    case MENU_STATE_AFTER_REPLAY:      After_Replay(task_ptr); break;
    case MENU_STATE_DISP_AUTO_SAVE2:   Disp_Auto_Save2(task_ptr); break;
    case MENU_STATE_WAIT_PAUSE_IN_TR:  Wait_Pause_in_Tr(task_ptr); break;
    case MENU_STATE_RESET_TRAINING:    Reset_Training(task_ptr); break;
    case MENU_STATE_RESET_REPLAY:      Reset_Replay(task_ptr); break;
    case MENU_STATE_END_REPLAY_MENU:   End_Replay_Menu(task_ptr); break;
    }
}

/** @brief Read controller type (pad vs. stick) for both players. */
void Setup_Pad_or_Stick() {
    plsw_00[0] = PLsw[0][0];
    plsw_01[0] = PLsw[0][1];
    plsw_00[1] = PLsw[1][0];
    plsw_01[1] = PLsw[1][1];
}

/** @brief After-title state â€” dispatch to sub-menu by r_no[1]. */
void After_Title(struct _TASK* task_ptr) {
    /* ── MenuScreen registry integration hook (Task 4) ──
     * If the registry is already driving a screen, tick it and return.
     * Otherwise, try to map the legacy r_no[1] index to a MenuScreenId;
     * if a migrated (and enabled) screen is found, hand off to the
     * registry.  Un-migrated indices fall through to the legacy table. */
    if (MenuScreen_IsActive()) {
        MenuScreen_Tick(task_ptr);
        return;
    }

    {
        MenuScreenId mapped = MenuScreen_FromLegacyIndex(task_ptr->r_no[1]);
        if (mapped != MENU_SCREEN_NONE) {
            MenuScreen_Goto(mapped);
            MenuScreen_Tick(task_ptr);
            return;
        }
    }

    /* ── Legacy dispatch (un-migrated screens only) ──
     * All migrated screens are intercepted by MenuScreen_FromLegacyIndex()
     * above and will never reach this table.  Migrated entries use Menu_Init
     * as a safe fallback in case of regression.  Only indices 0 (Menu_Init),
     * 19 (Save_Direction), and 20 (Load_Direction) are still actively
     * dispatched through this table. */
    if (task_ptr->r_no[1] >= AT_JMP_COUNT) {
        return;
    }

    switch (task_ptr->r_no[1]) {
    case MENU_AT_SAVE_DIRECTION:
        Save_Direction(task_ptr);
        break;
    case MENU_AT_LOAD_DIRECTION:
        Load_Direction(task_ptr);
        break;
    default:
        Menu_Init(task_ptr);
        break;
    }
}

/** @brief One-time menu initialisation (fade, BG, saver task). */
void Menu_Init(struct _TASK* task_ptr) {
    s16 ix;
    s16 fade_on;

    if (Pause_Type == 2) {
        task_ptr->r_no[1] = 4;
    } else {
        task_ptr->r_no[1] = 1;
    }

    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    Menu_Cursor_Y[0] = 0;
    Menu_Cursor_Y[1] = 0;

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
        Unsubstantial_BG[ix] = 0;
        Cursor_Y_Pos[0][ix] = 0;
    }

    All_Clear_Suicide();
    pulpul_stop();

    if (task_ptr->r_no[0] == 0) {
        FadeOut(1, 0xFF, 8);
        bg_etc_write_ex(2);
        Setup_Virtual_BG(0, 0x200, 0);
        Setup_BG(1, 0x200, 0);
        Setup_BG(2, 0x200, 0);
        base_y_pos = 0;

        if (task_ptr->r_no[1] != 0x12) {
            fade_on = 0;
        } else {
            fade_on = 1;
        }

        Order[0x4E] = 5;
        Order_Timer[0x4E] = 1;
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, fade_on);
        load_any_texture_patnum(0x7F30, 0xC, 0);
    }

    cpReadyTask(TASK_SAVER, Saver_Task);
}

/* Mode_Select() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Prepare VS mode â€” enable both operators and init grades. */
void Setup_VS_Mode(struct _TASK* task_ptr) {
    task_ptr->r_no[0] = 5;
    cpExitTask(TASK_SAVER);
    plw[0].wu.pl_operator = 1;
    plw[1].wu.pl_operator = 1;
    Operator_Status[0] = 1;
    Operator_Status[1] = 1;
    grade_check_work_1st_init(0, 0);
    grade_check_work_1st_init(0, 1);
    grade_check_work_1st_init(1, 0);
    grade_check_work_1st_init(1, 1);
    Setup_Training_Difficulty();
}

/** @brief Common sub-menu entry â€” fade out, reset cursors, show header. */
void Menu_in_Sub(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] += 1;
    task_ptr->timer = 5;
    Menu_Common_Init();

    /* Hide all Phase 3 game documents when entering a new sub-menu.
     * Each sub-menu's case 0 will show its own doc afterwards. */
    if (use_rmlui)
        rmlui_wrapper_hide_all_game_documents();

    Menu_Cursor_Y[0] = Cursor_Y_Pos[0][1];
    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 0;
    Order[0x64] = 4;
    Order_Timer[0x64] = 1;
}

/* ── Popup draw helpers (called from lobby and font_test) ──────── */





/** @brief Network Lobby screen — options-screen style with toggles and peer list.
 *  Uses effect_61 brightness for cursor indication (no effect_04 cursor bar). */
/* Peer selection indices — exposed as globals for RmlUI bindings */
int g_lobby_peer_idx = 0;
int g_net_peer_idx = 0;



/**
 * @brief Re-activate TASK_MENU at the Network_Lobby input loop (RmlUI mode).
 *
 * After a casual room match ends, Soft_Reset_Sub() kills TASK_MENU. When
 * the user then leaves the room and returns to the network lobby, the menu
 * task must be restored so input processing works (case 14 = RmlUI lobby loop).
 */


/* toSelectGame() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Draw the two game-select button images. */

/* Training_Mode() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Option_Select() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* System_Direction() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Direction_Menu() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Load_Replay() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

const u8 Setup_Index_64[10] = { 1, 2, 3, 3, 4, 5, 6, 7, 8, 8 };

/* Game_Option() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Button_Config() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Sound_Test() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Memory_Card() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Compute final cursor position for multi-column menus. */
s32 Setup_Final_Cursor_Pos(s8 cursor_x, s16 dir) {
    s16 ix;
    s16 check_x[2];
    s16 next_dir;

    if (cursor_x == -1) {
        cursor_x = 0;
    }

    if (vm_w.Connect[cursor_x]) {
        return cursor_x;
    }

    check_x[0] = cursor_x ^ 1;

    if (vm_w.Connect[check_x[0]]) {
        return check_x[0];
    }

    if (dir == 4) {
        next_dir = -2;
    } else {
        next_dir = 2;
    }

    check_x[0] = cursor_x;

    for (ix = 0; ix < 4; ix++) {
        check_x[0] += next_dir;

        if (check_x[0] < 0) {
            if (IO_Result == 0) {
                check_x[0] += 8;
            } else {
                return Menu_Cursor_X[1];
            }
        }

        if (check_x[0] > 7) {
            if (IO_Result == 0) {
                check_x[0] -= 8;
            } else {
                return Menu_Cursor_X[1];
            }
        }

        if (vm_w.Connect[check_x[0]]) {
            return check_x[0];
        }

        check_x[1] = check_x[0] ^ 1;

        if (vm_w.Connect[check_x[1]]) {
            return check_x[1];
        }
    }

    return -1;
}

/** @brief Generic exit sub-routine â€” fade and transition to next routine. */
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine) {
    switch (task_ptr->free[0]) {
    case 0:
        task_ptr->free[0] += 1;
        FadeInit();
        /* fallthrough */

    case 1:
        if (FadeOut(1, 0x19, 8) != 0) {
            task_ptr->r_no[1] = next_routine;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Cursor_Y_Pos[0][cursor_ix] = Menu_Cursor_Y[0];
            Cursor_Y_Pos[1][cursor_ix] = Menu_Cursor_Y[1];
            pulpul_stop();
            return 1;
        }

    default:
        return 0;
    }
}

const u8 Menu_Deley_Time[6] = { 15, 10, 6, 15, 15, 15 };

/** @brief Common menu init â€” reset cursors, clear effects, set timers. */
void Menu_Common_Init() {
    s16 ix;

    for (ix = 0; ix < 2; ix++) {
        Deley_Shot_No[ix] = 0;
        Deley_Shot_Timer[ix] = Menu_Deley_Time[Deley_Shot_No[ix]];
    }

    Menu_Cursor_Move = 0;
    r_no_plus = 0;
}

/** @brief Read and debounce menu lever input for a player. */
u16 Check_Menu_Lever(u8 PL_id, s16 type) {
    u16 sw;
    u16 lever;
    u16 ix;

    sw = ~plsw_01[PL_id] & plsw_00[PL_id];

    if (type) {
        sw = ~PLsw[PL_id][1] & PLsw[PL_id][0];
    }

    lever = plsw_00[PL_id] & SWK_DIRECTIONS;

    if (sw & (SWK_ATTACKS | SWK_START)) {
        return sw;
    }

    sw &= SWK_DIRECTIONS;

    if (sw) {
        return sw;
    }

    if (lever == 0) {
        Deley_Shot_No[PL_id] = 0;
        Deley_Shot_Timer[PL_id] = Menu_Deley_Time[Deley_Shot_No[PL_id]];
        return 0;
    }

    if (--Deley_Shot_Timer[PL_id] == 0) {
        if (++Deley_Shot_No[PL_id] > 2) {
            Deley_Shot_No[PL_id] = 2;
        }

        if (lever & (SWK_UP | SWK_DOWN)) {
            ix = 0;
        } else {
            ix = 3;
        }

        if (Deley_Shot_No[PL_id] + ix >= MENU_DELAY_COUNT) {
            return 0;
        }

        Deley_Shot_Timer[PL_id] = Menu_Deley_Time[Deley_Shot_No[PL_id] + ix];
        return lever;
    }

    return 0;
}

/** @brief Suspend-menu stub (no-op). */
static void Suspend_Menu(struct _TASK* /* unused */) {
    // Do nothing
}

/** @brief In-game state â€” delegate to game task. */
void In_Game(struct _TASK* task_ptr) {
    /* Phase 5b: Screens migrated to the registry dispatch here */
    if (MenuScreen_IsInGameActive()) {
        MenuScreen_InGameTick(task_ptr);
        return;
    }

    /* Intercept r_no[1] values that map to migrated screens */
    MenuScreenId ig_mapped = MenuScreen_FromInGameIndex(task_ptr->r_no[1]);
    if (ig_mapped != MENU_SCREEN_NONE) {
        MenuScreen_Goto(ig_mapped);
        MenuScreen_InGameTick(task_ptr);
        return;
    }

    /* Legacy dispatch (un-migrated In-Game screens only).
     * Indices 1–3 are intercepted by MenuScreen_FromInGameIndex() above.
     * Only index 0 (Menu_Init) and 4 (Pad_Come_Out) remain. */
    switch (task_ptr->r_no[1]) {
    case 0:                   Menu_Init(task_ptr);    break;
    case 4:                   Pad_Come_Out(task_ptr); break;
    default:                  break;
    }
}

/** @brief Write BG extras for menu backgrounds (type-based). */
void bg_etc_write_ex(s16 type) {
    u8 i;

    Family_Init();
    Scrn_Pos_Init();
    Zoomf_Init();
    scr_sc = 1.0f;
    bg_w.bg_opaque = 224;
    bg_w.pos_offset = 192;

    for (i = 0; i < 7; i++) {
        bg_w.bgw[i].pos_x_work = 0;
        bg_w.bgw[i].pos_y_work = 0;
        bg_w.bgw[i].zuubun = 0;
        bg_w.bgw[i].xy[0].cal = 0;
        bg_w.bgw[i].xy[1].cal = 0;
        bg_w.bgw[i].wxy[0].cal = 0;
        bg_w.bgw[i].wxy[1].cal = 0;
        bg_w.bgw[i].hos_xy[0].cal = 0;
        bg_w.bgw[i].hos_xy[1].cal = 0;
        bg_w.bgw[i].rewrite_flag = 0;
        bg_w.bgw[i].fam_no = i;
        bg_w.bgw[i].speed_x = 0;
        bg_w.bgw[i].speed_y = 0;
        bg_w.bgw[i].r_no_1 = bg_w.bgw[i].r_no_2 = 0;
    }

    bg_w.scr_stop = 0;
    bg_w.frame_flag = 0;
    bg_w.old_chase_flag = bg_w.chase_flag = 0;
    bg_w.bg_f_x = 64;
    bg_w.bg_f_y = 64;
    bg_w.bg2_sp_x2 = bg_w.bg2_sp_x = 0;
    bg_w.max_x = 8;
    bg_w.quake_x_index = 0;
    bg_w.quake_y_index = 0;

    for (i = 0; i <= 0; i++) {
        bg_w.bgw[i].hos_xy[0].cal = bg_w.bgw[i].wxy[0].cal = bg_w.bgw[i].xy[0].cal = bg_pos_tbl2[type][i][0];
        bg_w.bgw[i].hos_xy[1].cal = bg_w.bgw[i].wxy[1].cal = bg_w.bgw[i].xy[1].cal = bg_pos_tbl2[type][i][1];
        bg_w.bgw[i].pos_y_work = bg_w.bgw[i].xy[1].disp.pos;
        bg_w.bgw[i].old_pos_x = bg_w.bgw[i].pos_x_work = bg_w.bgw[i].xy[0].disp.pos;
        bg_w.bgw[i].speed_x = msp2[type][i][0];
        bg_w.bgw[i].speed_y = msp2[type][i][1];
        bg_w.bgw[i].rewrite_flag = 0;
        bg_w.bgw[i].zuubun = 0;
        bg_w.bgw[i].frame_deff = 64;
        bg_w.bgw[i].max_x_limit = bg_w.bgw[i].speed_x * bg_w.max_x;
    }

    base_y_pos = 40;
}

/** @brief Wait for save/load I/O completion before proceeding. */


/** @brief Display auto-save notification. */


/** @brief Auto-save step 1 â€” initiate save process. */


/** @brief Auto-save step 2 â€” wait for I/O completion. */


/** @brief Auto-save step 3 â€” display completion message. */


/** @brief Auto-save step 4 â€” fade and return. */


/** @brief Display auto-save notification (variant 2). */


/** @brief Auto-save variant 2 step 4 â€” fade and return. */


/** @brief Wait for replay check result before proceeding. */


/* VS_Result() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Save_Replay() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Save Replay step 2 â€” execute memory-card write. */


/** @brief Set up replay parameters (type, character, master player). */


/** @brief Wait-in-pause state for training mode. */


/** @brief Reset training session (reinitialise state). */


/** @brief Reset replay session (reinitialise state). */


/** @brief Training Menu dispatch â€” jump to selected training sub-screen. */


/** @brief Training initialisation â€” set up menu items and effects. */


/** @brief Normal Training sub-menu â€” recording, playback, and settings. */


/** @brief Dummy Setting sub-menu â€” configure training dummy. */


/** @brief Training Option sub-menu â€” configure training parameters. */


/** @brief Blocking (parrying) Training sub-menu. */


const LetterData training_letter_data[6] = { { 0x82, "NORMAL TRAINING" },   { 0x73, "PARRYING TRAINING" },
                                             { 0x7C, "DUMMY SETTING" },     { 0x87, "TRAINING OPTION" },
                                             { 0x7D, "RECORDING SETTING" }, { 0x8F, "BUTTON CONFIG." } };

/** @brief Blocking Training option screen. */


/** @brief Character Change screen in training mode. */


/** @brief Reset training data to defaults (optionally full or partial). */


/** @brief Wait for replay data to finish loading. */


/** @brief Menu-sub case 1 â€” wait for fade and timer. */
s32 Menu_Sub_case1(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);

    if ((task_ptr->timer -= 1) == 0) {
        task_ptr->r_no[2] += 1;
        FadeInit();
        return 1;
    }

    return 0;
}

/* Extra_Option() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief End Replay Menu â€” post-replay choices (retry / exit). */

