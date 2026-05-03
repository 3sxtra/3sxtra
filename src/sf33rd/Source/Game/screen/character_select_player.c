/**
 * @file character_select_player.c
 * Character/Super Art selection screen
 */

#include "sf33rd/Source/Game/screen/character_select_player.h"
#include "game_state.h"
#include "common.h"
#include "constants.h"
#include "main.h" /* TASK_MENU */
#include "port/menu_screen.h"
#include "port/rendering/renderer.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/com/ai_data_tables.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo_data.h"
#include "sf33rd/Source/Game/effect/effect_38_quake_base_xy.h"
#include "sf33rd/Source/Game/effect/effect_39_quake.h"
#include "sf33rd/Source/Game/effect/effect_42_quake.h"
#include "sf33rd/Source/Game/effect/effect_43_game_state.h"
#include "sf33rd/Source/Game/effect/effect_50_work_user_character_state.h"
#include "sf33rd/Source/Game/effect/effect_52_quake.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h"
#include "sf33rd/Source/Game/effect/effect_69_quake.h"
#include "sf33rd/Source/Game/effect/effect_70_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_75_quake_link.h"
#include "sf33rd/Source/Game/effect/effect_76_quake.h"
#include "sf33rd/Source/Game/effect/effect_79_quake_z_axis.h"
#include "sf33rd/Source/Game/effect/effect_93_quake_jump_table.h"
#include "sf33rd/Source/Game/effect/effect_99_position_data.h"
#include "sf33rd/Source/Game/effect/effect_d8_quake_priority.h"
#include "sf33rd/Source/Game/effect/effect_k6_quake.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/io/rumble.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/rendering/memory_texture_control.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/screen/next_cpu.h"
#include "sf33rd/Source/Game/screen/character_select_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/system_director.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

static void Switch_Work();
static void Sel_PL_Control();
static void Sel_PL_Cont_1st();
static void Check_Use_Gill();
static void Sel_PL_Cont_2nd();
static void Sel_PL_Cont_3rd();
static void Sel_PL_Cont_4th();
static void Setup_Face_ID();
static void Setup_1st_Play_Type();
static void Setup_Face_Sub();
static void Setup_Select_Status();
static u8 Setup_Aborigine();
static void Setup_Cursor_Y();
static void Initialize_BG();
static void Setup_BG_General();
static void Setup_FACE_BG();
static s16 Setup_Face_X();
static s16 Setup_Face_Y();
static void Face_Control();
static void Face_1st();
static void Face_2nd();
static void Face_3rd();
static void Face_4th();
static void Move_Face_BG();
static void OBJ_Control();
static void OBJ_1st();
static void OBJ_2nd();
static void OBJ_3rd();
static void Setup_EFF69();
static void Go_Away_Red_Lines();
static void Player_Select_Control();
static void PL_Sel_1st();
static void PL_Sel_2nd();
static void PL_Sel_3rd();
static void PL_Sel_4th();
static void PL_Sel_5th();
static void Setup_Plates(s8 PL_id, s16 Time);
static void Sel_PL();
static void Sel_PL_1st();
static void Sel_PL_2nd();
static void Sel_PL_3rd();
static u16 Deley_Shot_Sub(s16 PL_id);
static void Sel_PL_4th();
static void Sel_PL_5th();
static void Sel_PL_6th();
static u16 Disposal_Of_Diagonal(u16 sw);
static void Sel_PL_Sub(s16 PL_id, u16 sw);
static void Sel_PL_MoveCursorRight(s16 PL_id);
static void Sel_PL_MoveCursorLeft(s16 PL_id);
static void Sel_PL_MoveCursorUp(s16 PL_id);
static void Sel_PL_MoveCursorDown(s16 PL_id);
static void Auto_Repeat_Sub(s16 PL_id);
static u16 Auto_Repeat_Wait(s16 PL_id);
static void Sel_Arts_Sub(s16 PL_id, u16 sw, u16 /* unused */);
static void Check_Exit();
static void Exit_1st();
static void Exit_2nd();
static void Exit_3rd();
static void Exit_4th();
static void Exit_5th();
static void Exit_6th();
static void Exit_7th();
static void Handicap_1st();
static void Handicap_2nd();
static void Handicap_3rd();
static void Handicap_Control();
static void Handicap_1();
static void Handicap_2();
static void Handicap_3();
static void Handicap_4();
static void Handicap_Vital_Select(s16 PL_id);
static u16 Handicap_Vital_Move_Sub(u16 sw, s16 PL_id);
static void Handicap_Stage_Select(s16 PL_id);
static void Handicap_Stage_Move_Sub(u16 sw);
static void Correct_Control_Time(s16 PL_id);
static s32 Check_Boss(s16 PL_id);
static u8 Setup_Battle_Country();

u8 SEL_PL_X;
s16 Play_Type_1st;
u16 Color7[2];
u8 Decide_Stage;
u8 hc3alpha;

/** @brief Per-frame body of the character select — called by ms_char_select.c on_tick. */
void Sel_PL_Control_Frame() {
    SEL_PL_X = 0;

    if (g_state.Break_Into) {
        return;
    }

    g_state.Scene_Cut = Cut_Cut_Cut();
    Sel_PL_Control();
    Switch_Work();
    g_state.ID = 0;
    Sel_PL();
    g_state.ID = 1;
    Sel_PL();
    g_state.Time_Over = false;

    if (Check_Exit_Check() == 0 && Debug_w[DEBUG_TIME_STOP] == -1) {
        SEL_PL_X = 0;
    }
}

#include "port/menu_task.h"

/** @brief Main character-select dispatcher — thin wrapper around MenuScreen registry. */
s16 Select_Player() {
    struct _TASK* tp = MenuTask_GetTaskPtr();

    /* If another MenuScreen is driving (e.g. Attract Mode Demo simulating Character Select),
     * bypass the registry wrapper and run the legacy frame directly to prevent infinite recursion. */
    if (MenuScreen_IsActive() && MenuScreen_GetCurrent() != MENU_SCREEN_CHAR_SELECT) {
        Sel_PL_Control_Frame();
        return SEL_PL_X;
    }

    if (!MenuScreen_IsActive()) {
        SEL_PL_X = 0;
        MenuScreen_Goto(MENU_SCREEN_CHAR_SELECT);
    }

    MenuScreen_Tick(tp);

    /* Exit_7th() sets SEL_PL_X = 1 when the char select is complete.
     * We detect that here and exit the registry (no MenuScreen_RequestFadeOut
     * because the char select manages its own fade transition internally). */
    if (SEL_PL_X == 1) {
        MenuScreen_ExitToLegacy(tp);
    }

    if (Check_Exit_Check() == 0 && Debug_w[DEBUG_TIME_STOP] == -1) {
        SEL_PL_X = 0;
    }

    return SEL_PL_X;
}

/** @brief Mirror input in training mode so the champion’s inputs control both sides. */
static void Switch_Work() {
    if (g_state.Mode_Type != MODE_NORMAL_TRAINING && g_state.Mode_Type != MODE_PARRY_TRAINING &&
        g_state.Mode_Type != MODE_TRIALS) {
        return;
    }

    switch (g_state.select_phase[3]) {
    case 0:
        if (g_state.Champion) {
            p1sw_0 = 0;
        } else {
            p2sw_0 = 0;
        }

        break;

    case 1:
        g_state.select_phase[3]++;
        Default_Training_Data(0);
        g_state.Record_Data_Tr = 0;
        Training_Disp_Work_Clear();
        g_state.Menu_Cursor_X[0] = 0;
        g_state.Training_Cursor = 0;

        if (g_state.Champion) {
            p1sw_0 = p2sw_0;
            p1sw_1 = p2sw_0;
        } else {
            p2sw_0 = p1sw_0;
            p2sw_1 = p1sw_0;
        }

        break;

    case 2:
        if (g_state.Champion) {
            p1sw_0 = p2sw_0;
        } else {
            p2sw_0 = p1sw_0;
        }

        break;
    }
}

/** @brief Top-level select-screen controller — run status, face, OBJ, player-select, and exit phases. */
static void Sel_PL_Control() {
    Setup_Select_Status();
    switch (g_state.select_phase[0]) {
    case SEL_PL_CONT_1ST:
        Sel_PL_Cont_1st();
        break;
    case SEL_PL_CONT_2ND:
        Sel_PL_Cont_2nd();
        break;
    case SEL_PL_CONT_3RD:
        Sel_PL_Cont_3rd();
        break;
    case SEL_PL_CONT_4TH:
        Sel_PL_Cont_4th();
        break;
    }
    Face_Control();
    OBJ_Control();
    g_state.ID2 = 0;
    Player_Select_Control();
    g_state.ID2 = 1;
    Player_Select_Control();
    Check_Exit();
}

/** @brief Select control phase 1 — screen switch, clear state, build texcache, init BG/faces/timer. */
static void Sel_PL_Cont_1st() {
    s16 xx;

    Switch_Screen(1);
    g_state.select_phase[0]++;
    All_Clear_Suicide();
    SsBgmHalfVolume(0);
    g_state.Face_No[0] = 0;
    g_state.Face_No[1] = 0;
    g_state.SO_No[0] = 0;
    g_state.SO_No[1] = 0;
    g_state.Exit_No = 0;
    g_state.Fade_Flag = 0;
    g_state.judge_flag = 0;
    g_state.Game_pause = 0;

    for (xx = 0; xx < 4; xx++) {
        g_state.SP_No[0][xx] = 0;
        g_state.SP_No[1][xx] = 0;
    }

    Purge_mmtm_area(2);
    Allocate_Texture_Cache_List(2);
    bg_etc_write(2);
    Setup_Aborigine();
    Initialize_BG();
    Setup_Cursor_Y();

    if (g_state.Present_Mode == 4 || g_state.Present_Mode == 5) {
        g_state.Select_Timer = 0x20;
    } else {
        g_state.Select_Timer = 0x30;
    }

    g_state.Unit_Of_Timer = UNIT_OF_TIMER_MAX;
    Setup_Face_ID();
    Setup_1st_Play_Type();
    Setup_Face_Sub();
    g_state.Time_Stop = 1;
    SelectTimer_Init();
    g_state.Face_MV_Request = 0;
    g_state.Face_Status = 0;
    g_state.Face_Move = 0;
    g_state.Break_Into_CPU = 0;
    g_state.Explosion = 0;
    g_state.Time_Over = false;
    g_state.Move_Super_Arts[0] = 0;
    g_state.Move_Super_Arts[1] = 0;
    g_state.Flash_Complete[0] = 0;
    g_state.Flash_Complete[1] = 0;
    g_state.Cursor_Move[0] = 0;
    g_state.Cursor_Move[1] = 0;
    Check_Use_Gill();
    pulpul_stop();
    pp_operator_check_flag(1);
    effect_58_init(6, 20, 157);

    if (use_rmlui && rmlui_screen_select)
        rmlui_char_select_show();
}

/** @brief Unlock Gill if the player has used every other character at least once. */
static void Check_Use_Gill() {
    s16 ix;

    if (g_state.Mode_Type == MODE_NETWORK) {
        return;
    }

    permission_player[1].can_activate[0] = 0;
    permission_player[4].can_activate[0] = 0;
    permission_player[5].can_activate[0] = 0;

    if (CurrentSave()->Unlock_All) {
        permission_player[1].can_activate[0] = 1;
        permission_player[4].can_activate[0] = 1;
        permission_player[5].can_activate[0] = 1;
        return;
    }

    for (ix = 1; ix < 20; ix++) {
        if (save_w[SAVEW_ARCADE].PL_Color[0][ix] == 0) {
            return;
        }
    }

    permission_player[1].can_activate[0] = 1;
    permission_player[4].can_activate[0] = 1;
    permission_player[5].can_activate[0] = 1;
}

/** @brief Select control phase 2 — init screen-switch revival, request entry state, clear flash. */
static void Sel_PL_Cont_2nd() {
    Switch_Screen(1);
    Switch_Screen_Init(1);
    g_state.select_phase[0]++;
    g_state.Request_E_No = 1;
    Clear_Flash_No();
}

/** @brief Select control phase 3 — wait for screen revival, then enable break-in and clear demo flag. */
static void Sel_PL_Cont_3rd() {
    if (!Switch_Screen_Revival(0)) {
        return;
    }

    g_state.select_phase[0]++;
    g_state.Forbid_Break = 0;

    if (g_state.fsm[1] != 1) {
        // This is a comparison to zero in the decomp. Might be a programmer error
        g_state.Demo_Flag = 0;
    }
}

/** @brief Select control phase 4 — intentionally empty (placeholder). */
static void Sel_PL_Cont_4th() {
    // Do nothing
}

/** @brief Populate g_state.ID_of_Face grid from the Face_Cursor_Data layout table. */
static void Setup_Face_ID() {
    s16 x;
    s16 y;

    for (y = 0; y < 3; y++) {
        for (x = 0; x < 8; x++) {
            g_state.ID_of_Face[y][x] = Face_Cursor_Data[y][x];
        }
    }
}

/** @brief Record the initial play-type so we know whether a second player joined later. */
static void Setup_1st_Play_Type() {
    if (g_state.Play_Type == 1) {
        Play_Type_1st = 99;
    } else {
        Play_Type_1st = g_state.Aborigine;
    }
}

/** @brief Spawn all 19 character-face portrait effect objects on the grid. */
static void Setup_Face_Sub() {
    s16 x;

    g_state.Complete_Face = 19;

    for (x = 1; x < 20; x++) {
        effect_70_init(x);
    }
}

/** @brief Compute g_state.Select_Status from operator flags and arts-complete state. */
static void Setup_Select_Status() {
    if (g_state.plw[0].wu.pl_operator) {
        g_state.Select_Status[0] = 1;
    } else {
        g_state.Select_Status[0] = 0;
    }

    if (g_state.plw[1].wu.pl_operator) {
        g_state.Select_Status[0] |= 2;
    }

    if (g_state.Sel_Arts_Complete[0] != -1 && g_state.plw[0].wu.pl_operator != 0) {
        g_state.Select_Status[1] = 1;
    } else {
        g_state.Select_Status[1] = 0;
    }

    if (g_state.Sel_Arts_Complete[1] != -1 && g_state.plw[1].wu.pl_operator != 0) {
        g_state.Select_Status[1] |= 2;
    }
}

/** @brief Determine g_state.Aborigine (which player selects first) from operator state. */
static u8 Setup_Aborigine() {
    if (g_state.Select_Status[0] == 3) {
        return g_state.Aborigine = 153;
    }

    if (g_state.Select_Status[0] == 1) {
        return g_state.Aborigine = 0;
    }

    return g_state.Aborigine = 1;
}

/** @brief Build the per-player g_state.Cursor_Y_Pos arrays from the Cursor_Y_Data table. */
static void Setup_Cursor_Y() {
    s16 i;
    s16 j;

    s16 a;
    s16 b;
    s16 c;
    s16 d;

    for (i = 2, a = j = 0; i >= 0; i--, b = j++) {
        g_state.Cursor_Y_Pos[0][i] = Cursor_Y_Data[j];
    }

    for (i = 2, c = j = 3; i >= 0; i--, d = j++) {
        g_state.Cursor_Y_Pos[1][i] = Cursor_Y_Data[j];
    }
}

/** @brief Set up all background layers for the character-select screen. */
static void Initialize_BG() {
    Setup_BG_General();
    Setup_BG(2, 512, 0);
    Setup_BG(3, 704, 0);
    Setup_FACE_BG();
}

/** @brief General BG setup — init zoom, stock old position, set family. */
static void Setup_BG_General() {
    Zoomf_Init();
    g_state.bg_w.bgw[0].old_pos_x = g_state.bg_w.bgw[0].xy[0].disp.pos;
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Set up the face-grid BG layer position and family data. */
static void Setup_FACE_BG() {
    s16 face_x;
    s16 face_y;

    g_state.Unsubstantial_BG[1] = 1;
    face_x = Setup_Face_X();
    face_y = Setup_Face_Y();
    g_state.bg_w.bgw[1].xy[0].disp.pos = face_x;
    g_state.bg_w.bgw[1].xy[1].disp.pos = face_y;
    g_state.bg_w.bgw[1].wxy[0].disp.pos = face_x;
    g_state.bg_w.bgw[1].wxy[1].disp.pos = face_y;
    g_state.bg_w.bgw[1].xy[0].disp.low = 0;
    g_state.bg_w.bgw[1].xy[1].disp.low = 0;
    g_state.bg_w.bgw[1].position_x = face_x;
    g_state.bg_w.bgw[1].position_y = face_y;
    g_state.bg_w.bgw[1].hos_xy[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos = g_state.bg_w.bgw[1].xy[0].disp.pos;
    Bg_Family_Set_Ex(1);
}

/** @brief Return the X offset for the face-grid BG based on play type and aborigine. */
static s16 Setup_Face_X() {
    if (g_state.Play_Type == 1) {
        return 604;
    }

    if (g_state.Aborigine == 0) {
        return 512;
    }

    return 696;
}

/** @brief Return the Y offset for the face-grid BG based on play type and aborigine. */
static s16 Setup_Face_Y() {
    if (g_state.Play_Type == 1) {
        return 0;
    }

    if (g_state.Aborigine == 0) {
        return -24;
    }

    return 0;
}

/** @brief Face-panel state machine — dispatch face phase and move the BG. */
static void Face_Control() {
    switch (g_state.Face_No[0]) {
    case FACE_1ST:
        Face_1st();
        break;
    case FACE_2ND:
        Face_2nd();
        break;
    case FACE_3RD:
        Face_3rd();
        break;
    case FACE_4TH:
        Face_4th();
        break;
    }
    Move_Face_BG();
}

/** @brief Face phase 1 — choose initial face layout (1P or 2P). */
static void Face_1st() {
    if (g_state.Select_Status[0] == 3) {
        g_state.Face_No[0] = 3;
    } else {
        g_state.Face_No[0] = 1;
    }
}

/** @brief Face phase 2 — slide face BG when second player joins or first completes. */
static void Face_2nd() {
    if (g_state.Select_Status[0] == 3 && g_state.Face_MV_Request == 0) {
        g_state.Face_No[0] = 3;
        g_state.Face_MV_Time = 1;

        if (g_state.Aborigine == 1) {
            g_state.Face_MV_Request = 2;
            g_state.bg_mvxy.a[0].sp = -0x90000;
            g_state.bg_mvxy.d[0].sp = -0x8000;
            return;
        }

        g_state.Face_MV_Request = 1;
        g_state.bg_mvxy.a[0].sp = 0x90000;
        g_state.bg_mvxy.d[0].sp = 0x8000;
        return;
    }

    if (g_state.Sel_PL_Complete[g_state.Aborigine]) {
        g_state.Face_MV_Time = 5;
        g_state.Face_No[0]++;

        if (g_state.Aborigine == 0) {
            g_state.Face_MV_Request = 4;
            g_state.bg_mvxy.a[0].sp = -0xC0000;
            g_state.bg_mvxy.d[0].sp = -0x8000;
            return;
        }

        g_state.Face_MV_Request = 3;
        g_state.bg_mvxy.a[0].sp = 0xC0000;
        g_state.bg_mvxy.d[0].sp = 0x8000;
    }
}

/** @brief Face phase 3 — slide face BG back when both players are selecting. */
static void Face_3rd() {
    if (g_state.Select_Status[0] != 3) {
        return;
    }

    if (g_state.Face_MV_Request != 0) {
        return;
    }

    g_state.Face_No[0]++;
    g_state.Face_MV_Time = 1;

    if (g_state.Aborigine == 1) {
        g_state.Face_MV_Request = 2;
        g_state.bg_mvxy.a[0].sp = -0xC0000;
        g_state.bg_mvxy.d[0].sp = -0x8000;
        return;
    }

    g_state.Face_MV_Request = 1;
    g_state.bg_mvxy.a[0].sp = 0xC0000;
    g_state.bg_mvxy.d[0].sp = 0x8000;
}

/** @brief Face phase 4 — no-op (face movement complete). */
static void Face_4th() {}

/** @brief Apply pending face-move requests as effect_93 BG scrolls. */
static void Move_Face_BG() {
    switch (g_state.Face_No[1]) {
    case 0:
        if (g_state.Face_MV_Request) {
            g_state.Face_No[1]++;
            g_state.Face_Move = g_state.Face_MV_Request;
            effect_93_init(g_state.Face_Move - 1, g_state.Face_MV_Time);
        }

        break;

    default:
        if (!(g_state.Face_MV_Request = g_state.Face_Move)) {
            g_state.Face_No[1] = 0;
        }

        break;
    }
}

/** @brief OBJ state machine — dispatch portrait/plate object initialisation phases. */
static void OBJ_Control() {
    switch (g_state.SO_No[0]) {
    case OBJ_1ST:
        OBJ_1st();
        break;
    case OBJ_2ND:
        OBJ_2nd();
        break;
    case OBJ_3RD:
        OBJ_3rd();
        break;
    }
}

/** @brief OBJ phase 1 — spawn all character-select UI objects (portraits, name plates, effects). */
static void OBJ_1st() {
    Setup_EFF69();

    if (g_state.Select_Status[0] != 3) {
        g_state.SO_No[0] = 1;
        effect_38_init(g_state.Aborigine, g_state.Aborigine + 11, 127, 0, 2);
        g_state.Order[g_state.Aborigine + 11] = 1;
        g_state.Order_Timer[g_state.Aborigine + 11] = 35;
        effect_52_init(g_state.Aborigine, 37);
        g_state.Order[37] = 1;
        g_state.Order_Timer[37] = 30;
        g_state.Order_Dir[37] = 0;
        effect_K6_init(g_state.Aborigine, g_state.Aborigine + 31, 31, 2);
        g_state.Order[g_state.Aborigine + 31] = 1;
        g_state.Order_Timer[g_state.Aborigine + 31] = 35;
        g_state.Order_Dir[g_state.Aborigine + 31] = 0;
        effect_K6_init(g_state.Aborigine, g_state.Aborigine + 25, 25, 2);
        g_state.Order[g_state.Aborigine + 25] = 1;
        g_state.Order_Timer[g_state.Aborigine + 25] = 35;
        g_state.Order_Dir[g_state.Aborigine + 25] = 0;
        g_state.Order[0] = 1;
        g_state.Order_Timer[0] = 40;
        g_state.Order_Dir[0] = 4;
        g_state.Order[1] = 1;
        g_state.Order_Timer[1] = 45;
        g_state.Order_Dir[1] = 4;
        g_state.Order[3] = 1;
        g_state.Order_Timer[3] = 45;
        g_state.Order_Dir[3] = 4;
        effect_39_init(g_state.Aborigine, g_state.Aborigine + 13, 127, 2, 1);
        g_state.Order[g_state.Aborigine + 13] = 1;
        g_state.Order_Timer[g_state.Aborigine + 13] = 35;
        g_state.Order_Dir[g_state.Aborigine + 13] = 0;
        effect_42_init(5);
        g_state.Order[5] = 1;
        g_state.Order_Timer[5] = 45;
        g_state.Order_Dir[5] = 4;
        effect_42_init(6);
        g_state.Order[6] = 1;
        g_state.Order_Timer[6] = 45;
        g_state.Order_Dir[6] = 4;
        return;
    }

    g_state.SO_No[0] = 2;
    effect_75_init(42, 3, 2);
    g_state.Order[42] = 3;
    g_state.Order_Timer[42] = 1;
    g_state.Order_Dir[42] = 3;
    effect_38_init(0, 11, 127, 1, 2);
    g_state.Order[11] = 1;
    g_state.Order_Timer[11] = 86;
    effect_38_init(1, 12, 127, 1, 2);
    g_state.Order[12] = 1;
    g_state.Order_Timer[12] = 86;
    effect_K6_init(0, 33, 31, 2);
    g_state.Order[33] = 1;
    g_state.Order_Timer[33] = 86;
    g_state.Order_Dir[33] = 0;
    effect_52_init(0, 38);
    g_state.Order[38] = 3;
    g_state.Order_Timer[38] = 30;
    effect_K6_init(0, 27, 25, 2);
    g_state.Order[27] = 3;
    g_state.Order_Timer[27] = 86;
    effect_K6_init(1, 28, 25, 2);
    g_state.Order[28] = 3;
    g_state.Order_Timer[28] = 86;
    effect_K6_init(1, 34, 31, 2);
    g_state.Order[34] = 1;
    g_state.Order_Timer[34] = 86;
    g_state.Order_Dir[34] = 0;
    effect_52_init(1, 39);
    g_state.Order[39] = 3;
    g_state.Order_Timer[39] = 30;
    effect_39_init(0, 15, 127, 2, 0);
    g_state.Order[15] = 1;
    g_state.Order_Timer[15] = 86;
    g_state.Order_Dir[15] = 0;
    effect_39_init(1, 16, 127, 2, 0);
    g_state.Order[16] = 1;
    g_state.Order_Timer[16] = 86;
    g_state.Order_Dir[16] = 0;
    g_state.Order[4] = 3;
    g_state.Order_Timer[4] = 86;
    g_state.Order_Dir[4] = 255;
    effect_42_init(7);
    g_state.Order[7] = 0;
    g_state.Order_Timer[7] = 86;
    effect_42_init(8);
    g_state.Order[8] = 0;
    g_state.Order_Timer[8] = 86;
}

/** @brief OBJ phase 2 — reconfigure objects when a second player breaks in mid-select. */
static void OBJ_2nd() {
    if (g_state.Select_Status[0] != 3) {
        return;
    }

    g_state.SO_No[0]++;
    effect_75_init(42, 3, 2);
    g_state.Order[42] = 3;
    g_state.Order_Timer[42] = 1;
    g_state.Order_Dir[42] = 3;
    g_state.Order[g_state.Aborigine + 11] = 4;
    g_state.Order_Timer[g_state.Aborigine + 11] = 1;
    g_state.Select_Start[g_state.Aborigine] = 2;
    effect_38_init(g_state.New_Challenger, g_state.New_Challenger + 11, 127, 1, 2);
    g_state.Order[g_state.New_Challenger + 11] = 1;
    g_state.Order_Timer[g_state.New_Challenger + 11] = 1;
    Go_Away_Red_Lines();
    g_state.Order[g_state.Aborigine + 31] = 5;
    g_state.Order_Timer[g_state.Aborigine + 31] = 1;
    g_state.Order[g_state.Aborigine + 19] = 5;
    g_state.Order_Timer[g_state.Aborigine + 19] = 1;
    g_state.Order[g_state.Aborigine + 25] = 5;
    g_state.Order_Timer[g_state.Aborigine + 25] = 1;
    g_state.Order[g_state.Aborigine + 13] = 5;
    g_state.Order_Timer[g_state.Aborigine + 13] = 1;
    g_state.Order[37] = 4;
    g_state.Order_Timer[37] = 1;
    effect_K6_init(0, 33, 31, 2);
    g_state.Order[33] = 1;
    g_state.Order_Timer[33] = 1;
    g_state.Order_Dir[33] = 0;
    effect_K6_init(0, 27, 25, 2);
    g_state.Order[27] = 1;
    g_state.Order_Timer[27] = 1;
    g_state.Order_Dir[27] = 0;
    effect_39_init(0, 15, 127, 2, 0);
    g_state.Order[15] = 1;
    g_state.Order_Timer[15] = 1;
    g_state.Order_Dir[15] = 0;
    effect_K6_init(1, 34, 31, 2);
    g_state.Order[34] = 1;
    g_state.Order_Timer[34] = 1;
    g_state.Order_Dir[34] = 0;
    effect_K6_init(1, 28, 25, 2);
    g_state.Order[28] = 1;
    g_state.Order_Timer[28] = 1;
    g_state.Order_Dir[28] = 0;
    effect_39_init(1, 16, 127, 2, 0);
    g_state.Order[16] = 1;
    g_state.Order_Timer[16] = 1;
    g_state.Order_Dir[16] = 0;
    g_state.Order[4] = 3;
    g_state.Order_Timer[4] = 1;
    g_state.Order_Dir[4] = 255;
    effect_42_init(7);
    g_state.Order[7] = 0;
    g_state.Order_Timer[7] = 1;
    effect_42_init(8);
    g_state.Order[8] = 0;
    g_state.Order_Timer[8] = 1;
}

/** @brief OBJ phase 3 — no-op (object setup complete). */
static void OBJ_3rd() {}

/** @brief Spawn the 5 red-line / decoration effect-69 objects. */
static void Setup_EFF69() {
    s16 xx;

    for (xx = 0; xx < 5; xx++) {
        g_state.Order[xx] = 0;
        effect_69_init(xx);
    }
}

/** @brief Dismiss all red-line decoration objects with a fade-out animation. */
static void Go_Away_Red_Lines() {
    g_state.Order[0] = 2;
    g_state.Order_Timer[0] = 1;
    g_state.Order_Dir[0] = 8;
    g_state.Order[2] = 2;
    g_state.Order_Timer[2] = 1;
    g_state.Order_Dir[2] = 8;
    g_state.Order[1] = 2;
    g_state.Order_Timer[1] = 1;
    g_state.Order_Dir[1] = 8;
    g_state.Order[3] = 2;
    g_state.Order_Timer[3] = 1;
    g_state.Order_Dir[3] = 8;
    g_state.Order[5] = 2;
    g_state.Order[6] = 2;
    g_state.Order_Timer[5] = 1;
    g_state.Order_Timer[6] = 1;
    g_state.Order_Dir[5] = 8;
    g_state.Order_Dir[6] = 8;
}

/** @brief Per-player select control — dispatch PL_Sel phases if the player is an operator. */
static void Player_Select_Control() {
    if (g_state.plw[g_state.ID2].wu.pl_operator != 0) {
        switch (g_state.SP_No[g_state.ID2][1]) {
        case PL_SEL_1ST:
            PL_Sel_1st();
            break;
        case PL_SEL_2ND:
            PL_Sel_2nd();
            break;
        case PL_SEL_3RD:
            PL_Sel_3rd();
            break;
        case PL_SEL_4TH:
            PL_Sel_4th();
            break;
        case PL_SEL_5TH:
            PL_Sel_5th();
            break;
        }
    }
}

/** @brief PL_Sel phase 1 — init cursor state, spawn D8 effects, play voice; skip if already complete. */
static void PL_Sel_1st() {
    s16 ret;
    s16 ret2;

    if (g_state.Sel_PL_Complete[g_state.ID2] == -0x8000) {
        g_state.SP_No[g_state.ID2][1] = 2;
        Push_LDREQ_Queue_Player(g_state.ID2, g_state.My_char[g_state.ID2]);
        ret = check_use_all_SA();
        ret2 = check_without_SA();
        ret |= ret2;

        if (ret != 0) {
            return;
        }

        if (g_state.My_char[g_state.ID2] == 0) {
            return;
        }

        g_state.Sel_Arts_Complete[g_state.ID2] = 0;
        Setup_Plates(g_state.ID2, 0x55);
        effect_50_init(g_state.ID2, 1, 0);
        effect_50_init(g_state.ID2, 1, 1);
        effect_50_init(g_state.ID2, 2, 0);
        effect_50_init(g_state.ID2, 2, 1);

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (!Debug_w[DEBUG_MY_CHAR_PL2]) {
            return;
        }

        g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        return;
    }

    g_state.SP_No[g_state.ID2][1]++;
}

/** @brief PL_Sel phase 2 — handle character confirmation via loading and SA-availability checks. */
static void PL_Sel_2nd() {
    s16 ret;
    s16 ret2;

    switch (g_state.SP_No[g_state.ID2][3]) {
    case 0:
        if (!g_state.Sel_PL_Complete[g_state.ID2]) {
            break;
        }

        ret = check_use_all_SA();
        ret2 = check_without_SA();
        ret |= ret2;

        if (ret != 0 || g_state.My_char[g_state.ID2] == 0) {
            g_state.SP_No[g_state.ID2][3]++;
            g_state.Cursor_Timer[g_state.ID2] = 40;
            Go_Away_Red_Lines();

            if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING ||
                g_state.Mode_Type == MODE_TRIALS) {
                g_state.select_phase[3] = 1;
                break;
            }

            break;
        }

        g_state.SP_No[g_state.ID2][1]++;
        Setup_Plates(g_state.ID2, 1);
        effect_50_init(g_state.ID2, 1, 0);
        effect_50_init(g_state.ID2, 1, 1);
        effect_50_init(g_state.ID2, 2, 0);
        effect_50_init(g_state.ID2, 2, 1);
        break;

    case 1:
        if ((g_state.Cursor_Timer[g_state.ID2] -= 1) != 0) {
            break;
        }

        g_state.Sel_Arts_Complete[g_state.ID2] = -1;
        g_state.SP_No[g_state.ID2][1]++;
        g_state.SP_No[g_state.ID2][3] = 0;
        Setup_ID();

        if (g_state.Used_char[g_state.ID2] != g_state.My_char[g_state.ID2]) {
            g_state.Last_Player_id = g_state.ID2;
        }

        g_state.Used_char[g_state.ID2] = g_state.My_char[g_state.ID2];
        break;
    }
}

/** @brief PL_Sel phase 3 — wait for arts completion before advancing. */
static void PL_Sel_3rd() {
    if (g_state.Sel_Arts_Complete[g_state.ID2] < 0) {
        g_state.SP_No[g_state.ID2][1]++;
    }
}

/** @brief PL_Sel phase 4 — no-op placeholder. */
static void PL_Sel_4th() {}

/** @brief PL_Sel phase 5 — no-op placeholder. */
static void PL_Sel_5th() {}

/** @brief Spawn the 3 super-art selection plates for the given player. */
static void Setup_Plates(s8 PL_id, s16 Time) {
    g_state.Move_Super_Arts[PL_id] = 3;
    g_state.Select_Arts[PL_id] = 3;
    effect_79_init(PL_id, 0, Arts_Y_Data[g_state.Super_Arts[PL_id]][0], Time, 2);
    effect_79_init(PL_id, 1, Arts_Y_Data[g_state.Super_Arts[PL_id]][1], Time, 2);
    effect_79_init(PL_id, 2, Arts_Y_Data[g_state.Super_Arts[PL_id]][2], Time, 2);
}

/** @brief Per-player character-select state machine dispatcher. */
static void Sel_PL() {
    if (g_state.plw[g_state.ID].wu.pl_operator != 0) {
        switch (g_state.SP_No[g_state.ID][0]) {
        case SEL_PL_1ST:
            Sel_PL_1st();
            break;
        case SEL_PL_2ND:
            Sel_PL_2nd();
            break;
        case SEL_PL_3RD:
            Sel_PL_3rd();
            break;
        case SEL_PL_4TH:
            Sel_PL_4th();
            break;
        case SEL_PL_5TH:
            Sel_PL_5th();
            break;
        case SEL_PL_6TH:
            Sel_PL_6th();
            break;
        }
    }
}

/** @brief Sel_PL phase 1 — init cursor/auto-repeat state, spawn D8/voice, set g_state.Select_Start. */
static void Sel_PL_1st() {
    u16 Rnd;

    if (g_state.Exit_No) {
        return;
    }

    g_state.SP_No[g_state.ID][0]++;
    g_state.Stop_Cursor[g_state.ID] = 1;
    g_state.Auto_No[g_state.ID] = 0;
    g_state.Auto_Index[g_state.ID] = 0;
    g_state.Auto_Cursor[g_state.ID] = 0;
    g_state.Moving_Plate[g_state.ID] = 0;
    g_state.Moving_Plate_Counter[g_state.ID] = 0;
    g_state.Select_Start[g_state.ID] = 2;
    g_state.Select_Arts[g_state.ID] = -1;

    if (g_state.ID == 1) {
        effect_D8_init(1, 1);
        effect_D8_init(1, 3);
        Rnd = random_16() & 3;
        Free_Ptr[1] = Voice_Random_Data[1][Rnd];
    } else {
        effect_D8_init(0, 0);
        effect_D8_init(0, 2);
        Rnd = random_16() & 3;
        Free_Ptr[0] = Voice_Random_Data[1][Rnd];
    }

    if (g_state.Sel_PL_Complete[g_state.ID]) {
        g_state.SP_No[g_state.ID][0] = 3;
        g_state.Select_Start[g_state.ID] = 3;
        g_state.Select_Arts[g_state.ID] = 3;
        g_state.Stop_Cursor[g_state.ID] = 1;
        g_state.parry_ctr_vs[0][g_state.ID] = 0;
        g_state.parry_ctr_vs[1][g_state.ID] = 0;
        return;
    }

    g_state.Arts_Y[g_state.ID] = g_state.Super_Arts[g_state.ID] = g_state.Last_Super_Arts[g_state.ID];
}

/** @brief Sel_PL phase 2 — wait for g_state.Select_Start countdown, then enable cursor input. */
static void Sel_PL_2nd() {
    if (g_state.Select_Start[g_state.ID] > 0) {
        return;
    }

    g_state.SP_No[g_state.ID][0]++;
    g_state.Stop_Cursor[g_state.ID] = 0;
    g_state.Deley_Shot_No[g_state.ID] = 0;
    g_state.Cursor_Timer[g_state.ID] = 1;

    if (g_state.Demo_Flag == 0) {
        g_state.Demo_Timer[g_state.ID] = 0;
        Demo_Ptr[g_state.ID] = (u16*)Sel_PL_Data_Address[g_state.Select_Demo_Index];
    }
}

/** @brief Sel_PL phase 3 — handle cursor+button input per-player (or demo), commit character on press. */
static void Sel_PL_3rd() {
    if (g_state.Stop_Cursor[g_state.ID] != 0 || g_state.Face_Move != 0) {
        return;
    }

    if (g_state.Demo_Flag == 0) {
        if (g_state.ID) {
            Sel_PL_Sub(1, Check_Demo_Data(1));
        } else {
            Sel_PL_Sub(0, Check_Demo_Data(0));
        }
    } else if (g_state.ID) {
        Sel_PL_Sub(1, Deley_Shot_Sub(1));
    } else {
        Sel_PL_Sub(0, Deley_Shot_Sub(0));
    }

    if (g_state.Sel_PL_Complete[g_state.ID] >= 0) {
        return;
    }

    if (Debug_w[DEBUG_MY_CHAR_PL1]) {
        g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
    }

    if (Debug_w[DEBUG_MY_CHAR_PL2]) {
        g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
    }

    Push_LDREQ_Queue_Player(g_state.ID, g_state.My_char[g_state.ID]);
    g_state.SP_No[g_state.ID][0]++;
    g_state.Stop_Cursor[g_state.ID] = 1;
    g_state.Auto_No[g_state.ID] = 0;
    g_state.parry_ctr_vs[0][g_state.ID] = 0;
    g_state.parry_ctr_vs[1][g_state.ID] = 0;

    if (g_state.Continue_Coin[g_state.ID] == 0) {
        Clear_Break_Com(g_state.ID);
        grade_check_work_1st_init(g_state.ID, 0);
        grade_check_work_1st_init(g_state.ID, 1);
        Initialize_EM_Candidate(g_state.ID);
        g_state.Best_Grade[g_state.ID] = -1;
        g_state.Result_Timer[g_state.ID] = 180;
        g_state.Request_Disp_Rank[g_state.ID][0] = -1;
        g_state.Request_Disp_Rank[g_state.ID][1] = -1;
        g_state.Request_Disp_Rank[g_state.ID][2] = -1;
        g_state.Request_Disp_Rank[g_state.ID][3] = -1;
        return;
    }

    Check_Same_CPU(g_state.ID);
}

/** @brief Delayed-shot sub — accumulate attack buttons over a short window for multi-button detection. */
static u16 Deley_Shot_Sub(s16 PL_id) {
    u16 sw;
    u16 lever;

    if (PL_id == 0) {
        sw = ~p1sw_1 & p1sw_0;
    } else {
        sw = ~p2sw_1 & p2sw_0;
    }

    lever = Disposal_Of_Diagonal(sw);
    sw &= SWK_ATTACKS;

    switch (g_state.Deley_Shot_No[PL_id]) {
    case 0:
        if (!(sw & SWK_ATTACKS)) {
            break;
        }

        if (sw == (SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST)) {
            return lever | (SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST);
        }

        if (sw & (SWK_NORTH | SWK_SOUTH | SWK_RIGHT_TRIGGER | SWK_START)) {
            return sw | lever;
        }

        Color7[PL_id] = sw;
        g_state.Deley_Shot_No[PL_id] = 1;
        g_state.Deley_Shot_Timer[PL_id] = 3;

        break;

    case 1:
        Color7[PL_id] |= sw;

        if ((g_state.Deley_Shot_Timer[PL_id] -= 1) == 0) {
            return lever | Color7[PL_id];
        }

        if (Color7[PL_id] == (SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST)) {
            return lever | (SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST);
        }

        break;
    }

    return lever;
}

/** @brief Sel_PL phase 4 — wait for arts plate animation to finish, then enable cursor. */
static void Sel_PL_4th() {
    if (!g_state.Select_Arts[g_state.ID]) {
        g_state.SP_No[g_state.ID][0]++;
        g_state.Stop_Cursor[g_state.ID] = 0;
    }
}

/** @brief Sel_PL phase 5 — super-art selection input; check boss on completion. */
static void Sel_PL_5th() {
    if (g_state.Stop_Cursor[g_state.ID] != 0 || g_state.Face_Move != 0) {
        return;
    }

    if (g_state.Demo_Flag == 0) {
        if (g_state.ID) {
            Sel_Arts_Sub(1, Check_Demo_Data(1), 0);
        } else {
            Sel_Arts_Sub(0, Check_Demo_Data(0), 0);
        }
    } else if (g_state.ID) {
        Sel_Arts_Sub(1, ~p2sw_1 & p2sw_0, p2sw_0);
    } else {
        Sel_Arts_Sub(0, ~p1sw_1 & p1sw_0, p1sw_0);
    }

    if (!g_state.Sel_Arts_Complete[g_state.ID]) {
        return;
    }

    g_state.SP_No[g_state.ID][0]++;

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING ||
        g_state.Mode_Type == MODE_TRIALS) {
        g_state.select_phase[3] = 1;
    }

    if (g_state.plw[0].wu.pl_operator == 0 || g_state.plw[1].wu.pl_operator == 0) {
        Check_Boss(g_state.ID);
    }
}

/** @brief Sel_PL phase 6 — no-op (selection complete). */
static void Sel_PL_6th() {
    // Do nothing
}

/** @brief Strip diagonal input so only cardinal directions remain for the face grid. */
static u16 Disposal_Of_Diagonal(u16 sw) {
    sw &= SWK_DIRECTIONS;

    if (sw == SWK_UP) {
        return SWK_UP;
    }

    if (sw == SWK_DOWN) {
        return SWK_DOWN;
    }

    if (sw == (SWK_UP | SWK_RIGHT)) {
        return SWK_UP;
    }

    if (sw == (SWK_DOWN | SWK_LEFT)) {
        return SWK_DOWN;
    }

    return sw &= (SWK_LEFT | SWK_RIGHT);
}

/** @brief Character-grid cursor logic — move cursor, play SE, confirm on attack press. */
static void Sel_PL_Sub(s16 PL_id, u16 sw) {
    g_state.Cursor_Move[PL_id] = 0;

    if (g_state.Sel_PL_Complete[PL_id]) {
        return;
    }

    if (g_state.Time_Over) {
        sw = SWK_WEST;
    }

    if (sw == 0) {
        Auto_Repeat_Sub(PL_id);
    }

    if ((g_state.Cursor_Timer[PL_id] -= 1) == 0) {
        g_state.Cursor_Timer[PL_id] = 1;

        if (sw & SWK_RIGHT) {
            g_state.Cursor_Timer[PL_id] = 5;
            Sel_PL_MoveCursorRight(PL_id);
        } else if (sw & SWK_LEFT) {
            g_state.Cursor_Timer[PL_id] = 5;
            Sel_PL_MoveCursorLeft(PL_id);
        } else if (sw & SWK_UP) {
            g_state.Cursor_Timer[PL_id] = 5;
            Sel_PL_MoveCursorUp(PL_id);
        } else if (sw & SWK_DOWN) {
            g_state.Cursor_Timer[PL_id] = 5;
            Sel_PL_MoveCursorDown(PL_id);
        }
    }

    if (g_state.Cursor_Move[PL_id]) {
        Sound_SE(g_state.ID + 96);
    }

    if (!(sw & SWK_ATTACKS)) {
        return;
    }

    g_state.Sel_PL_Complete[PL_id] = 1;
    g_state.My_char[PL_id] = g_state.ID_of_Face[g_state.Cursor_Y[PL_id]][g_state.Cursor_X[PL_id]];

    if (g_state.Last_My_char2[PL_id] != g_state.My_char[PL_id]) {
        g_state.Arts_Y[g_state.ID] = g_state.Super_Arts[g_state.ID] = g_state.Last_Super_Arts[g_state.ID] = 0;
        g_state.Introduce_Boss[g_state.ID][0] = 0;
    }

    g_state.Last_My_char2[PL_id] = g_state.My_char[PL_id];
    g_state.Last_Selected_ID = PL_id;
    g_state.Order[1] = 2;
    g_state.Order_Timer[1] = 1;
    g_state.Order_Dir[1] = 8;

    if (g_state.Select_Status[0] != 3) {
        g_state.Order[2] = 1;
        g_state.Order_Timer[2] = 10;
        g_state.Order_Dir[2] = 4;
    }

    Sound_SE(g_state.ID + 98);
    Sound_SE(*Free_Ptr[PL_id]++);
    Setup_PL_Color(PL_id, sw);
    Correct_Control_Time(PL_id);
}

/** @brief Move cursor right on the face grid, wrapping rows. */
static void Sel_PL_MoveCursorRight(s16 PL_id) {
    if (g_state.Cursor_X[PL_id] == 7) {
        return;
    }

    g_state.Cursor_Move[PL_id] = 1;

    do {
        g_state.Cursor_Y[PL_id]++;

        switch (g_state.Cursor_X[PL_id]) {
        case 6:
            if (g_state.Cursor_Y[PL_id] > 1) {
                g_state.Cursor_Y[PL_id] = 1;
                g_state.Cursor_X[PL_id] = 0;
            }

            break;

        default:
            if (g_state.Cursor_Y[PL_id] > 2) {
                g_state.Cursor_Y[PL_id] = 0;
                g_state.Cursor_X[PL_id]++;
            }

            break;
        }
    } while (!permission_player[g_state.Present_Mode]
                  .can_activate[Face_Cursor_Data[g_state.Cursor_Y[PL_id]][g_state.Cursor_X[PL_id]]]);
}

/** @brief Move cursor left on the face grid, wrapping rows. */
static void Sel_PL_MoveCursorLeft(s16 PL_id) {
    if (g_state.Cursor_X[PL_id] == 7) {
        return;
    }

    g_state.Cursor_Move[PL_id] = 1;

    do {
        g_state.Cursor_Y[PL_id]--;

        switch (g_state.Cursor_X[PL_id]) {
        case 0:
            if (g_state.Cursor_Y[PL_id] <= 0) {
                g_state.Cursor_Y[PL_id] = 1;
                g_state.Cursor_X[PL_id] = 6;
            }
            break;

        case 1:
            if (g_state.Cursor_Y[PL_id] < 0) {
                g_state.Cursor_Y[PL_id] = 2;
                g_state.Cursor_X[PL_id] = 0;
            }
            break;

        default:
            if (g_state.Cursor_Y[PL_id] < 0) {
                g_state.Cursor_Y[PL_id] = 2;
                g_state.Cursor_X[PL_id]--;
            }

            break;
        }
    } while (!permission_player[g_state.Present_Mode]
                  .can_activate[Face_Cursor_Data[g_state.Cursor_Y[PL_id]][g_state.Cursor_X[PL_id]]]);
}

/** @brief Move cursor up on the face grid, wrapping columns. */
static void Sel_PL_MoveCursorUp(s16 PL_id) {
    g_state.Cursor_Move[PL_id] = 1;

    do {
        g_state.Cursor_X[PL_id]++;

        switch (g_state.Cursor_Y[PL_id]) {
        case 0:
            if (g_state.Cursor_X[PL_id] > 6) {
                g_state.Cursor_X[PL_id] = 1;
            }

            break;

        case 1:
            if (g_state.Cursor_X[PL_id] > 7) {
                g_state.Cursor_X[PL_id] = 0;
            }

            break;

        default:
            if (g_state.Cursor_X[PL_id] > 5) {
                g_state.Cursor_X[PL_id] = 0;
            }

            break;
        }
    } while (!permission_player[g_state.Present_Mode]
                  .can_activate[Face_Cursor_Data[g_state.Cursor_Y[PL_id]][g_state.Cursor_X[PL_id]]]);
}

/** @brief Move cursor down on the face grid, wrapping columns. */
static void Sel_PL_MoveCursorDown(s16 PL_id) {
    g_state.Cursor_Move[PL_id] = 1;

    do {
        g_state.Cursor_X[PL_id]--;

        switch (g_state.Cursor_Y[PL_id]) {
        case 0:
            if (g_state.Cursor_X[PL_id] <= 0) {
                g_state.Cursor_X[PL_id] = 6;
            }

            break;

        case 1:
            if (g_state.Cursor_X[PL_id] < 0) {
                g_state.Cursor_X[PL_id] = 7;
            }

            break;

        default:
            if (g_state.Cursor_X[PL_id] < 0) {
                g_state.Cursor_X[PL_id] = 5;
            }

            break;
        }
    } while (!permission_player[g_state.Present_Mode]
                  .can_activate[Face_Cursor_Data[g_state.Cursor_Y[PL_id]][g_state.Cursor_X[PL_id]]]);
}

/** @brief Auto-repeat logic for held directions on the character grid (accelerating repeat). */
static void Auto_Repeat_Sub(s16 PL_id) {
    u16 sw;

    if (g_state.Demo_Flag == 0) {
        return;
    }

    if (g_state.Cursor_Move[PL_id]) {
        return;
    }

    if (PL_id == 0) {
        sw = p1sw_0;
    } else {
        sw = p2sw_0;
    }

    sw = Disposal_Of_Diagonal(sw);

    switch (g_state.Auto_No[PL_id]) {
    case 0:
        if (sw & SWK_RIGHT) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 8;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data[0];
            g_state.Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_LEFT) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 4;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data[0];
            g_state.Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_UP) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 1;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data[0];
            g_state.Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_DOWN) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 2;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data[0];
            g_state.Auto_Index[PL_id] = 1;
        }

        break;

    case 1:
        if (sw != g_state.Auto_Cursor[PL_id]) {
            g_state.Auto_No[PL_id] = 0;
            break;
        }

        if (g_state.Auto_Timer[PL_id] -= 1) {
            break;
        }

        g_state.Auto_Timer[PL_id] = Repeat_Time_Data[g_state.Auto_Index[PL_id]];
        g_state.Auto_Index[PL_id]++;

        if ((g_state.Auto_Index[PL_id]) > 2) {
            g_state.Auto_Index[PL_id] = 2;
        }

        if (sw & SWK_RIGHT) {
            Sel_PL_MoveCursorRight(PL_id);
        }

        if (sw & SWK_LEFT) {
            Sel_PL_MoveCursorLeft(PL_id);
        }

        if (sw & SWK_UP) {
            Sel_PL_MoveCursorUp(PL_id);
        }

        if (sw & SWK_DOWN) {
            Sel_PL_MoveCursorDown(PL_id);
        }

        break;
    }
}

/** @brief Auto-repeat logic for the super-art plate (up/down only, instant repeat). */
static u16 Auto_Repeat_Wait(s16 PL_id) {
    u16 sw;

    if (g_state.Cursor_Move[PL_id] || g_state.Demo_Flag == 0) {
        return 0;
    }

    if (PL_id == 0) {
        sw = p1sw_0;
    } else {
        sw = p2sw_0;
    }

    switch (g_state.Auto_No[PL_id]) {
    case 0:
        if (sw & SWK_UP) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 1;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data_Wife[0];
            g_state.Auto_Index[PL_id] = 1;
        } else if (sw & SWK_DOWN) {
            g_state.Auto_No[PL_id] = 1;
            g_state.Auto_Cursor[PL_id] = 2;
            g_state.Auto_Timer[PL_id] = Repeat_Time_Data_Wife[0];
            g_state.Auto_Index[PL_id] = 1;
        }

        break;

    case 1:
        sw &= g_state.Auto_Cursor[PL_id];

        if (sw) {
            if (g_state.Auto_Timer[PL_id] -= 1) {
                break;
            }

            g_state.Auto_Timer[PL_id] = Repeat_Time_Data_Wife[g_state.Auto_Index[PL_id]++];

            if (g_state.Auto_Index[PL_id] > 2) {
                g_state.Auto_Index[PL_id] = 2;
            }

            if (sw & SWK_UP) {
                return SWK_UP;
            }

            if (sw & SWK_DOWN) {
                return SWK_DOWN;
            }

            break;
        }

        g_state.Auto_No[PL_id] = 0;
        break;
    }

    return 0;
}

/** @brief Super-art selector — move art plate up/down, confirm on attack. */
static void Sel_Arts_Sub(s16 PL_id, u16 sw, u16 /* unused */) {
    u16 lever_sw;

    if (g_state.Sel_Arts_Complete[PL_id]) {
        return;
    }

    if (g_state.Moving_Plate_Counter[PL_id]) {
        return;
    }

    if (g_state.Moving_Plate[PL_id]) {
        return;
    }

    if (g_state.Plate_Disposal_No[PL_id][0] != 0 || g_state.Plate_Disposal_No[PL_id][1] != 0 ||
        g_state.Plate_Disposal_No[PL_id][2] != 0) {
        return;
    }

    if (g_state.Time_Over) {
        sw = SWK_WEST;
    }

    lever_sw = sw & SWK_DIRECTIONS;

    if (lever_sw == 0) {
        sw |= Auto_Repeat_Wait(PL_id);
    }

    if (sw & SWK_DOWN) {
        Sound_SE(g_state.ID + 96);
        g_state.Moving_Plate[PL_id] = 2;
        g_state.Moving_Plate_Counter[PL_id] = 3;
        g_state.OK_Priority[PL_id] = 0;

        if ((g_state.Arts_Y[PL_id] += 1) > 2) {
            g_state.Arts_Y[PL_id] = 0;
        }
    }

    if (sw & SWK_UP) {
        Sound_SE(g_state.ID + 96);
        g_state.Moving_Plate[PL_id] = 1;
        g_state.Moving_Plate_Counter[PL_id] = 3;
        g_state.OK_Priority[PL_id] = 0;

        if ((g_state.Arts_Y[PL_id] -= 1) < 0) {
            g_state.Arts_Y[PL_id] = 2;
        }
    }

    if (sw & SWK_ATTACKS) {
        g_state.Stop_Cursor[g_state.ID] = 1;
        g_state.Slide_Type = PL_id;
        g_state.Sel_Arts_Complete[PL_id] = 1;
        g_state.Last_Super_Arts[PL_id] = g_state.Super_Arts[PL_id] = g_state.Arts_Y[PL_id];
        Sound_SE(g_state.ID + 98);
        Sound_SE(*Free_Ptr[PL_id]++);
        Setup_ID();

        if (g_state.Used_char[PL_id] != g_state.My_char[PL_id]) {
            g_state.Last_Player_id = PL_id;
        }

        g_state.Used_char[PL_id] = g_state.My_char[PL_id];
    }
}

/** @brief Exit state machine dispatcher — run the active exit/handicap phase. */
static void Check_Exit() {
    void (*Sel_Exit_Tbl[10])() = { Exit_1st, Exit_2nd, Exit_3rd,     Exit_4th,     Exit_5th,
                                   Exit_6th, Exit_7th, Handicap_1st, Handicap_2nd, Handicap_3rd };

    /* NOTE: Do NOT call rmlui_char_select_hide() here.
     * Keeping rmlui_char_select_visible == true ensures native effects
     * (eff42 timer, eff79 SA plates, etc.) stay gated through the exit
     * transition.  All visible RmlUI elements already hide themselves
     * via data bindings (g_state.Exit_No >= 1), and the auto-hide in
     * rmlui_char_select_update() cleans up when g_state.Play_Game != 0. */

    Sel_Exit_Tbl[g_state.Exit_No]();
}

/** @brief Exit phase 1 — wait until all operators have arts complete, dismiss red lines, route to handicap or normal
 * exit. */
static void Exit_1st() {
    if (g_state.plw[0].wu.pl_operator != 0 && g_state.Sel_Arts_Complete[0] >= 0) {
        return;
    }

    if (g_state.plw[1].wu.pl_operator != 0 && g_state.Sel_Arts_Complete[1] >= 0) {
        return;
    }

    Go_Away_Red_Lines();
    g_state.Order[4] = 4;
    g_state.Order_Timer[4] = 1;
    g_state.Order[7] = 4;
    g_state.Order[8] = 4;
    g_state.Order_Timer[7] = 1;
    g_state.Order_Timer[8] = 1;
    Setup_Training_Difficulty();

    if (g_state.Mode_Type == MODE_VERSUS && CurrentSave()->Handicap != 0) {
        g_state.Exit_No = 7;
    } else {
        g_state.Exit_No++;
    }

    if (g_state.Demo_Flag) {
        g_state.entry_phase[0] = 3;
        g_state.entry_phase[1] = 0;
        g_state.entry_phase[2] = 0;
        g_state.entry_phase[3] = 0;
    }
}

/** @brief Exit phase 2 — determine battle country/stage, queue BG load, start exit timer. */
static void Exit_2nd() {
    s16 xx;

    g_state.select_phase[1] = 0;

    if (g_state.Select_Status[0] == 3) {
        g_state.Exit_No = 3;
        g_state.Last_My_char[0] = g_state.My_char[0];
        g_state.Last_My_char[1] = g_state.My_char[1];
        g_state.Battle_Country = Setup_Battle_Country();
        g_state.bg_w.stage = g_state.Battle_Country;
        g_state.bg_w.area = 0;

        if (Debug_w[DEBUG_STAGE_SELECT]) {
            g_state.Battle_Country = g_state.bg_w.stage = Debug_w[DEBUG_STAGE_SELECT] - 1;
        }

        Push_LDREQ_Queue_BG(g_state.bg_w.stage + 0);
        return;
    }

    if (g_state.Scene_Cut) {
        g_state.Exit_Timer = 1;
    } else {
        g_state.Exit_Timer = 60;
    }

    g_state.Exit_No++;
    g_state.Last_My_char[g_state.Player_id] = g_state.My_char[g_state.Player_id];
    g_state.Time_Stop = 2;

    for (xx = 0; xx < 4; xx++) {
        g_state.next_cpu_phase[xx] = 0;
    }
}

/** @brief Exit phase 3 — run Select_CPU_First, then set g_state.EM_Rank for the upcoming fight. */
static void Exit_3rd() {
    if (!Select_CPU_First()) {
        return;
    }

    g_state.Exit_No++;
    g_state.select_phase[1] = 0;
    g_state.Suicide[3] = 1;

    if (g_state.VS_Index[g_state.Player_id] >= 9) {
        g_state.EM_Rank = 1;
        return;
    }

    g_state.EM_Rank = 0;
}

/** @brief Exit phase 4 — fade in, start BGM, spawn VS-screen objects. */
static void Exit_4th() {
    FadeInit();
    FadeIn(0, 4, 8);
    g_state.Exit_No++;
    g_state.Forbid_Break = 0;
    g_state.Suicide[0] = 1;
    g_state.Menu_Suicide[0] = 1;
    g_state.bgPalCodeOffset[0] = 144;
    BGM_Request(51);
    g_state.Exit_Timer = 240;
    effect_58_init(17, 2, 0);

    if (g_state.Select_Status[0] != 3) {
        effect_K6_init(0, 35, 35, 2);
        g_state.Order[35] = 3;
        g_state.Order_Timer[35] = 1;
        effect_K6_init(1, 36, 35, 2);
        g_state.Order[36] = 3;
        g_state.Order_Timer[36] = 1;
        effect_39_init(0, 17, g_state.My_char[0], 2, 0);
        g_state.Order[17] = 3;
        g_state.Order_Timer[17] = 1;
        effect_39_init(1, 18, g_state.My_char[1], 2, 0);
        g_state.Order[18] = 3;
        g_state.Order_Timer[18] = 1;
        effect_K6_init(0, 29, 29, 2);
        g_state.Order[29] = 3;
        g_state.Order_Timer[29] = 1;
        effect_K6_init(1, 30, 29, 2);
        g_state.Order[30] = 3;
        g_state.Order_Timer[30] = 1;
    } else if (g_state.Win_Record[g_state.Champion]) {
        effect_76_init(72);
        g_state.Order[72] = 3;
        g_state.Order_Timer[72] = 1;
        effect_76_init(73);
        g_state.Order[73] = 3;
        g_state.Order_Timer[73] = 1;
    }

    effect_43_init(2, 2);
    g_state.Order[42] = 2;
    g_state.Order_Timer[42] = 1;
    g_state.Order_Dir[42] = 5;
}

/** @brief Exit phase 5 — count down while fading, then advance. */
static void Exit_5th() {
    g_state.Exit_Timer--;

    if (!FadeIn(0, 4, 8)) {
        return;
    }

    g_state.Exit_No++;

    if (g_state.Exit_Timer < 0) {
        g_state.Exit_Timer = 1;
    }
}

/** @brief Exit phase 6 — wait for all loads, then count down exit timer and init omop. */
static void Exit_6th() {
    if (!Check_PL_Load()) {
        return;
    }

    if (!Check_LDREQ_Queue_BG(g_state.bg_w.stage + 0)) {
        return;
    }

    // We shouldn't skip VS screen in network mode, because that can lead to IO race conditions
    if (g_state.Scene_Cut && (g_state.Mode_Type != MODE_NETWORK)) {
        g_state.Exit_Timer = 1;
    }

    if ((g_state.Exit_Timer -= 1) == 0) {
        g_state.Exit_No++;
        init_omop();
    }
}

/** @brief Exit phase 7 — set final battle stage and signal exit. */
static void Exit_7th() {
    g_state.bg_w.stage = g_state.Battle_Country;
    g_state.bg_w.area = 0;
    SEL_PL_X = 1;

    if (use_rmlui && rmlui_screen_select)
        rmlui_char_select_hide();
}

/** @brief Handicap phase 1 — spawn handicap menu UI (vital bars, stage selector, labels). */
static void Handicap_1st() {
    g_state.Exit_No++;
    Decide_Stage = 0;
    Menu_Common_Init();
    Setup_Training_Difficulty();
    g_state.SP_No[0][2] = 0;
    g_state.SP_No[1][2] = 0;
    effect_66_init(138, 31, 0, 2, -1, -1, -0x7FF8);
    g_state.Order[138] = 3;
    g_state.Order_Timer[138] = 1;
    effect_66_init(139, 35, 0, 2, 71, 20, 0);
    g_state.Order[139] = 5;
    effect_66_init(140, 36, 0, 2, 71, 20, 0);
    g_state.Order[140] = 5;
    effect_66_init(141, 37, 0, 2, 71, 21, 0);
    g_state.Order[141] = 5;
    effect_66_init(91, 28, 0, 2, 71, 15, 0);
    g_state.Order[91] = 3;
    g_state.Order_Timer[91] = 1;
    effect_66_init(92, 29, 0, 2, 71, 16, 0);
    g_state.Order[92] = 3;
    g_state.Order_Timer[92] = 1;
    effect_66_init(93, 30, 0, 2, 71, 17, 0);
    g_state.Order[93] = 3;
    g_state.Order_Timer[93] = 1;
    effect_66_init(120, 32, 0, 2, 71, 18, 0);
    g_state.Order[120] = 2;
    g_state.Order_Timer[120] = 1;
    effect_66_init(121, 33, 0, 2, 71, 18, 0);
    g_state.Order[121] = 2;
    g_state.Order_Timer[121] = 1;
    effect_66_init(122, 34, 0, 2, 71, 19, 0);
    g_state.Order[122] = 5;
    effect_99_init(0, 0, 0x7047, 0, 0, 0);
    effect_99_init(1, 0, 0x7047, 1, 1, 0);
    effect_99_init(255, 1, 0x7047, 2, 2, 0);
    effect_99_init(255, 1, 0x70A7, 3, 3, 0);
    effect_99_init(255, 1, 0x70A7, 4, 4, 0);
}

/** @brief Handicap phase 2 — run per-player handicap control. */
static void Handicap_2nd() {
    g_state.ID2 = 0;
    Handicap_Control();
    g_state.ID2 = 1;
    Handicap_Control();
}

/** @brief Handicap phase 3 — fade BGM and return to exit phase 1 when timer expires. */
static void Handicap_3rd() {
    if (g_state.select_timer_legacy == 9) {
        SsBgmFadeOut(0x1000);
    }

    if ((g_state.select_timer_legacy -= 1) == 0) {
        g_state.Exit_No = 1;
    }
}

/** @brief Per-player handicap sub-state machine dispatcher. */
static void Handicap_Control() {
    switch (g_state.SP_No[g_state.ID2][2]) {
    case HANDICAP_1:
        Handicap_1();
        break;
    case HANDICAP_2:
        Handicap_2();
        break;
    case HANDICAP_3:
        Handicap_3();
        break;
    case HANDICAP_4:
        Handicap_4();
        break;
    }
}

/** @brief Handicap sub 1 — vital-bar selection for this player; advance when confirmed. */
static void Handicap_1() {
    Handicap_Vital_Select(g_state.ID2);

    if (!(g_state.IO_Result & 0x100)) {
        return;
    }

    SE_selected();
    g_state.Order[g_state.ID2 + 120] = 5;
    g_state.Order[g_state.ID2 + 139] = 6;
    g_state.Order_Timer[g_state.ID2 + 139] = 1;

    if (g_state.SP_No[g_state.ID2 ^ 1][2] == 2) {
        g_state.SP_No[g_state.ID2][2] = 1;
        return;
    }

    g_state.SP_No[g_state.ID2][2] = 2;

    if (g_state.SP_No[g_state.ID2 ^ 1][2] < 3) {
        g_state.Order[122] = 2;
        g_state.Order_Timer[122] = 1;
    }
}

/** @brief Handicap sub 2 — wait or go back if other player cancelled; otherwise proceed to stage. */
static void Handicap_2() {
    u16 sw;

    if (g_state.ID2 == 0) {
        sw = ~p1sw_1 & p1sw_0;
    } else {
        sw = ~p2sw_1 & p2sw_0;
    }

    if (sw & SWK_EAST && Decide_Stage == 0) {
        g_state.SP_No[g_state.ID2][2] = 0;
        SE_selected();
        g_state.Order[g_state.ID2 + 139] = 5;
        g_state.Order[g_state.ID2 + 120] = 2;
        g_state.Order_Timer[g_state.ID2 + 120] = 1;
        return;
    }

    if (g_state.SP_No[g_state.ID2 ^ 1][2] == 0) {
        g_state.SP_No[g_state.ID2][2] = 2;
        g_state.Order[122] = 2;
        g_state.Order_Timer[122] = 1;
    }
}

u8 hc3alphaadd = { 1 };

/** @brief Handicap sub 3 — stage selection with flashing cursor; back or confirm. */
static void Handicap_3() {
    Handicap_Stage_Select(g_state.ID2);

    if (g_state.IO_Result & 0x100) {
        g_state.SP_No[g_state.ID2][2]++;
        SE_selected();
        g_state.Order[141] = 6;
        g_state.Order_Timer[141] = 1;
        g_state.Order[122] = 5;
        Decide_Stage = 1;
        return;
    }

    if (g_state.IO_Result & 0x200 && Decide_Stage == 0) {
        g_state.SP_No[g_state.ID2][2] = 0;
        SE_selected();
        g_state.Order[122] = 5;
        g_state.Order[g_state.ID2 + 139] = 5;
        g_state.Order[g_state.ID2 + 120] = 2;
        g_state.Order_Timer[g_state.ID2 + 120] = 1;
    }

    hc3alpha += hc3alphaadd;
    hc3alpha &= 0xF;

    if (hc3alpha == 0) {
        if (hc3alphaadd == 1) {
            hc3alpha = 16;
        }

        hc3alphaadd = -hc3alphaadd;
    }

    if (Decide_Stage != 0) {
        return;
    }

    if (g_state.ID2) {
        f32 dmypos[8] = { 296.0f, 90.0f, 296.0f, 98.0f, 284.0f, 90.0f, 268.0f, 112.0f };
        Renderer_Queue2DPrimitive(dmypos, PrioBase[2], (hc3alpha + 48) * 0x1000000 | 0xFFFFFF, 0);
    } else {
        f32 dmypos[8] = { 88.0f, 90.0f, 88.0f, 98.0f, 100.0f, 90.0f, 116.0f, 112.0f };
        Renderer_Queue2DPrimitive(dmypos, PrioBase[2], (hc3alpha + 48) * 0x1000000 | 0xFFFFFF, 0);
    }
}

/** @brief Handicap sub 4 — wait for both players to finish, then advance to exit timer. */
static void Handicap_4() {
    if (g_state.SP_No[0][2] > 0 && g_state.SP_No[1][2] > 0) {
        g_state.Exit_No = 9;
        g_state.select_timer_legacy = 60;
    }
}

/** @brief Read pad input and process vital-bar handicap lever movement. */
static void Handicap_Vital_Select(s16 PL_id) {
    Setup_Pad_or_Stick();
    g_state.IO_Result = Check_Menu_Lever(PL_id, 0);
    Handicap_Vital_Move_Sub(g_state.IO_Result, PL_id);
}

/** @brief Move the vital-bar handicap slider left/right (direction swapped for 2P). */
static u16 Handicap_Vital_Move_Sub(u16 sw, s16 PL_id) {
    if (PL_id == 0) {
        switch (sw) {
        case SWK_LEFT:
            if ((g_state.Vital_Handicap[g_state.Present_Mode][PL_id] += 1) > 7) {
                g_state.Vital_Handicap[g_state.Present_Mode][PL_id] = 7;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_LEFT;

        case SWK_RIGHT:
            if ((g_state.Vital_Handicap[g_state.Present_Mode][PL_id] -= 1) < 0) {
                g_state.Vital_Handicap[g_state.Present_Mode][PL_id] = 0;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_RIGHT;
        }
    } else {
        switch (sw) {
        case SWK_LEFT:
            if ((g_state.Vital_Handicap[g_state.Present_Mode][PL_id] -= 1) < 0) {
                g_state.Vital_Handicap[g_state.Present_Mode][PL_id] = 0;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_LEFT;

        case SWK_RIGHT:
            if ((g_state.Vital_Handicap[g_state.Present_Mode][PL_id] += 1) > 7) {
                g_state.Vital_Handicap[g_state.Present_Mode][PL_id] = 7;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_RIGHT;
        }
    }

    return 0;
}

/** @brief Read pad input and process stage-select lever movement. */
static void Handicap_Stage_Select(s16 PL_id) {
    Setup_Pad_or_Stick();
    g_state.IO_Result = Check_Menu_Lever(PL_id, 0);
    Handicap_Stage_Move_Sub(g_state.IO_Result);
}

/** @brief Move the stage selector left/right, wrapping and skipping stage 17. */
static void Handicap_Stage_Move_Sub(u16 sw) {
    switch (sw) {
    case SWK_LEFT:
        if ((g_state.VS_Stage -= 1) < 0) {
            g_state.VS_Stage = 20;
        }

        if (g_state.VS_Stage == 17) {
            g_state.VS_Stage = 16;
        }

        SE_dir_cursor_move();
        break;

    case SWK_RIGHT:
        if ((g_state.VS_Stage += 1) > 20) {
            g_state.VS_Stage = 0;
        }

        if (g_state.VS_Stage == 17) {
            g_state.VS_Stage = 18;
        }

        SE_dir_cursor_move();
        break;
    }
}

/** @brief Reduce the select timer based on the player’s continue count. */
static void Correct_Control_Time(s16 PL_id) {
    u8 xx;
    u8 zz;

    if (g_state.Play_Type == 1) {
        return;
    }

    if (g_state.Stage_Continue[PL_id] == 0) {
        return;
    }

    xx = g_state.Stage_Continue[PL_id];

    if (g_state.VS_Index[PL_id] >= 9) {
        zz = 1;
    } else {
        zz = 0;
    }

    if (g_state.Stage_Continue[PL_id] >= 16) {
        xx = 16;
    } else {
        xx = g_state.Stage_Continue[PL_id];
    }

    g_state.Control_Time = g_state.SC_Personal_Time[PL_id] - Correct_Cont_Time_Data[zz][xx];

    if (g_state.Control_Time < 0) {
        g_state.Control_Time = 0;
    }

    g_state.SC_Personal_Time[PL_id] = g_state.Control_Time;
}

/** @brief If the player is at boss stage and hasn’t seen the intro, force max time and flag g_state.Break_Into_CPU. */
static s32 Check_Boss(s16 PL_id) {
    if (g_state.VS_Index[g_state.Player_id] >= 9 && g_state.Introduce_Boss[g_state.Player_id][1] == 0) {
        g_state.Control_Time = g_state.Limit_Time;
        g_state.SC_Personal_Time[PL_id] = g_state.Control_Time;
        return g_state.Break_Into_CPU = 1;
    }

    return g_state.Break_Into_CPU = 0;
}

/** @brief Pick the battle stage from g_state.VS_Stage, random, or character match-up. */
static u8 Setup_Battle_Country() {
    s16 Rnd32;

    if (g_state.Mode_Type == MODE_VERSUS) {
        if (g_state.VS_Stage == 20) {
            Rnd32 = random_32();
            return Random_Stage_Data[1][Rnd32];
        }

        return g_state.VS_Stage;
    }

    if (g_state.My_char[0] == 17 && g_state.My_char[1] == 17) {
        Rnd32 = random_32();
        return Random_Stage_Data[0][Rnd32];
    }

    if (g_state.My_char[g_state.New_Challenger] == 17) {
        return g_state.My_char[g_state.Champion];
    }

    return g_state.My_char[g_state.New_Challenger];
}
