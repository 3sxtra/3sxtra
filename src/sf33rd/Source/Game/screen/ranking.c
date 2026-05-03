/**
 * @file ranking.c
 * Manages the display of the high-score ranking tables
 */

#include "sf33rd/Source/Game/screen/ranking.h"
#include "game_state.h"
#include "common.h"
#include "main.h"
#include "port/menu_screen.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo_02_parry_tutorial.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_67_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_76_quake.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/rendering/memory_texture_control.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/work_sys.h"

// sbss
RANK_DATA Present_Data[2];

// bss
RANK_DATA Ranking_Data[20];

// forward declaration
const RANK_DATA Score_Ranking_Table[20];

#include "port/menu_task.h"

/** @brief Ranking_Init sub-states via g_state.demo_phase[1]. */
enum Ranking00State {
    R00_INIT_TEXCACHE = 0,
    R00_LAYOUT_ENTRIES = 1,
    R00_WAIT_FLASH = 2,
    R00_TOGGLE_FLASH = 3,
    R00_DELAY_CLEAR = 4,
    R00_DISPLAY_EXIT = 5,
    R00_STATE_COUNT
};

/** @brief Ranking_Display sub-states via g_state.demo_phase[1]. */
enum Ranking01State {
    R01_INIT_SCREEN = 0,
    R01_LAYOUT_ENTRIES = 1,
    R01_WAIT_FLASH = 2, /* shared — calls Ranking_00_3rd */
    R01_COUNTDOWN = 3,
    R01_FADE_EXIT = 4,
    R01_STATE_COUNT
};

/** @brief Ranking_01_5th fade sub-states via g_state.demo_phase[2]. */
enum Ranking01FadeState {
    R01F_FADEOUT_INIT = 0,
    R01F_SCREEN_WAIT = 1,
    R01F_DONE = 2,
};

/** @brief Main ranking dispatcher — thin wrapper around MenuScreen registry. */
s32 Ranking() {
    struct _TASK* tp = MenuTask_GetTaskPtr();

    if (!MenuScreen_IsActive()) {
        g_state.Ranking_X = 0;
        MenuScreen_Goto(MENU_SCREEN_RANKING);
    }

    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        g_state.Ranking_X = 1;
    }

    BG_Draw_System();
    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        g_state.Ranking_X = 0;
    }
    return g_state.Ranking_X;
}

/** @brief Ranking table 00 dispatcher — used when arriving from gameplay (post-credit entry). */
void Ranking_ScoreEntry() {
    g_state.Ranking_X = 0;

    switch (g_state.demo_phase[1]) {
    case R00_INIT_TEXCACHE:
        Ranking_00_1st();
        break;
    case R00_LAYOUT_ENTRIES:
        Ranking_00_2nd();
        break;
    case R00_WAIT_FLASH:
        Ranking_00_3rd();
        break;
    case R00_TOGGLE_FLASH:
        Ranking_00_4th();
        break;
    case R00_DELAY_CLEAR:
        Ranking_00_5th();
        break;
    case R00_DISPLAY_EXIT:
        Ranking_00_6th();
        break;
    default:
        break;
    }
}

/** @brief Ranking_Init phase 1 — build tex-cache, init demo type, and prepare BG/effects. */
void Ranking_00_1st() {
    Allocate_Texture_Cache(14);
    g_state.demo_phase[1]++;
    g_state.Demo_Type = 0;
    g_state.Flash_Sign[0] = 1;
    Ranking_Sub();
}

/** @brief Ranking_Init phase 2 — layout all rank entries (names, scores, faces, grades). */
void Ranking_00_2nd() {
    s16 Char_Index;

    g_state.demo_phase[1]++;
    g_state.demo_timer_global = 1;
    g_state.Rank_X = 0;
    g_state.Flash_Rank_Time = 0;
    g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos - 104;
    g_state.Rank_Pos_Y = g_state.bg_w.bgw[0].xy[1].disp.pos + 160;

    if (g_state.Rank_Type >= 10) {
        g_state.Order[85] = 3;
        g_state.Order_Timer[85] = 1;
        g_state.Order_Dir[85] = (u8)g_state.Rank_Type;
        effect_76_init(85);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 128, g_state.bg_w.bgw[0].xy[1].disp.pos + 80, 180, 10, 35, 5, 0);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 120, g_state.bg_w.bgw[0].xy[1].disp.pos + 40, 180, 13, 35, 5, 0);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos - 144, g_state.bg_w.bgw[0].xy[1].disp.pos + 68, 180, 7, 30, 5, 0);
        g_state.base_y_pos = 40;
        switch (g_state.Rank_Type) {
        case 10:
            if (Ranking_Data[10].cpu_grade == -1) {
                Char_Index = 0;
            } else {
                Char_Index = Ranking_Data[10].cpu_grade;
            }
            break;
        case 15:
            if (Ranking_Data[15].grade == -1) {
                Char_Index = 0;
            } else {
                Char_Index = Ranking_Data[15].grade;
            }
            break;
        }

        effect_67_init(0,
                       g_state.bg_w.bgw[0].xy[0].disp.pos - 136,
                       g_state.bg_w.bgw[0].xy[1].disp.pos + 60,
                       180,
                       Char_Index,
                       10,
                       6,
                       1);
        g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos - 72;
        g_state.Rank_Pos_Y = g_state.bg_w.bgw[0].xy[1].disp.pos + 120;
        g_state.Rank_Pos_X += 16;
        g_state.Rank_Pos_Y -= 48;
        g_state.Rank = g_state.Rank_Type;
        g_state.Rank_Pos_Y -= 1;
        Setup_Name(5);
        g_state.Rank_Pos_Y += 1;
        g_state.Rank_Pos_X += 16;
        g_state.Rank_Pos_Y += 1;
        g_state.Rank_Pos_Y += 256;
        Setup_Face(5);
        g_state.Rank_Pos_Y -= 256;
        g_state.Rank_Pos_Y -= 1;
        g_state.Rank_Pos_X -= 32;
        g_state.Rank_Pos_Y -= 40;
        if (g_state.Rank_Type == 10) {
            g_state.Rank_Pos_X -= 80;
            Setup_Score(5);
        } else {
            g_state.Rank_Pos_X -= 32;
            Setup_Wins2(5);
        }
    } else {
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos - 143, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 4, 30, 5, 0);
        for (g_state.Rank = g_state.Rank_Type; g_state.Rank < (g_state.Rank_Type + 5); g_state.Rank++) {
            if ((g_state.Present_Rank[0] == (g_state.Rank - g_state.Rank_Type)) ||
                (g_state.Present_Rank[1] == (g_state.Rank - g_state.Rank_Type))) {
                g_state.Flash_Rank_Interval = 1;
            } else {
                g_state.Flash_Rank_Interval = 0;
            }

            Setup_Name(5);
            if (g_state.Rank_Type == 0) {
                Setup_Score(5);
                g_state.Rank_Pos_X += 3;
                Setup_grade(5);
                g_state.Rank_Pos_X -= 3;
            } else {
                Setup_Wins(5);
                Setup_grade(5);
            }

            Setup_Face(5);
            g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos - 104;
            g_state.Rank_Pos_Y -= 32;
            g_state.Rank_X = 0;
            g_state.Flash_Rank_Time = 0;
        }

        if ((g_state.Present_Rank[0] < 5) || (g_state.Present_Rank[1] < 5)) {
            g_state.demo_phase[1] += 1;
        } else {
            g_state.demo_phase[1] = R00_DELAY_CLEAR;
        }
    }

    switch (g_state.Rank_Type) {
    case 0:
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 0, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 0, 10, 5, 0);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 168, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 5, 20, 5, 0);
        return;
    case 5:
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 0, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 2, 10, 5, 0);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 168, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 5, 20, 5, 0);
        return;
    case 10:
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 1, 10, 5, 0);
        return;
    case 15:
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 3, 10, 5, 0);
    }
}

/** @brief Ranking_Init phase 3 — wait for the g_state.Flash_Sign ready flag. */
void Ranking_00_3rd() {
    if (g_state.Flash_Sign[0] == 1) {
        g_state.demo_phase[1]++;
        g_state.demo_timer_global = 1;
    }
}

/** @brief Ranking_Init phase 4 — countdown timer, then toggle flash sign flags. */
void Ranking_00_4th() {
    if (!(--g_state.demo_timer_global)) {
        g_state.demo_phase[1]++;
        g_state.demo_timer_global = 30;
        g_state.Flash_Sign[0] = 0;
        g_state.Flash_Sign[1] = 1;
    }
}

/** @brief Ranking_Init phase 5 — delay then clear the flash sign. */
void Ranking_00_5th() {
    if (--g_state.demo_timer_global == 0) {
        g_state.demo_phase[1]++;
        g_state.demo_timer_global = 300;
        g_state.Flash_Sign[1] = 0;
    }
}

/** @brief Ranking_Init phase 6 — final display timer and BGM fade-out, then signal exit. */
void Ranking_00_6th() {
    if (--g_state.demo_timer_global == 0) {
        g_state.demo_timer_global = 1;
        g_state.Ranking_X = 1;
        return;
    }

    if (g_state.demo_timer_global == 60) {
        SsBgmFadeOut(546);
    }
}

/** @brief Ranking table 01 dispatcher — used from demo/attract-mode sequence. */
void Ranking_Display() {
    switch (g_state.demo_phase[1]) {
    case R01_INIT_SCREEN:
        Ranking_01_1st();
        break;
    case R01_LAYOUT_ENTRIES:
        Ranking_01_2nd();
        break;
    case R01_WAIT_FLASH:
        Ranking_00_3rd();
        break; /* shared phase 3 */
    case R01_COUNTDOWN:
        Ranking_01_4th();
        break;
    case R01_FADE_EXIT:
        Ranking_01_5th();
        break;
    default:
        break;
    }
}

/** @brief Ranking_Display phase 1 — switch screen, start BGM, purge resources, build texcache. */
void Ranking_01_1st() {
    Switch_Screen(1);
    BGM_Request(57);
    Purge_mmtm_area(4);
    Allocate_Texture_Cache_List(2);
    Allocate_Texture_Cache(14);
    g_state.demo_phase[1]++;
    g_state.Suicide[0] = 0;
    g_state.Present_Rank[0] = 99;
    g_state.Present_Rank[1] = 99;
    Ranking_Sub();
    effect_58_init(1, 1, -1);
}

/** @brief Ranking_Display phase 2 — lay out all rank entries and champion card, queue demo load. */
void Ranking_01_2nd() {
    s16 Char_Index;

    Switch_Screen(1);
    g_state.demo_phase[1]++;
    g_state.demo_timer_global = 420;
    g_state.Rank_X = 0;
    g_state.Flash_Rank_Time = 0;
    g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos - 104;
    g_state.Rank_Pos_Y = g_state.bg_w.bgw[0].xy[1].disp.pos + 160;
    Setup_Ranking_Obj();
    Setup_Score_Obj();
    if (g_state.Rank_Type == 0) {
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 168, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 5, 20, 0, 0);
    } else {
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos + 168, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 5, 20, 0, 0);
    }

    for (g_state.Rank = g_state.Rank_Type; g_state.Rank < (g_state.Rank_Type + 5); g_state.Rank++) {
        if ((g_state.Present_Rank[0] == (g_state.Rank - g_state.Rank_Type)) ||
            (g_state.Present_Rank[1] == (g_state.Rank - g_state.Rank_Type))) {
            g_state.Flash_Rank_Interval = 1;
        } else {
            g_state.Flash_Rank_Interval = 0;
        }

        Setup_Name(0);
        if (g_state.Rank_Type == 0) {
            Setup_Score(0);
            g_state.Rank_Pos_X += 4;
            Setup_grade(0);
            g_state.Rank_Pos_X -= 4;
        } else {
            Setup_Wins(0);
            Setup_grade(0);
        }

        Setup_Face(0);
        g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos - 104;
        g_state.Rank_Pos_Y -= 32;
        g_state.Rank_X = 0;
        g_state.Flash_Rank_Time = 0;
    }
    g_state.Order[85] = 1;
    g_state.Order_Timer[85] = 180;
    g_state.Order_Dir[85] = g_state.Rank_Type + 10;
    effect_76_init(85);
    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos + 512, g_state.bg_w.bgw[0].xy[1].disp.pos + 80, 180, 10, 35, 0, 0);
    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos + 504, g_state.bg_w.bgw[0].xy[1].disp.pos + 40, 180, 13, 35, 0, 0);
    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos + 240, g_state.bg_w.bgw[0].xy[1].disp.pos + 68, 180, 7, 30, 0, 0);
    g_state.base_y_pos = 40;
    switch (g_state.Rank_Type) {
    case 0:
        if (Ranking_Data[10].cpu_grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[10].cpu_grade;
        }
        break;
    case 5:
        if (Ranking_Data[15].grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[15].grade;
        }
        break;
    }
    effect_67_init(0,
                   g_state.bg_w.bgw[0].xy[0].disp.pos + 248,
                   g_state.bg_w.bgw[0].xy[1].disp.pos + 60,
                   180,
                   Char_Index,
                   10,
                   2,
                   1);
    g_state.Rank_Pos_X = g_state.bg_w.bgw[0].xy[0].disp.pos + 312;
    g_state.Rank_Pos_Y = g_state.bg_w.bgw[0].xy[1].disp.pos + 120;
    g_state.Rank_Pos_X += 16;
    g_state.Rank_Pos_Y -= 48;
    g_state.Rank = (g_state.Rank_Type + 10);
    g_state.Rank_Pos_Y -= 1;
    Setup_Name(0);
    g_state.Rank_Pos_Y += 1;
    g_state.Rank_Pos_X += 16;
    g_state.Rank_Pos_Y += 1;
    g_state.Rank_Pos_Y += 256;
    Setup_Face(0);
    g_state.Rank_Pos_Y -= 256;
    g_state.Rank_Pos_Y -= 1;
    g_state.Rank_Pos_X -= 32;
    g_state.Rank_Pos_Y -= 40;
    if (g_state.Rank_Type == 0) {
        g_state.Rank_Pos_X -= 80;
        Setup_Score(0);
    } else {
        g_state.Rank_Pos_X -= 32;
        Setup_Wins2(0);
    }

    g_state.Rank -= 10;
    if ((g_state.Present_Rank[0] < 5) || (g_state.Present_Rank[1] < 5)) {
        g_state.demo_phase[1]++;
    } else {
        g_state.demo_phase[1] = R01_FADE_EXIT;
    }

    if (g_state.fsm[1] < 6) {
        Setup_Demo_PL();
        Setup_Demo_Arts();
        Setup_Demo_Stage();
        Push_LDREQ_Queue_Player(0, g_state.My_char[0]);
        Push_LDREQ_Queue_Player(1, g_state.My_char[1]);
        Push_LDREQ_Queue_BG((s32)g_state.bg_w.stage);
    }
}

/** @brief Ranking_Display phase 4 — countdown delay before exit sequence. */
void Ranking_01_4th() {
    if (!(--g_state.demo_timer_global)) {
        g_state.demo_phase[1]++;
        g_state.demo_timer_global = 240;
    }
}

/** @brief Ranking_Display phase 5 — fade-out and transition to next demo/game. */
void Ranking_01_5th() {
    switch (g_state.demo_phase[2]) {
    case R01F_FADEOUT_INIT:
        if (!(--g_state.demo_timer_global)) {
            g_state.demo_phase[2] += 1;
            if ((g_state.Demo_Flag == 0) && (g_state.Demo_Type == 0)) {
                Clear_Personal_Data(0);
                Clear_Personal_Data(1);
            }

            Switch_Screen_Init(0);
            SsBgmFadeOut(1638);
        }
        break;

    case R01F_SCREEN_WAIT:
        if (Switch_Screen(0) != 0) {
            g_state.demo_phase[2] += 1;
            Game_ResetMatchState();
            g_state.Cover_Timer = 24;
            return;
        }
        break;

    default:
        Switch_Screen(1);
        g_state.Ranking_X = 1;
        break;
    }
}

/** @brief Common ranking setup — clear screen and write the appropriate BG for the rank type. */
void Ranking_Sub() {
    System_all_clear_Level_B();
    if (g_state.Rank_Type == 0) {
        bg_etc_write(1);
    } else {
        bg_etc_write(6);
    }

    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Render the grade icon (CPU grade or player grade) for the current rank entry. */
void Setup_grade(s16 y) {
    s16 Char_Index;

    switch (g_state.Rank_Type) {
    case 0:
        if (Ranking_Data[g_state.Rank].cpu_grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[g_state.Rank].cpu_grade;
        }
        break;

    case 5:
        if (Ranking_Data[g_state.Rank].grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[g_state.Rank].grade;
        }
        /* fallthrough */
    case 10:
        if (Ranking_Data[g_state.Rank].cpu_grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[g_state.Rank].cpu_grade;
        }
        /* fallthrough */
    case 15:
        if (Ranking_Data[g_state.Rank].grade == -1) {
            Char_Index = 0;
        } else {
            Char_Index = Ranking_Data[g_state.Rank].grade;
        }
        break;
    }

    effect_67_init(26, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, Char_Index + 98, 10, y, 0);
    if ((g_state.Rank_Type) == 0) {
        g_state.Rank_Pos_X += 16;
        if (Ranking_Data[g_state.Rank].all_clear) {
            g_state.Rank_Pos_Y += 16;
            effect_67_init(26, g_state.Rank_Pos_X - 4, g_state.Rank_Pos_Y + 3, 180, 74, 10, y, 0);
            g_state.Rank_Pos_Y -= 16;
        }

        g_state.Rank_Pos_X -= 16;
    }

    g_state.Rank_Pos_X += 48;
}

/** @brief Render the 3-letter name for the current rank entry. */
void Setup_Name(s16 y) {
    Name_Sub(0, y);
    Name_Sub(1, y);
    Name_Sub(2, y);

    if (g_state.Rank_Type != 0) {
        g_state.Rank_Pos_X += 16;
    }

    g_state.Rank_X += 3;
}

/** @brief Render a single name character at the current g_state.Rank_Pos_X. */
void Name_Sub(s16 xx, s16 y) {
    effect_67_init(26, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, Ranking_Data[g_state.Rank].name[xx], 10, y, 0);
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
    g_state.Rank_Pos_X += 24;
}

/** @brief Render the character portrait icon for the current rank entry. */
void Setup_Face(s16 y) {
    g_state.Rank_Pos_Y += 16;
    effect_67_init(26, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, Ranking_Data[g_state.Rank].player + 47, 10, y, 1);
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
    g_state.Rank_Pos_X += 40;
    g_state.Rank_Pos_Y -= 16;
    g_state.Rank_X += 2;
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
}

/** @brief Render the 8-digit score as individual digit sprites. */
void Setup_Score(s16 y) {
    s16 i;
    s16 First_Digit;
    u32 xx;
    u32 Score_Buff;
    s16 Digit[8];

    u32 assign1;
    u32 assign2;
    u32 assign3;
    u32 assign4;

    Score_Buff = Ranking_Data[g_state.Rank].score;
    First_Digit = -1;
    for (i = 7, assign1 = xx = 10000000; i > 0; i--, assign2 = xx /= 10) {
        Digit[i] = (Score_Buff / xx);
        Score_Buff -= xx * Digit[i];
        if ((First_Digit < 0) && (Digit[i])) {
            First_Digit = i;
        }
    }

    Digit[0] = Score_Buff;
    if (First_Digit < 0) {
        First_Digit = 0;
    }

    for (i = 0, assign3 = xx = 7; i < 8; i++, assign4 = xx--) {
        g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
        g_state.Rank_Pos_X += 16;
        if (First_Digit >= xx) {
            effect_67_init(26, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, Digit[xx] + 75, 10, y, 0);
        }
    }

    g_state.Rank_Pos_X += 24;
}

/**
 * @brief Decompose Ranking_Data[g_state.Rank].wins into 3 decimal digits and render
 *        each one as an effect sprite.
 *
 * @param y             Effect layer parameter forwarded to effect_67_init.
 * @param digit_offset  Sprite-index offset added to each digit value
 *                      (130 for the normal table, 75 for the champion card).
 * @param mid_nudge     Extra X pixels per digit position (1× for tens, 2× for ones;
 *                      3 for the normal table, 0 for the champion card).
 */
static void render_wins_digits(s16 y, s16 digit_offset, s16 mid_nudge) {
    u32 Score_Buff;
    s16 Digit[3];

    Score_Buff = Ranking_Data[g_state.Rank].wins;
    Digit[2] = Score_Buff / 100;
    Score_Buff -= Digit[2] * 100;
    Digit[1] = Score_Buff / 10;
    Digit[0] = (Score_Buff -= (Digit[1] * 10));

    if (Digit[2]) {
        effect_67_init(26, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, Digit[2] + digit_offset, 10, y, 0);
        g_state.Rank_Pos_X += 16;
        effect_67_init(26, g_state.Rank_Pos_X + mid_nudge, g_state.Rank_Pos_Y, 180, Digit[1] + digit_offset, 10, y, 0);
        g_state.Rank_Pos_X += 16;
    } else {
        g_state.Rank_Pos_X += 16;
        if (Digit[1]) {
            effect_67_init(
                26, g_state.Rank_Pos_X + mid_nudge, g_state.Rank_Pos_Y, 180, Digit[1] + digit_offset, 10, y, 0);
        }

        g_state.Rank_Pos_X += 16;
    }

    g_state.Rank_X += 1;
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
    effect_67_init(26, g_state.Rank_Pos_X + 2 * mid_nudge, g_state.Rank_Pos_Y, 180, Digit[0] + digit_offset, 10, y, 0);
    g_state.Rank_Pos_X += 40;
}

/** @brief Render a win-count (up to 3 digits) with a WINS label. */
void Setup_Wins(s16 y) {
    render_wins_digits(y, 130, 3);
    g_state.Rank_X += 1;
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
    effect_67_init(24, g_state.Rank_Pos_X, g_state.Rank_Pos_Y, 180, 6, 10, y, 0);
    g_state.Rank_Pos_X += 64;
    g_state.Rank_X += 1;
}

/** @brief Render a win-count variant for the champion card (top-entry). */
void Setup_Wins2(s16 y) {
    render_wins_digits(y, 75, 0);
    effect_67_init(24, g_state.Rank_Pos_X, g_state.Rank_Pos_Y + 8, 180, 11, 10, y, 0);
    g_state.Rank_X += 1;
    g_state.Flash_Rank_Time += g_state.Flash_Rank_Interval;
}

/** @brief Spawn the RANKING header label as an effect object. */
void Setup_Ranking_Obj() {
    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos - 143, g_state.bg_w.bgw[0].xy[1].disp.pos + 32, 180, 4, 30, 0, 0);
}

/** @brief Spawn SCORE/WINS header labels depending on g_state.Rank_Type. */
void Setup_Score_Obj() {
    if (g_state.Rank_Type == 0) {
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos - 0, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 0, 10, 1, 0);
        effect_67_init(
            24, g_state.bg_w.bgw[0].xy[0].disp.pos - 384, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 1, 10, 1, 0);
        return;
    }

    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos - 0, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 2, 10, 1, 0);
    effect_67_init(
        24, g_state.bg_w.bgw[0].xy[0].disp.pos - 384, g_state.bg_w.bgw[0].xy[1].disp.pos + 200, 180, 3, 10, 1, 0);
}

/** @brief Initialise all save-data ranking slots from the default Score_Ranking_Table. */
void Ranking_Init() {
    s16 ix;
    s16 ix2;

    for (ix = 0; ix < 4; ix++) {
        for (ix2 = 0; ix2 < 20; ix2++) {
            save_w[ix].Ranking[ix2] = Score_Ranking_Table[ix2];
        }
    }
}

const RANK_DATA Score_Ranking_Table[20] = { { .name = { 28, 13, 22 },
                                              .player = 15,
                                              .score = 100000,
                                              .cpu_grade = 10,
                                              .grade = 0,
                                              .wins = 10,
                                              .player_color = 0,
                                              .all_clear = 1 },
                                            { .name = { 22, 24, 24 },
                                              .player = 16,
                                              .score = 90000,
                                              .cpu_grade = 9,
                                              .grade = 0,
                                              .wins = 9,
                                              .player_color = 1,
                                              .all_clear = 0 },
                                            { .name = { 23, 14, 24 },
                                              .player = 17,
                                              .score = 80000,
                                              .cpu_grade = 8,
                                              .grade = 0,
                                              .wins = 8,
                                              .player_color = 2,
                                              .all_clear = 1 },
                                            { .name = { 24, 23, 30 },
                                              .player = 18,
                                              .score = 70000,
                                              .cpu_grade = 7,
                                              .grade = 0,
                                              .wins = 7,
                                              .player_color = 3,
                                              .all_clear = 0 },
                                            { .name = { 34, 24, 20 },
                                              .player = 19,
                                              .score = 60000,
                                              .cpu_grade = 6,
                                              .grade = 0,
                                              .wins = 6,
                                              .player_color = 4,
                                              .all_clear = 0 },
                                            { .name = { 20, 10, 18 },
                                              .player = 15,
                                              .score = 100000,
                                              .cpu_grade = 10,
                                              .grade = 10,
                                              .wins = 10,
                                              .player_color = 0,
                                              .all_clear = 1 },
                                            { .name = { 21, 29, 1 },
                                              .player = 16,
                                              .score = 90000,
                                              .cpu_grade = 9,
                                              .grade = 9,
                                              .wins = 9,
                                              .player_color = 1,
                                              .all_clear = 0 },
                                            { .name = { 34, 30, 20 },
                                              .player = 17,
                                              .score = 80000,
                                              .cpu_grade = 8,
                                              .grade = 8,
                                              .wins = 8,
                                              .player_color = 2,
                                              .all_clear = 1 },
                                            { .name = { 27, 10, 24 },
                                              .player = 18,
                                              .score = 70000,
                                              .cpu_grade = 7,
                                              .grade = 7,
                                              .wins = 7,
                                              .player_color = 3,
                                              .all_clear = 0 },
                                            { .name = { 0, 0, 8 },
                                              .player = 19,
                                              .score = 60000,
                                              .cpu_grade = 6,
                                              .grade = 6,
                                              .wins = 6,
                                              .player_color = 4,
                                              .all_clear = 0 },
                                            { .name = { 18, 12, 17 },
                                              .player = 15,
                                              .score = 100000,
                                              .cpu_grade = 11,
                                              .grade = 0,
                                              .wins = 10,
                                              .player_color = 0,
                                              .all_clear = 1 },
                                            { .name = { 28, 14, 29 },
                                              .player = 5,
                                              .score = 90000,
                                              .cpu_grade = 9,
                                              .grade = 0,
                                              .wins = 9,
                                              .player_color = 1,
                                              .all_clear = 0 },
                                            { .name = { 23, 14, 24 },
                                              .player = 4,
                                              .score = 80000,
                                              .cpu_grade = 8,
                                              .grade = 0,
                                              .wins = 8,
                                              .player_color = 2,
                                              .all_clear = 1 },
                                            { .name = { 24, 23, 30 },
                                              .player = 10,
                                              .score = 70000,
                                              .cpu_grade = 7,
                                              .grade = 0,
                                              .wins = 7,
                                              .player_color = 3,
                                              .all_clear = 0 },
                                            { .name = { 34, 24, 20 },
                                              .player = 8,
                                              .score = 60000,
                                              .cpu_grade = 6,
                                              .grade = 0,
                                              .wins = 6,
                                              .player_color = 4,
                                              .all_clear = 0 },
                                            { .name = { 18, 23, 14 },
                                              .player = 19,
                                              .score = 100000,
                                              .cpu_grade = 11,
                                              .grade = 11,
                                              .wins = 10,
                                              .player_color = 0,
                                              .all_clear = 1 },
                                            { .name = { 21, 29, 1 },
                                              .player = 1,
                                              .score = 90000,
                                              .cpu_grade = 9,
                                              .grade = 9,
                                              .wins = 9,
                                              .player_color = 1,
                                              .all_clear = 0 },
                                            { .name = { 34, 30, 20 },
                                              .player = 4,
                                              .score = 80000,
                                              .cpu_grade = 8,
                                              .grade = 8,
                                              .wins = 8,
                                              .player_color = 2,
                                              .all_clear = 1 },
                                            { .name = { 27, 10, 24 },
                                              .player = 10,
                                              .score = 70000,
                                              .cpu_grade = 7,
                                              .grade = 7,
                                              .wins = 7,
                                              .player_color = 3,
                                              .all_clear = 0 },
                                            { .name = { 18, 23, 14 },
                                              .player = 8,
                                              .score = 60000,
                                              .cpu_grade = 6,
                                              .grade = 6,
                                              .wins = 6,
                                              .player_color = 4,
                                              .all_clear = 0 } };
