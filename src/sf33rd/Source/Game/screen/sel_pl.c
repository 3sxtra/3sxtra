/**
 * @file sel_pl.c
 * Character/Super Art selection screen
 */

#include "sf33rd/Source/Game/screen/sel_pl.h"
#include "common.h"
#include "constants.h"
#include "port/renderer.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/com/com_data.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo_dat.h"
#include "sf33rd/Source/Game/effect/eff38.h"
#include "sf33rd/Source/Game/effect/eff39.h"
#include "sf33rd/Source/Game/effect/eff42.h"
#include "sf33rd/Source/Game/effect/eff43.h"
#include "sf33rd/Source/Game/effect/eff50.h"
#include "sf33rd/Source/Game/effect/eff52.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff66.h"
#include "sf33rd/Source/Game/effect/eff69.h"
#include "sf33rd/Source/Game/effect/eff70.h"
#include "sf33rd/Source/Game/effect/eff75.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/eff79.h"
#include "sf33rd/Source/Game/effect/eff93.h"
#include "sf33rd/Source/Game/effect/eff99.h"
#include "sf33rd/Source/Game/effect/effd8.h"
#include "sf33rd/Source/Game/effect/effk6.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/screen/next_cpu.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "port/sdl/rmlui_char_select.h"
#include "port/sdl/rmlui_phase3_toggles.h"

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
static void Sel_PL_Sub_CR(s16 PL_id);
static void Sel_PL_Sub_CL(s16 PL_id);
static void Sel_PL_Sub_CU(s16 PL_id);
static void Sel_PL_Sub_CD(s16 PL_id);
static void Auto_Repeat_Sub(s16 PL_id);
static u16 Auto_Repeat_Sub_Wife(s16 PL_id);
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

#define SEL_PL_CONT_JMP_COUNT 4
#define FACE_JMP_COUNT 4
#define OBJ_JMP_COUNT 3
#define PL_SEL_JMP_COUNT 5
#define SEL_PL_JMP_COUNT 6
#define HANDICAP_JMP_COUNT 4

u8 SEL_PL_X;
s16 Play_Type_1st;
u16 Color7[2];
u8 Decide_Stage;
u8 hc3alpha;

const s16 Cursor_Y_Data[6] = { 80, 104, 128, 80, 104, 128 };

const u8 Arts_Y_Data[3][3] = { { 0, 1, 2 }, { 2, 0, 1 }, { 1, 2, 0 } };

const u8 Repeat_Time_Data[3] = { 26, 9, 7 };

const u8 Repeat_Time_Data_Wife[3] = { 1, 1, 1 };

/** @brief Main character-select dispatcher — run controls, per-player select, and return exit flag. */
s16 Select_Player() {
    SEL_PL_X = 0;

    if (Break_Into) {
        return 0;
    }

    Scene_Cut = Cut_Cut_Cut();
    Sel_PL_Control();
    Switch_Work();
    ID = 0;
    Sel_PL();
    ID = 1;
    Sel_PL();
    Time_Over = false;

    if (Check_Exit_Check() == 0 && Debug_w[DEBUG_TIME_STOP] == -1) {
        SEL_PL_X = 0;
    }

    return SEL_PL_X;
}

/** @brief Mirror input in training mode so the champion’s inputs control both sides. */
static void Switch_Work() {
    if (Mode_Type != MODE_NORMAL_TRAINING && Mode_Type != MODE_PARRY_TRAINING && Mode_Type != MODE_TRIALS) {
        return;
    }

    switch (S_No[3]) {
    case 0:
        if (Champion) {
            p1sw_0 = 0;
        } else {
            p2sw_0 = 0;
        }

        break;

    case 1:
        S_No[3]++;
        Default_Training_Data(0);
        Record_Data_Tr = 0;
        Training_Disp_Work_Clear();
        Menu_Cursor_X[0] = 0;
        Training_Cursor = 0;

        if (Champion) {
            p1sw_0 = p2sw_0;
            p1sw_1 = p2sw_0;
        } else {
            p2sw_0 = p1sw_0;
            p2sw_1 = p1sw_0;
        }

        break;

    case 2:
        if (Champion) {
            p1sw_0 = p2sw_0;
        } else {
            p2sw_0 = p1sw_0;
        }

        break;
    }
}

/** @brief Top-level select-screen controller — run status, face, OBJ, player-select, and exit phases. */
static void Sel_PL_Control() {
    void (*Sel_PL_Cont_Tbl[SEL_PL_CONT_JMP_COUNT])() = {
        Sel_PL_Cont_1st, Sel_PL_Cont_2nd, Sel_PL_Cont_3rd, Sel_PL_Cont_4th
    };
    Setup_Select_Status();
    if (S_No[0] < SEL_PL_CONT_JMP_COUNT) {
        Sel_PL_Cont_Tbl[S_No[0]]();
    }
    Face_Control();
    OBJ_Control();
    ID2 = 0;
    Player_Select_Control();
    ID2 = 1;
    Player_Select_Control();
    Check_Exit();
}

/** @brief Select control phase 1 — screen switch, clear state, build texcache, init BG/faces/timer. */
static void Sel_PL_Cont_1st() {
    s16 xx;

    Switch_Screen(1);
    S_No[0]++;
    All_Clear_Suicide();
    SsBgmHalfVolume(0);
    Face_No[0] = 0;
    Face_No[1] = 0;
    SO_No[0] = 0;
    SO_No[1] = 0;
    Exit_No = 0;
    Fade_Flag = 0;
    judge_flag = 0;
    Game_pause = 0;

    for (xx = 0; xx < 4; xx++) {
        SP_No[0][xx] = 0;
        SP_No[1][xx] = 0;
    }

    Purge_mmtm_area(2);
    Make_texcash_of_list(2);
    bg_etc_write(2);
    Setup_Aborigine();
    Initialize_BG();
    Setup_Cursor_Y();

    if (Present_Mode == 4 || Present_Mode == 5) {
        Select_Timer = 0x20;
    } else {
        Select_Timer = 0x30;
    }

    Unit_Of_Timer = UNIT_OF_TIMER_MAX;
    Setup_Face_ID();
    Setup_1st_Play_Type();
    Setup_Face_Sub();
    Time_Stop = 1;
    SelectTimer_Init();
    Face_MV_Request = 0;
    Face_Status = 0;
    Face_Move = 0;
    Break_Into_CPU = 0;
    Explosion = 0;
    Time_Over = false;
    Move_Super_Arts[0] = 0;
    Move_Super_Arts[1] = 0;
    Flash_Complete[0] = 0;
    Flash_Complete[1] = 0;
    Cursor_Move[0] = 0;
    Cursor_Move[1] = 0;
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

    if (Mode_Type == MODE_NETWORK) {
        return;
    }

    permission_player[1].ok[0] = 0;
    permission_player[4].ok[0] = 0;
    permission_player[5].ok[0] = 0;

    if (save_w[Present_Mode].Unlock_All) {
        permission_player[1].ok[0] = 1;
        permission_player[4].ok[0] = 1;
        permission_player[5].ok[0] = 1;
        return;
    }

    for (ix = 1; ix < 20; ix++) {
        if (save_w[1].PL_Color[0][ix] == 0) {
            return;
        }
    }

    permission_player[1].ok[0] = 1;
    permission_player[4].ok[0] = 1;
    permission_player[5].ok[0] = 1;
}

/** @brief Select control phase 2 — init screen-switch revival, request entry state, clear flash. */
static void Sel_PL_Cont_2nd() {
    Switch_Screen(1);
    Switch_Screen_Init(1);
    S_No[0]++;
    Request_E_No = 1;
    Clear_Flash_No();
}

/** @brief Select control phase 3 — wait for screen revival, then enable break-in and clear demo flag. */
static void Sel_PL_Cont_3rd() {
    if (!Switch_Screen_Revival(0)) {
        return;
    }

    S_No[0]++;
    Forbid_Break = 0;

    if (G_No[1] != 1) {
        // This is a comparison to zero in the decomp. Might be a programmer error
        Demo_Flag = 0;
    }
}

/** @brief Select control phase 4 — intentionally empty (placeholder). */
static void Sel_PL_Cont_4th() {
    // Do nothing
}

/** @brief Populate ID_of_Face grid from the Face_Cursor_Data layout table. */
static void Setup_Face_ID() {
    s16 x;
    s16 y;

    for (y = 0; y < 3; y++) {
        for (x = 0; x < 8; x++) {
            ID_of_Face[y][x] = Face_Cursor_Data[y][x];
        }
    }
}

/** @brief Record the initial play-type so we know whether a second player joined later. */
static void Setup_1st_Play_Type() {
    if (Play_Type == 1) {
        Play_Type_1st = 99;
    } else {
        Play_Type_1st = Aborigine;
    }
}

/** @brief Spawn all 19 character-face portrait effect objects on the grid. */
static void Setup_Face_Sub() {
    s16 x;

    Complete_Face = 19;

    for (x = 1; x < 20; x++) {
        effect_70_init(x);
    }
}

/** @brief Compute Select_Status from operator flags and arts-complete state. */
static void Setup_Select_Status() {
    if (plw[0].wu.pl_operator) {
        Select_Status[0] = 1;
    } else {
        Select_Status[0] = 0;
    }

    if (plw[1].wu.pl_operator) {
        Select_Status[0] |= 2;
    }

    if (Sel_Arts_Complete[0] != -1 && plw[0].wu.pl_operator != 0) {
        Select_Status[1] = 1;
    } else {
        Select_Status[1] = 0;
    }

    if (Sel_Arts_Complete[1] != -1 && plw[1].wu.pl_operator != 0) {
        Select_Status[1] |= 2;
    }
}

/** @brief Determine Aborigine (which player selects first) from operator state. */
static u8 Setup_Aborigine() {
    if (Select_Status[0] == 3) {
        return Aborigine = 153;
    }

    if (Select_Status[0] == 1) {
        return Aborigine = 0;
    }

    return Aborigine = 1;
}

/** @brief Build the per-player Cursor_Y_Pos arrays from the Cursor_Y_Data table. */
static void Setup_Cursor_Y() {
    s16 i;
    s16 j;

    s16 a;
    s16 b;
    s16 c;
    s16 d;

    for (i = 2, a = j = 0; i >= 0; i--, b = j++) {
        Cursor_Y_Pos[0][i] = Cursor_Y_Data[j];
    }

    for (i = 2, c = j = 3; i >= 0; i--, d = j++) {
        Cursor_Y_Pos[1][i] = Cursor_Y_Data[j];
    }
}

/** @brief Set up all background layers for the character-select screen. */
static void Initialize_BG() {
    Setup_BG_General();
    Setup_BG(2, 512, 0);
    Setup_BG(3, 704, 0);
    Setup_FACE_BG();
}

/** @brief General BG setup — init zoom, store old position, set family. */
static void Setup_BG_General() {
    Zoomf_Init();
    bg_w.bgw[0].old_pos_x = bg_w.bgw[0].xy[0].disp.pos;
    bg_pos_hosei2();
    Bg_Family_Set();
}

/** @brief Set up the face-grid BG layer position and family data. */
static void Setup_FACE_BG() {
    s16 face_x;
    s16 face_y;

    Unsubstantial_BG[1] = 1;
    face_x = Setup_Face_X();
    face_y = Setup_Face_Y();
    bg_w.bgw[1].xy[0].disp.pos = face_x;
    bg_w.bgw[1].xy[1].disp.pos = face_y;
    bg_w.bgw[1].wxy[0].disp.pos = face_x;
    bg_w.bgw[1].wxy[1].disp.pos = face_y;
    bg_w.bgw[1].xy[0].disp.low = 0;
    bg_w.bgw[1].xy[1].disp.low = 0;
    bg_w.bgw[1].position_x = face_x;
    bg_w.bgw[1].position_y = face_y;
    bg_w.bgw[1].hos_xy[0].disp.pos = bg_w.bgw[1].wxy[0].disp.pos = bg_w.bgw[1].xy[0].disp.pos;
    Bg_Family_Set_Ex(1);
}

/** @brief Return the X offset for the face-grid BG based on play type and aborigine. */
static s16 Setup_Face_X() {
    if (Play_Type == 1) {
        return 604;
    }

    if (Aborigine == 0) {
        return 512;
    }

    return 696;
}

/** @brief Return the Y offset for the face-grid BG based on play type and aborigine. */
static s16 Setup_Face_Y() {
    if (Play_Type == 1) {
        return 0;
    }

    if (Aborigine == 0) {
        return -24;
    }

    return 0;
}

/** @brief Face-panel state machine — dispatch face phase and move the BG. */
static void Face_Control() {
    void (*Face_Jmp_Tbl[FACE_JMP_COUNT])() = { Face_1st, Face_2nd, Face_3rd, Face_4th };
    if (Face_No[0] < FACE_JMP_COUNT) {
        Face_Jmp_Tbl[Face_No[0]]();
    }
    Move_Face_BG();
}

/** @brief Face phase 1 — choose initial face layout (1P or 2P). */
static void Face_1st() {
    if (Select_Status[0] == 3) {
        Face_No[0] = 3;
    } else {
        Face_No[0] = 1;
    }
}

/** @brief Face phase 2 — slide face BG when second player joins or first completes. */
static void Face_2nd() {
    if (Select_Status[0] == 3 && Face_MV_Request == 0) {
        Face_No[0] = 3;
        Face_MV_Time = 1;

        if (Aborigine == 1) {
            Face_MV_Request = 2;
            bg_mvxy.a[0].sp = -0x90000;
            bg_mvxy.d[0].sp = -0x8000;
            return;
        }

        Face_MV_Request = 1;
        bg_mvxy.a[0].sp = 0x90000;
        bg_mvxy.d[0].sp = 0x8000;
        return;
    }

    if (Sel_PL_Complete[Aborigine]) {
        Face_MV_Time = 5;
        Face_No[0]++;

        if (Aborigine == 0) {
            Face_MV_Request = 4;
            bg_mvxy.a[0].sp = -0xC0000;
            bg_mvxy.d[0].sp = -0x8000;
            return;
        }

        Face_MV_Request = 3;
        bg_mvxy.a[0].sp = 0xC0000;
        bg_mvxy.d[0].sp = 0x8000;
    }
}

/** @brief Face phase 3 — slide face BG back when both players are selecting. */
static void Face_3rd() {
    if (Select_Status[0] != 3) {
        return;
    }

    if (Face_MV_Request != 0) {
        return;
    }

    Face_No[0]++;
    Face_MV_Time = 1;

    if (Aborigine == 1) {
        Face_MV_Request = 2;
        bg_mvxy.a[0].sp = -0xC0000;
        bg_mvxy.d[0].sp = -0x8000;
        return;
    }

    Face_MV_Request = 1;
    bg_mvxy.a[0].sp = 0xC0000;
    bg_mvxy.d[0].sp = 0x8000;
}

/** @brief Face phase 4 — no-op (face movement complete). */
static void Face_4th() {}

/** @brief Apply pending face-move requests as effect_93 BG scrolls. */
static void Move_Face_BG() {
    switch (Face_No[1]) {
    case 0:
        if (Face_MV_Request) {
            Face_No[1]++;
            Face_Move = Face_MV_Request;
            effect_93_init(Face_Move - 1, Face_MV_Time);
        }

        break;

    default:
        if (!(Face_MV_Request = Face_Move)) {
            Face_No[1] = 0;
        }

        break;
    }
}

/** @brief OBJ state machine — dispatch portrait/plate object initialisation phases. */
static void OBJ_Control() {
    void (*OBJ_Jmp_Tbl[OBJ_JMP_COUNT])() = { OBJ_1st, OBJ_2nd, OBJ_3rd };
    if (SO_No[0] < OBJ_JMP_COUNT) {
        OBJ_Jmp_Tbl[SO_No[0]]();
    }
}

/** @brief OBJ phase 1 — spawn all character-select UI objects (portraits, name plates, effects). */
static void OBJ_1st() {
    Setup_EFF69();

    if (Select_Status[0] != 3) {
        SO_No[0] = 1;
        effect_38_init(Aborigine, Aborigine + 11, 127, 0, 2);
        Order[Aborigine + 11] = 1;
        Order_Timer[Aborigine + 11] = 35;
        effect_52_init(Aborigine, 37);
        Order[37] = 1;
        Order_Timer[37] = 30;
        Order_Dir[37] = 0;
        effect_K6_init(Aborigine, Aborigine + 31, 31, 2);
        Order[Aborigine + 31] = 1;
        Order_Timer[Aborigine + 31] = 35;
        Order_Dir[Aborigine + 31] = 0;
        effect_K6_init(Aborigine, Aborigine + 25, 25, 2);
        Order[Aborigine + 25] = 1;
        Order_Timer[Aborigine + 25] = 35;
        Order_Dir[Aborigine + 25] = 0;
        Order[0] = 1;
        Order_Timer[0] = 40;
        Order_Dir[0] = 4;
        Order[1] = 1;
        Order_Timer[1] = 45;
        Order_Dir[1] = 4;
        Order[3] = 1;
        Order_Timer[3] = 45;
        Order_Dir[3] = 4;
        effect_39_init(Aborigine, Aborigine + 13, 127, 2, 1);
        Order[Aborigine + 13] = 1;
        Order_Timer[Aborigine + 13] = 35;
        Order_Dir[Aborigine + 13] = 0;
        effect_42_init(5);
        Order[5] = 1;
        Order_Timer[5] = 45;
        Order_Dir[5] = 4;
        effect_42_init(6);
        Order[6] = 1;
        Order_Timer[6] = 45;
        Order_Dir[6] = 4;
        return;
    }

    SO_No[0] = 2;
    effect_75_init(42, 3, 2);
    Order[42] = 3;
    Order_Timer[42] = 1;
    Order_Dir[42] = 3;
    effect_38_init(0, 11, 127, 1, 2);
    Order[11] = 1;
    Order_Timer[11] = 86;
    effect_38_init(1, 12, 127, 1, 2);
    Order[12] = 1;
    Order_Timer[12] = 86;
    effect_K6_init(0, 33, 31, 2);
    Order[33] = 1;
    Order_Timer[33] = 86;
    Order_Dir[33] = 0;
    effect_52_init(0, 38);
    Order[38] = 3;
    Order_Timer[38] = 30;
    effect_K6_init(0, 27, 25, 2);
    Order[27] = 3;
    Order_Timer[27] = 86;
    effect_K6_init(1, 28, 25, 2);
    Order[28] = 3;
    Order_Timer[28] = 86;
    effect_K6_init(1, 34, 31, 2);
    Order[34] = 1;
    Order_Timer[34] = 86;
    Order_Dir[34] = 0;
    effect_52_init(1, 39);
    Order[39] = 3;
    Order_Timer[39] = 30;
    effect_39_init(0, 15, 127, 2, 0);
    Order[15] = 1;
    Order_Timer[15] = 86;
    Order_Dir[15] = 0;
    effect_39_init(1, 16, 127, 2, 0);
    Order[16] = 1;
    Order_Timer[16] = 86;
    Order_Dir[16] = 0;
    Order[4] = 3;
    Order_Timer[4] = 86;
    Order_Dir[4] = 255;
    effect_42_init(7);
    Order[7] = 0;
    Order_Timer[7] = 86;
    effect_42_init(8);
    Order[8] = 0;
    Order_Timer[8] = 86;
}

/** @brief OBJ phase 2 — reconfigure objects when a second player breaks in mid-select. */
static void OBJ_2nd() {
    if (Select_Status[0] != 3) {
        return;
    }

    SO_No[0]++;
    effect_75_init(42, 3, 2);
    Order[42] = 3;
    Order_Timer[42] = 1;
    Order_Dir[42] = 3;
    Order[Aborigine + 11] = 4;
    Order_Timer[Aborigine + 11] = 1;
    Select_Start[Aborigine] = 2;
    effect_38_init(New_Challenger, New_Challenger + 11, 127, 1, 2);
    Order[New_Challenger + 11] = 1;
    Order_Timer[New_Challenger + 11] = 1;
    Go_Away_Red_Lines();
    Order[Aborigine + 31] = 5;
    Order_Timer[Aborigine + 31] = 1;
    Order[Aborigine + 19] = 5;
    Order_Timer[Aborigine + 19] = 1;
    Order[Aborigine + 25] = 5;
    Order_Timer[Aborigine + 25] = 1;
    Order[Aborigine + 13] = 5;
    Order_Timer[Aborigine + 13] = 1;
    Order[37] = 4;
    Order_Timer[37] = 1;
    effect_K6_init(0, 33, 31, 2);
    Order[33] = 1;
    Order_Timer[33] = 1;
    Order_Dir[33] = 0;
    effect_K6_init(0, 27, 25, 2);
    Order[27] = 1;
    Order_Timer[27] = 1;
    Order_Dir[27] = 0;
    effect_39_init(0, 15, 127, 2, 0);
    Order[15] = 1;
    Order_Timer[15] = 1;
    Order_Dir[15] = 0;
    effect_K6_init(1, 34, 31, 2);
    Order[34] = 1;
    Order_Timer[34] = 1;
    Order_Dir[34] = 0;
    effect_K6_init(1, 28, 25, 2);
    Order[28] = 1;
    Order_Timer[28] = 1;
    Order_Dir[28] = 0;
    effect_39_init(1, 16, 127, 2, 0);
    Order[16] = 1;
    Order_Timer[16] = 1;
    Order_Dir[16] = 0;
    Order[4] = 3;
    Order_Timer[4] = 1;
    Order_Dir[4] = 255;
    effect_42_init(7);
    Order[7] = 0;
    Order_Timer[7] = 1;
    effect_42_init(8);
    Order[8] = 0;
    Order_Timer[8] = 1;
}

/** @brief OBJ phase 3 — no-op (object setup complete). */
static void OBJ_3rd() {}

/** @brief Spawn the 5 red-line / decoration effect-69 objects. */
static void Setup_EFF69() {
    s16 xx;

    for (xx = 0; xx < 5; xx++) {
        Order[xx] = 0;
        effect_69_init(xx);
    }
}

/** @brief Dismiss all red-line decoration objects with a fade-out animation. */
static void Go_Away_Red_Lines() {
    Order[0] = 2;
    Order_Timer[0] = 1;
    Order_Dir[0] = 8;
    Order[2] = 2;
    Order_Timer[2] = 1;
    Order_Dir[2] = 8;
    Order[1] = 2;
    Order_Timer[1] = 1;
    Order_Dir[1] = 8;
    Order[3] = 2;
    Order_Timer[3] = 1;
    Order_Dir[3] = 8;
    Order[5] = 2;
    Order[6] = 2;
    Order_Timer[5] = 1;
    Order_Timer[6] = 1;
    Order_Dir[5] = 8;
    Order_Dir[6] = 8;
}

/** @brief Per-player select control — dispatch PL_Sel phases if the player is an operator. */
static void Player_Select_Control() {
    void (*PL_Sel_Jmp_Tbl[PL_SEL_JMP_COUNT])() = { PL_Sel_1st, PL_Sel_2nd, PL_Sel_3rd, PL_Sel_4th, PL_Sel_5th };

    if (plw[ID2].wu.pl_operator != 0) {
        if (SP_No[ID2][1] < PL_SEL_JMP_COUNT) {
            PL_Sel_Jmp_Tbl[SP_No[ID2][1]]();
        }
    }
}

/** @brief PL_Sel phase 1 — init cursor state, spawn D8 effects, play voice; skip if already complete. */
static void PL_Sel_1st() {
    s16 ret;
    s16 ret2;

    if (Sel_PL_Complete[ID2] == -0x8000) {
        SP_No[ID2][1] = 2;
        Push_LDREQ_Queue_Player(ID2, My_char[ID2]);
        ret = check_use_all_SA();
        ret2 = check_without_SA();
        ret |= ret2;

        if (ret != 0) {
            return;
        }

        if (My_char[ID2] == 0) {
            return;
        }

        Sel_Arts_Complete[ID2] = 0;
        Setup_Plates(ID2, 0x55);
        effect_50_init(ID2, 1, 0);
        effect_50_init(ID2, 1, 1);
        effect_50_init(ID2, 2, 0);
        effect_50_init(ID2, 2, 1);

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (!Debug_w[DEBUG_MY_CHAR_PL2]) {
            return;
        }

        My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        return;
    }

    SP_No[ID2][1]++;
}

/** @brief PL_Sel phase 2 — handle character confirmation via loading and SA-availability checks. */
static void PL_Sel_2nd() {
    s16 ret;
    s16 ret2;

    switch (SP_No[ID2][3]) {
    case 0:
        if (!Sel_PL_Complete[ID2]) {
            break;
        }

        ret = check_use_all_SA();
        ret2 = check_without_SA();
        ret |= ret2;

        if (ret != 0 || My_char[ID2] == 0) {
            SP_No[ID2][3]++;
            Cursor_Timer[ID2] = 40;
            Go_Away_Red_Lines();

            if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
                S_No[3] = 1;
                break;
            }

            break;
        }

        SP_No[ID2][1]++;
        Setup_Plates(ID2, 1);
        effect_50_init(ID2, 1, 0);
        effect_50_init(ID2, 1, 1);
        effect_50_init(ID2, 2, 0);
        effect_50_init(ID2, 2, 1);
        break;

    case 1:
        if ((Cursor_Timer[ID2] -= 1) != 0) {
            break;
        }

        Sel_Arts_Complete[ID2] = -1;
        SP_No[ID2][1]++;
        SP_No[ID2][3] = 0;
        Setup_ID();

        if (Used_char[ID2] != My_char[ID2]) {
            Last_Player_id = ID2;
        }

        Used_char[ID2] = My_char[ID2];
        break;
    }
}

/** @brief PL_Sel phase 3 — wait for arts completion before advancing. */
static void PL_Sel_3rd() {
    if (Sel_Arts_Complete[ID2] < 0) {
        SP_No[ID2][1]++;
    }
}

/** @brief PL_Sel phase 4 — no-op placeholder. */
static void PL_Sel_4th() {}

/** @brief PL_Sel phase 5 — no-op placeholder. */
static void PL_Sel_5th() {}

/** @brief Spawn the 3 super-art selection plates for the given player. */
static void Setup_Plates(s8 PL_id, s16 Time) {
    Move_Super_Arts[PL_id] = 3;
    Select_Arts[PL_id] = 3;
    effect_79_init(PL_id, 0, Arts_Y_Data[Super_Arts[PL_id]][0], Time, 2);
    effect_79_init(PL_id, 1, Arts_Y_Data[Super_Arts[PL_id]][1], Time, 2);
    effect_79_init(PL_id, 2, Arts_Y_Data[Super_Arts[PL_id]][2], Time, 2);
}

/** @brief Per-player character-select state machine dispatcher. */
static void Sel_PL() {
    void (*Sel_PL_Jmp_Tbl[SEL_PL_JMP_COUNT])() = { Sel_PL_1st, Sel_PL_2nd, Sel_PL_3rd,
                                                   Sel_PL_4th, Sel_PL_5th, Sel_PL_6th };

    if (plw[ID].wu.pl_operator != 0) {
        if (SP_No[ID][0] < SEL_PL_JMP_COUNT) {
            Sel_PL_Jmp_Tbl[SP_No[ID][0]]();
        }
    }
}

/** @brief Sel_PL phase 1 — init cursor/auto-repeat state, spawn D8/voice, set Select_Start. */
static void Sel_PL_1st() {
    u16 Rnd;

    if (Exit_No) {
        return;
    }

    SP_No[ID][0]++;
    Stop_Cursor[ID] = 1;
    Auto_No[ID] = 0;
    Auto_Index[ID] = 0;
    Auto_Cursor[ID] = 0;
    Moving_Plate[ID] = 0;
    Moving_Plate_Counter[ID] = 0;
    Select_Start[ID] = 2;
    Select_Arts[ID] = -1;

    if (ID == 1) {
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

    if (Sel_PL_Complete[ID]) {
        SP_No[ID][0] = 3;
        Select_Start[ID] = 3;
        Select_Arts[ID] = 3;
        Stop_Cursor[ID] = 1;
        paring_ctr_vs[0][ID] = 0;
        paring_ctr_vs[1][ID] = 0;
        return;
    }

    Arts_Y[ID] = Super_Arts[ID] = Last_Super_Arts[ID];
}

/** @brief Sel_PL phase 2 — wait for Select_Start countdown, then enable cursor input. */
static void Sel_PL_2nd() {
    if (Select_Start[ID] > 0) {
        return;
    }

    SP_No[ID][0]++;
    Stop_Cursor[ID] = 0;
    Deley_Shot_No[ID] = 0;
    Cursor_Timer[ID] = 1;

    if (Demo_Flag == 0) {
        Demo_Timer[ID] = 0;
        Demo_Ptr[ID] = (u16*)Sel_PL_Data_Address[Select_Demo_Index];
    }
}

/** @brief Sel_PL phase 3 — handle cursor+button input per-player (or demo), commit character on press. */
static void Sel_PL_3rd() {
    if (Stop_Cursor[ID] != 0 || Face_Move != 0) {
        return;
    }

    if (Demo_Flag == 0) {
        if (ID) {
            Sel_PL_Sub(1, Check_Demo_Data(1));
        } else {
            Sel_PL_Sub(0, Check_Demo_Data(0));
        }
    } else if (ID) {
        Sel_PL_Sub(1, Deley_Shot_Sub(1));
    } else {
        Sel_PL_Sub(0, Deley_Shot_Sub(0));
    }

    if (Sel_PL_Complete[ID] >= 0) {
        return;
    }

    if (Debug_w[DEBUG_MY_CHAR_PL1]) {
        My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
    }

    if (Debug_w[DEBUG_MY_CHAR_PL2]) {
        My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
    }

    Push_LDREQ_Queue_Player(ID, My_char[ID]);
    SP_No[ID][0]++;
    Stop_Cursor[ID] = 1;
    Auto_No[ID] = 0;
    paring_ctr_vs[0][ID] = 0;
    paring_ctr_vs[1][ID] = 0;

    if (Continue_Coin[ID] == 0) {
        Clear_Break_Com(ID);
        grade_check_work_1st_init(ID, 0);
        grade_check_work_1st_init(ID, 1);
        Initialize_EM_Candidate(ID);
        Best_Grade[ID] = -1;
        Result_Timer[ID] = 180;
        Request_Disp_Rank[ID][0] = -1;
        Request_Disp_Rank[ID][1] = -1;
        Request_Disp_Rank[ID][2] = -1;
        Request_Disp_Rank[ID][3] = -1;
        return;
    }

    Check_Same_CPU(ID);
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

    switch (Deley_Shot_No[PL_id]) {
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
        Deley_Shot_No[PL_id] = 1;
        Deley_Shot_Timer[PL_id] = 3;

        break;

    case 1:
        Color7[PL_id] |= sw;

        if ((Deley_Shot_Timer[PL_id] -= 1) == 0) {
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
    if (!Select_Arts[ID]) {
        SP_No[ID][0]++;
        Stop_Cursor[ID] = 0;
    }
}

/** @brief Sel_PL phase 5 — super-art selection input; check boss on completion. */
static void Sel_PL_5th() {
    if (Stop_Cursor[ID] != 0 || Face_Move != 0) {
        return;
    }

    if (Demo_Flag == 0) {
        if (ID) {
            Sel_Arts_Sub(1, Check_Demo_Data(1), 0);
        } else {
            Sel_Arts_Sub(0, Check_Demo_Data(0), 0);
        }
    } else if (ID) {
        Sel_Arts_Sub(1, ~p2sw_1 & p2sw_0, p2sw_0);
    } else {
        Sel_Arts_Sub(0, ~p1sw_1 & p1sw_0, p1sw_0);
    }

    if (!Sel_Arts_Complete[ID]) {
        return;
    }

    SP_No[ID][0]++;

    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
        S_No[3] = 1;
    }

    if (plw[0].wu.pl_operator == 0 || plw[1].wu.pl_operator == 0) {
        Check_Boss(ID);
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
    Cursor_Move[PL_id] = 0;

    if (Sel_PL_Complete[PL_id]) {
        return;
    }

    if (Time_Over) {
        sw = SWK_WEST;
    }

    if (sw == 0) {
        Auto_Repeat_Sub(PL_id);
    }

    if ((Cursor_Timer[PL_id] -= 1) == 0) {
        Cursor_Timer[PL_id] = 1;

        if (sw & SWK_RIGHT) {
            Cursor_Timer[PL_id] = 5;
            Sel_PL_Sub_CR(PL_id);
        } else if (sw & SWK_LEFT) {
            Cursor_Timer[PL_id] = 5;
            Sel_PL_Sub_CL(PL_id);
        } else if (sw & SWK_UP) {
            Cursor_Timer[PL_id] = 5;
            Sel_PL_Sub_CU(PL_id);
        } else if (sw & SWK_DOWN) {
            Cursor_Timer[PL_id] = 5;
            Sel_PL_Sub_CD(PL_id);
        }
    }

    if (Cursor_Move[PL_id]) {
        Sound_SE(ID + 96);
    }

    if (!(sw & SWK_ATTACKS)) {
        return;
    }

    Sel_PL_Complete[PL_id] = 1;
    My_char[PL_id] = ID_of_Face[Cursor_Y[PL_id]][Cursor_X[PL_id]];

    if (Last_My_char2[PL_id] != My_char[PL_id]) {
        Arts_Y[ID] = Super_Arts[ID] = Last_Super_Arts[ID] = 0;
        Introduce_Boss[ID][0] = 0;
    }

    Last_My_char2[PL_id] = My_char[PL_id];
    Last_Selected_ID = PL_id;
    Order[1] = 2;
    Order_Timer[1] = 1;
    Order_Dir[1] = 8;

    if (Select_Status[0] != 3) {
        Order[2] = 1;
        Order_Timer[2] = 10;
        Order_Dir[2] = 4;
    }

    Sound_SE(ID + 98);
    Sound_SE(*Free_Ptr[PL_id]++);
    Setup_PL_Color(PL_id, sw);
    Correct_Control_Time(PL_id);
}

/** @brief Move cursor right on the face grid, wrapping rows. */
static void Sel_PL_Sub_CR(s16 PL_id) {
    if (Cursor_X[PL_id] == 7) {
        return;
    }

    Cursor_Move[PL_id] = 1;

    do {
        Cursor_Y[PL_id]++;

        switch (Cursor_X[PL_id]) {
        case 6:
            if (Cursor_Y[PL_id] > 1) {
                Cursor_Y[PL_id] = 1;
                Cursor_X[PL_id] = 0;
            }

            break;

        default:
            if (Cursor_Y[PL_id] > 2) {
                Cursor_Y[PL_id] = 0;
                Cursor_X[PL_id]++;
            }

            break;
        }
    } while (!permission_player[Present_Mode].ok[Face_Cursor_Data[Cursor_Y[PL_id]][Cursor_X[PL_id]]]);
}

/** @brief Move cursor left on the face grid, wrapping rows. */
static void Sel_PL_Sub_CL(s16 PL_id) {
    if (Cursor_X[PL_id] == 7) {
        return;
    }

    Cursor_Move[PL_id] = 1;

    do {
        Cursor_Y[PL_id]--;

        switch (Cursor_X[PL_id]) {
        case 0:
            if (Cursor_Y[PL_id] <= 0) {
                Cursor_Y[PL_id] = 1;
                Cursor_X[PL_id] = 6;
            }
            break;

        case 1:
            if (Cursor_Y[PL_id] < 0) {
                Cursor_Y[PL_id] = 2;
                Cursor_X[PL_id] = 0;
            }
            break;

        default:
            if (Cursor_Y[PL_id] < 0) {
                Cursor_Y[PL_id] = 2;
                Cursor_X[PL_id]--;
            }

            break;
        }
    } while (!permission_player[Present_Mode].ok[Face_Cursor_Data[Cursor_Y[PL_id]][Cursor_X[PL_id]]]);
}

/** @brief Move cursor up on the face grid, wrapping columns. */
static void Sel_PL_Sub_CU(s16 PL_id) {
    Cursor_Move[PL_id] = 1;

    do {
        Cursor_X[PL_id]++;

        switch (Cursor_Y[PL_id]) {
        case 0:
            if (Cursor_X[PL_id] > 6) {
                Cursor_X[PL_id] = 1;
            }

            break;

        case 1:
            if (Cursor_X[PL_id] > 7) {
                Cursor_X[PL_id] = 0;
            }

            break;

        default:
            if (Cursor_X[PL_id] > 5) {
                Cursor_X[PL_id] = 0;
            }

            break;
        }
    } while (!permission_player[Present_Mode].ok[Face_Cursor_Data[Cursor_Y[PL_id]][Cursor_X[PL_id]]]);
}

/** @brief Move cursor down on the face grid, wrapping columns. */
static void Sel_PL_Sub_CD(s16 PL_id) {
    Cursor_Move[PL_id] = 1;

    do {
        Cursor_X[PL_id]--;

        switch (Cursor_Y[PL_id]) {
        case 0:
            if (Cursor_X[PL_id] <= 0) {
                Cursor_X[PL_id] = 6;
            }

            break;

        case 1:
            if (Cursor_X[PL_id] < 0) {
                Cursor_X[PL_id] = 7;
            }

            break;

        default:
            if (Cursor_X[PL_id] < 0) {
                Cursor_X[PL_id] = 5;
            }

            break;
        }
    } while (!permission_player[Present_Mode].ok[Face_Cursor_Data[Cursor_Y[PL_id]][Cursor_X[PL_id]]]);
}

/** @brief Auto-repeat logic for held directions on the character grid (accelerating repeat). */
static void Auto_Repeat_Sub(s16 PL_id) {
    u16 sw;

    if (Demo_Flag == 0) {
        return;
    }

    if (Cursor_Move[PL_id]) {
        return;
    }

    if (PL_id == 0) {
        sw = p1sw_0;
    } else {
        sw = p2sw_0;
    }

    sw = Disposal_Of_Diagonal(sw);

    switch (Auto_No[PL_id]) {
    case 0:
        if (sw & SWK_RIGHT) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 8;
            Auto_Timer[PL_id] = Repeat_Time_Data[0];
            Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_LEFT) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 4;
            Auto_Timer[PL_id] = Repeat_Time_Data[0];
            Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_UP) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 1;
            Auto_Timer[PL_id] = Repeat_Time_Data[0];
            Auto_Index[PL_id] = 1;
            break;
        }

        if (sw & SWK_DOWN) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 2;
            Auto_Timer[PL_id] = Repeat_Time_Data[0];
            Auto_Index[PL_id] = 1;
        }

        break;

    case 1:
        if (sw != Auto_Cursor[PL_id]) {
            Auto_No[PL_id] = 0;
            break;
        }

        if (Auto_Timer[PL_id] -= 1) {
            break;
        }

        Auto_Timer[PL_id] = Repeat_Time_Data[Auto_Index[PL_id]];
        Auto_Index[PL_id]++;

        if ((Auto_Index[PL_id]) > 2) {
            Auto_Index[PL_id] = 2;
        }

        if (sw & SWK_RIGHT) {
            Sel_PL_Sub_CR(PL_id);
        }

        if (sw & SWK_LEFT) {
            Sel_PL_Sub_CL(PL_id);
        }

        if (sw & SWK_UP) {
            Sel_PL_Sub_CU(PL_id);
        }

        if (sw & SWK_DOWN) {
            Sel_PL_Sub_CD(PL_id);
        }

        break;
    }
}

/** @brief Auto-repeat logic for the super-art plate (up/down only, instant repeat). */
static u16 Auto_Repeat_Sub_Wife(s16 PL_id) {
    u16 sw;

    if (Cursor_Move[PL_id] || Demo_Flag == 0) {
        return 0;
    }

    if (PL_id == 0) {
        sw = p1sw_0;
    } else {
        sw = p2sw_0;
    }

    switch (Auto_No[PL_id]) {
    case 0:
        if (sw & SWK_UP) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 1;
            Auto_Timer[PL_id] = Repeat_Time_Data_Wife[0];
            Auto_Index[PL_id] = 1;
        } else if (sw & SWK_DOWN) {
            Auto_No[PL_id] = 1;
            Auto_Cursor[PL_id] = 2;
            Auto_Timer[PL_id] = Repeat_Time_Data_Wife[0];
            Auto_Index[PL_id] = 1;
        }

        break;

    case 1:
        sw &= Auto_Cursor[PL_id];

        if (sw) {
            if (Auto_Timer[PL_id] -= 1) {
                break;
            }

            Auto_Timer[PL_id] = Repeat_Time_Data_Wife[Auto_Index[PL_id]++];

            if (Auto_Index[PL_id] > 2) {
                Auto_Index[PL_id] = 2;
            }

            if (sw & SWK_UP) {
                return SWK_UP;
            }

            if (sw & SWK_DOWN) {
                return SWK_DOWN;
            }

            break;
        }

        Auto_No[PL_id] = 0;
        break;
    }

    return 0;
}

/** @brief Super-art selector — move art plate up/down, confirm on attack. */
static void Sel_Arts_Sub(s16 PL_id, u16 sw, u16 /* unused */) {
    u16 lever_sw;

    if (Sel_Arts_Complete[PL_id]) {
        return;
    }

    if (Moving_Plate_Counter[PL_id]) {
        return;
    }

    if (Moving_Plate[PL_id]) {
        return;
    }

    if (Plate_Disposal_No[PL_id][0] != 0 || Plate_Disposal_No[PL_id][1] != 0 || Plate_Disposal_No[PL_id][2] != 0) {
        return;
    }

    if (Time_Over) {
        sw = SWK_WEST;
    }

    lever_sw = sw & SWK_DIRECTIONS;

    if (lever_sw == 0) {
        sw |= Auto_Repeat_Sub_Wife(PL_id);
    }

    if (sw & SWK_DOWN) {
        Sound_SE(ID + 96);
        Moving_Plate[PL_id] = 2;
        Moving_Plate_Counter[PL_id] = 3;
        OK_Priority[PL_id] = 0;

        if ((Arts_Y[PL_id] += 1) > 2) {
            Arts_Y[PL_id] = 0;
        }
    }

    if (sw & SWK_UP) {
        Sound_SE(ID + 96);
        Moving_Plate[PL_id] = 1;
        Moving_Plate_Counter[PL_id] = 3;
        OK_Priority[PL_id] = 0;

        if ((Arts_Y[PL_id] -= 1) < 0) {
            Arts_Y[PL_id] = 2;
        }
    }

    if (sw & SWK_ATTACKS) {
        Stop_Cursor[ID] = 1;
        Slide_Type = PL_id;
        Sel_Arts_Complete[PL_id] = 1;
        Last_Super_Arts[PL_id] = Super_Arts[PL_id] = Arts_Y[PL_id];
        Sound_SE(ID + 98);
        Sound_SE(*Free_Ptr[PL_id]++);
        Setup_ID();

        if (Used_char[PL_id] != My_char[PL_id]) {
            Last_Player_id = PL_id;
        }

        Used_char[PL_id] = My_char[PL_id];
    }
}

/** @brief Exit state machine dispatcher — run the active exit/handicap phase. */
static void Check_Exit() {
    void (*Sel_Exit_Tbl[10])() = { Exit_1st, Exit_2nd, Exit_3rd,     Exit_4th,     Exit_5th,
                                   Exit_6th, Exit_7th, Handicap_1st, Handicap_2nd, Handicap_3rd };
    Sel_Exit_Tbl[Exit_No]();
}

/** @brief Exit phase 1 — wait until all operators have arts complete, dismiss red lines, route to handicap or normal
 * exit. */
static void Exit_1st() {
    if (plw[0].wu.pl_operator != 0 && Sel_Arts_Complete[0] >= 0) {
        return;
    }

    if (plw[1].wu.pl_operator != 0 && Sel_Arts_Complete[1] >= 0) {
        return;
    }

    Go_Away_Red_Lines();
    Order[4] = 4;
    Order_Timer[4] = 1;
    Order[7] = 4;
    Order[8] = 4;
    Order_Timer[7] = 1;
    Order_Timer[8] = 1;
    Setup_Training_Difficulty();

    if (Mode_Type == MODE_VERSUS && save_w[Present_Mode].Handicap != 0) {
        Exit_No = 7;
    } else {
        Exit_No++;
    }

    if (Demo_Flag) {
        E_No[0] = 3;
        E_No[1] = 0;
        E_No[2] = 0;
        E_No[3] = 0;
    }
}

/** @brief Exit phase 2 — determine battle country/stage, queue BG load, start exit timer. */
static void Exit_2nd() {
    s16 xx;

    S_No[1] = 0;

    if (Select_Status[0] == 3) {
        Exit_No = 3;
        Last_My_char[0] = My_char[0];
        Last_My_char[1] = My_char[1];
        Battle_Country = Setup_Battle_Country();
        bg_w.stage = Battle_Country;
        bg_w.area = 0;

        if (Debug_w[DEBUG_STAGE_SELECT]) {
            Battle_Country = bg_w.stage = Debug_w[DEBUG_STAGE_SELECT] - 1;
        }

        Push_LDREQ_Queue_BG(bg_w.stage + 0);
        return;
    }

    if (Scene_Cut) {
        Exit_Timer = 1;
    } else {
        Exit_Timer = 60;
    }

    Exit_No++;
    Last_My_char[Player_id] = My_char[Player_id];
    Time_Stop = 2;

    for (xx = 0; xx < 4; xx++) {
        SC_No[xx] = 0;
    }
}

/** @brief Exit phase 3 — run Select_CPU_First, then set EM_Rank for the upcoming fight. */
static void Exit_3rd() {
    if (!Select_CPU_First()) {
        return;
    }

    Exit_No++;
    S_No[1] = 0;
    Suicide[3] = 1;

    if (VS_Index[Player_id] >= 9) {
        EM_Rank = 1;
        return;
    }

    EM_Rank = 0;
}

/** @brief Exit phase 4 — fade in, start BGM, spawn VS-screen objects. */
static void Exit_4th() {
    FadeInit();
    FadeIn(0, 4, 8);
    Exit_No++;
    Forbid_Break = 0;
    Suicide[0] = 1;
    Menu_Suicide[0] = 1;
    bgPalCodeOffset[0] = 144;
    BGM_Request(51);
    Exit_Timer = 240;
    effect_58_init(17, 2, 0);

    if (Select_Status[0] != 3) {
        effect_K6_init(0, 35, 35, 2);
        Order[35] = 3;
        Order_Timer[35] = 1;
        effect_K6_init(1, 36, 35, 2);
        Order[36] = 3;
        Order_Timer[36] = 1;
        effect_39_init(0, 17, My_char[0], 2, 0);
        Order[17] = 3;
        Order_Timer[17] = 1;
        effect_39_init(1, 18, My_char[1], 2, 0);
        Order[18] = 3;
        Order_Timer[18] = 1;
        effect_K6_init(0, 29, 29, 2);
        Order[29] = 3;
        Order_Timer[29] = 1;
        effect_K6_init(1, 30, 29, 2);
        Order[30] = 3;
        Order_Timer[30] = 1;
    } else if (Win_Record[Champion]) {
        effect_76_init(72);
        Order[72] = 3;
        Order_Timer[72] = 1;
        effect_76_init(73);
        Order[73] = 3;
        Order_Timer[73] = 1;
    }

    effect_43_init(2, 2);
    Order[42] = 2;
    Order_Timer[42] = 1;
    Order_Dir[42] = 5;
}

/** @brief Exit phase 5 — count down while fading, then advance. */
static void Exit_5th() {
    Exit_Timer--;

    if (!FadeIn(0, 4, 8)) {
        return;
    }

    Exit_No++;

    if (Exit_Timer < 0) {
        Exit_Timer = 1;
    }
}

/** @brief Exit phase 6 — wait for all loads, then count down exit timer and init omop. */
static void Exit_6th() {
    if (!Check_PL_Load()) {
        return;
    }

    if (!Check_LDREQ_Queue_BG(bg_w.stage + 0)) {
        return;
    }

    // We shouldn't skip VS screen in network mode, because that can lead to IO race conditions
    if (Scene_Cut && (Mode_Type != MODE_NETWORK)) {
        Exit_Timer = 1;
    }

    if ((Exit_Timer -= 1) == 0) {
        Exit_No++;
        init_omop();
    }
}

/** @brief Exit phase 7 — set final battle stage and signal exit. */
static void Exit_7th() {
    bg_w.stage = Battle_Country;
    bg_w.area = 0;
    SEL_PL_X = 1;

    if (use_rmlui && rmlui_screen_select)
        rmlui_char_select_hide();
}

/** @brief Handicap phase 1 — spawn handicap menu UI (vital bars, stage selector, labels). */
static void Handicap_1st() {
    Exit_No++;
    Decide_Stage = 0;
    Menu_Common_Init();
    Setup_Training_Difficulty();
    SP_No[0][2] = 0;
    SP_No[1][2] = 0;
    effect_66_init(138, 31, 0, 2, -1, -1, -0x7FF8);
    Order[138] = 3;
    Order_Timer[138] = 1;
    effect_66_init(139, 35, 0, 2, 71, 20, 0);
    Order[139] = 5;
    effect_66_init(140, 36, 0, 2, 71, 20, 0);
    Order[140] = 5;
    effect_66_init(141, 37, 0, 2, 71, 21, 0);
    Order[141] = 5;
    effect_66_init(91, 28, 0, 2, 71, 15, 0);
    Order[91] = 3;
    Order_Timer[91] = 1;
    effect_66_init(92, 29, 0, 2, 71, 16, 0);
    Order[92] = 3;
    Order_Timer[92] = 1;
    effect_66_init(93, 30, 0, 2, 71, 17, 0);
    Order[93] = 3;
    Order_Timer[93] = 1;
    effect_66_init(120, 32, 0, 2, 71, 18, 0);
    Order[120] = 2;
    Order_Timer[120] = 1;
    effect_66_init(121, 33, 0, 2, 71, 18, 0);
    Order[121] = 2;
    Order_Timer[121] = 1;
    effect_66_init(122, 34, 0, 2, 71, 19, 0);
    Order[122] = 5;
    effect_99_init(0, 0, 0x7047, 0, 0, 0);
    effect_99_init(1, 0, 0x7047, 1, 1, 0);
    effect_99_init(255, 1, 0x7047, 2, 2, 0);
    effect_99_init(255, 1, 0x70A7, 3, 3, 0);
    effect_99_init(255, 1, 0x70A7, 4, 4, 0);
}

/** @brief Handicap phase 2 — run per-player handicap control. */
static void Handicap_2nd() {
    ID2 = 0;
    Handicap_Control();
    ID2 = 1;
    Handicap_Control();
}

/** @brief Handicap phase 3 — fade BGM and return to exit phase 1 when timer expires. */
static void Handicap_3rd() {
    if (S_Timer == 9) {
        SsBgmFadeOut(0x1000);
    }

    if ((S_Timer -= 1) == 0) {
        Exit_No = 1;
    }
}

/** @brief Per-player handicap sub-state machine dispatcher. */
static void Handicap_Control() {
    void (*Handicap_Jmp_Tbl[HANDICAP_JMP_COUNT])() = { Handicap_1, Handicap_2, Handicap_3, Handicap_4 };
    if (SP_No[ID2][2] < HANDICAP_JMP_COUNT) {
        Handicap_Jmp_Tbl[SP_No[ID2][2]]();
    }
}

/** @brief Handicap sub 1 — vital-bar selection for this player; advance when confirmed. */
static void Handicap_1() {
    Handicap_Vital_Select(ID2);

    if (!(IO_Result & 0x100)) {
        return;
    }

    SE_selected();
    Order[ID2 + 120] = 5;
    Order[ID2 + 139] = 6;
    Order_Timer[ID2 + 139] = 1;

    if (SP_No[ID2 ^ 1][2] == 2) {
        SP_No[ID2][2] = 1;
        return;
    }

    SP_No[ID2][2] = 2;

    if (SP_No[ID2 ^ 1][2] < 3) {
        Order[122] = 2;
        Order_Timer[122] = 1;
    }
}

/** @brief Handicap sub 2 — wait or go back if other player cancelled; otherwise proceed to stage. */
static void Handicap_2() {
    u16 sw;

    if (ID2 == 0) {
        sw = ~p1sw_1 & p1sw_0;
    } else {
        sw = ~p2sw_1 & p2sw_0;
    }

    if (sw & SWK_EAST && Decide_Stage == 0) {
        SP_No[ID2][2] = 0;
        SE_selected();
        Order[ID2 + 139] = 5;
        Order[ID2 + 120] = 2;
        Order_Timer[ID2 + 120] = 1;
        return;
    }

    if (SP_No[ID2 ^ 1][2] == 0) {
        SP_No[ID2][2] = 2;
        Order[122] = 2;
        Order_Timer[122] = 1;
    }
}

u8 hc3alphaadd = { 1 };

/** @brief Handicap sub 3 — stage selection with flashing cursor; back or confirm. */
static void Handicap_3() {
    Handicap_Stage_Select(ID2);

    if (IO_Result & 0x100) {
        SP_No[ID2][2]++;
        SE_selected();
        Order[141] = 6;
        Order_Timer[141] = 1;
        Order[122] = 5;
        Decide_Stage = 1;
        return;
    }

    if (IO_Result & 0x200 && Decide_Stage == 0) {
        SP_No[ID2][2] = 0;
        SE_selected();
        Order[122] = 5;
        Order[ID2 + 139] = 5;
        Order[ID2 + 120] = 2;
        Order_Timer[ID2 + 120] = 1;
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

    if (ID2) {
        f32 dmypos[8] = { 296.0f, 90.0f, 296.0f, 98.0f, 284.0f, 90.0f, 268.0f, 112.0f };
        Renderer_Queue2DPrimitive(dmypos, PrioBase[2], (hc3alpha + 48) * 0x1000000 | 0xFFFFFF, 0);
    } else {
        f32 dmypos[8] = { 88.0f, 90.0f, 88.0f, 98.0f, 100.0f, 90.0f, 116.0f, 112.0f };
        Renderer_Queue2DPrimitive(dmypos, PrioBase[2], (hc3alpha + 48) * 0x1000000 | 0xFFFFFF, 0);
    }
}

/** @brief Handicap sub 4 — wait for both players to finish, then advance to exit timer. */
static void Handicap_4() {
    if (SP_No[0][2] > 0 && SP_No[1][2] > 0) {
        Exit_No = 9;
        S_Timer = 60;
    }
}

/** @brief Read pad input and process vital-bar handicap lever movement. */
static void Handicap_Vital_Select(s16 PL_id) {
    Setup_Pad_or_Stick();
    IO_Result = Check_Menu_Lever(PL_id, 0);
    Handicap_Vital_Move_Sub(IO_Result, PL_id);
}

/** @brief Move the vital-bar handicap slider left/right (direction swapped for 2P). */
static u16 Handicap_Vital_Move_Sub(u16 sw, s16 PL_id) {
    if (PL_id == 0) {
        switch (sw) {
        case SWK_LEFT:
            if ((Vital_Handicap[Present_Mode][PL_id] += 1) > 7) {
                Vital_Handicap[Present_Mode][PL_id] = 7;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_LEFT;

        case SWK_RIGHT:
            if ((Vital_Handicap[Present_Mode][PL_id] -= 1) < 0) {
                Vital_Handicap[Present_Mode][PL_id] = 0;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_RIGHT;
        }
    } else {
        switch (sw) {
        case SWK_LEFT:
            if ((Vital_Handicap[Present_Mode][PL_id] -= 1) < 0) {
                Vital_Handicap[Present_Mode][PL_id] = 0;
            } else {
                SE_dir_cursor_move();
            }

            return SWK_LEFT;

        case SWK_RIGHT:
            if ((Vital_Handicap[Present_Mode][PL_id] += 1) > 7) {
                Vital_Handicap[Present_Mode][PL_id] = 7;
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
    IO_Result = Check_Menu_Lever(PL_id, 0);
    Handicap_Stage_Move_Sub(IO_Result);
}

/** @brief Move the stage selector left/right, wrapping and skipping stage 17. */
static void Handicap_Stage_Move_Sub(u16 sw) {
    switch (sw) {
    case SWK_LEFT:
        if ((VS_Stage -= 1) < 0) {
            VS_Stage = 20;
        }

        if (VS_Stage == 17) {
            VS_Stage = 16;
        }

        SE_dir_cursor_move();
        break;

    case SWK_RIGHT:
        if ((VS_Stage += 1) > 20) {
            VS_Stage = 0;
        }

        if (VS_Stage == 17) {
            VS_Stage = 18;
        }

        SE_dir_cursor_move();
        break;
    }
}

/** @brief Reduce the select timer based on the player’s continue count. */
static void Correct_Control_Time(s16 PL_id) {
    u8 xx;
    u8 zz;

    if (Play_Type == 1) {
        return;
    }

    if (Stage_Continue[PL_id] == 0) {
        return;
    }

    xx = Stage_Continue[PL_id];

    if (VS_Index[PL_id] >= 9) {
        zz = 1;
    } else {
        zz = 0;
    }

    if (Stage_Continue[PL_id] >= 16) {
        xx = 16;
    } else {
        xx = Stage_Continue[PL_id];
    }

    Control_Time = SC_Personal_Time[PL_id] - Correct_Cont_Time_Data[zz][xx];

    if (Control_Time < 0) {
        Control_Time = 0;
    }

    SC_Personal_Time[PL_id] = Control_Time;
}

/** @brief If the player is at boss stage and hasn’t seen the intro, force max time and flag Break_Into_CPU. */
static s32 Check_Boss(s16 PL_id) {
    if (VS_Index[Player_id] >= 9 && Introduce_Boss[Player_id][1] == 0) {
        Control_Time = Limit_Time;
        SC_Personal_Time[PL_id] = Control_Time;
        return Break_Into_CPU = 1;
    }

    return Break_Into_CPU = 0;
}

/** @brief Pick the battle stage from VS_Stage, random, or character match-up. */
static u8 Setup_Battle_Country() {
    s16 Rnd32;

    if (Mode_Type == MODE_VERSUS) {
        if (VS_Stage == 20) {
            Rnd32 = random_32();
            return Random_Stage_Data[1][Rnd32];
        }

        return VS_Stage;
    }

    if (My_char[0] == 17 && My_char[1] == 17) {
        Rnd32 = random_32();
        return Random_Stage_Data[0][Rnd32];
    }

    if (My_char[New_Challenger] == 17) {
        return My_char[Champion];
    }

    return My_char[New_Challenger];
}
