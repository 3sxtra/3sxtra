/**
 * @file sys_score.c
 * @brief Score display, win records, digit rendering, and copyright.
 *
 * Handles calculating and displaying score digits on the HUD,
 * rendering win-record counts, 16x24 digit rendering, and
 * displaying the Capcom copyright text.
 *
 * Split from sys_sub.c for organizational clarity.
 */

#include "sf33rd/Source/Game/system/sys_score.h"
#include "common.h"
#include "main.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

/** @brief Calculate and display the score digits on the HUD for active players. */
void Score_Sub() {
    u32 Score_Buff;
    s8 i;
    s8 j;
    s32 xx;
    s8 First_Digit;
    s8 Digit[8];
    s16 PL_id;

    s8 assign1;
    s32 assign2;
    s8 assign3;

    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
        return;
    }

    if (omop_cockpit == 0) {
        return;
    }

    for (PL_id = 0; PL_id < 2; PL_id++) {
        if ((Mode_Type != MODE_VERSUS && Mode_Type != MODE_REPLAY) && plw[PL_id].wu.pl_operator == 0) {
            continue;
        }

        if (Stop_Update_Score) {
            Score_Buff = Keep_Score[PL_id];
        } else {
            Score_Buff = Score[PL_id][Play_Type];
            Score_Buff += Continue_Coin[PL_id];
            Keep_Score[PL_id] = Score_Buff;
        }

        for (i = 7, xx = 10000000, assign1 = First_Digit = -1; i > 0; i--, assign2 = xx /= 10) {
            Digit[i] = Score_Buff / xx;
            Score_Buff -= Digit[i] * xx;

            if (First_Digit < 0 && Digit[i]) {
                First_Digit = i;
            }
        }

        Digit[0] = Score_Buff;

        if (First_Digit < 0) {
            First_Digit = 1;
        }

        for (i = Coin_Message_Data[3][PL_id] - First_Digit, j = First_Digit; j >= 0; j--, assign3 = i++) {
            score8x16_put(i, 0, 8, Digit[j]);
        }
    }
}

/** @brief Display win-record count(s) on the HUD according to the current game mode. */
void Disp_Win_Record() {
    s16 PL_id;
    s16 zz;

    if (omop_cockpit == 0) {
        return;
    }

    switch (Mode_Type) {
    case MODE_ARCADE:
        if (Play_Type == 1) {
            if (Win_Record[0] != 0 || Win_Record[1] != 0) {
                if (Win_Record[0]) {
                    PL_id = 0;
                    zz = 5;
                } else {
                    PL_id = 1;
                    zz = 43;
                }
            } else {
                break;
            }
        } else if (Win_Record[Player_id] == 0) {
            break;
        } else {
            PL_id = Player_id;

            if (Player_id == 0) {
                zz = 5;
            } else {
                zz = 43;
            }
        }

        Disp_Win_Record_Sub(Win_Record[PL_id], zz);
        break;

    case MODE_VERSUS:
    case MODE_NETWORK:
        if (VS_Win_Record[0] > 0) {
            Disp_Win_Record_Sub(VS_Win_Record[0], 5);
        }

        if (VS_Win_Record[1] > 0) {
            Disp_Win_Record_Sub(VS_Win_Record[1], 43);
        }

        break;

    default:
        // Do nothing
        break;
    }
}

/** @brief Render a win-record number and "WIN"/"WINS" label at the specified HUD X position. */
void Disp_Win_Record_Sub(u16 win_record, s16 zz) {
    s16 xx;
    s16 Wins_Buff;
    s16 First_Digit;

    switch (win_record) {
    case 1:
        SSPutStr(zz, 0, 9, "WIN");
        break;

    default:
        SSPutStr(zz, 0, 9, "WINS");
        break;
    }

    First_Digit = 0;
    Wins_Buff = win_record;
    xx = Wins_Buff / 100;

    if (xx > 0) {
        First_Digit = 1;
        SSPutDec(zz - 4, 0, 9, xx, 1);
    }

    Wins_Buff -= xx * 100;
    xx = Wins_Buff / 10;

    if (First_Digit != 0 || xx > 0) {
        SSPutDec(zz - 3, 0, 9, xx, 1);
    }

    Wins_Buff -= xx * 10;

    SSPutDec(zz - 2, 0, 9, Wins_Buff, 1);
}

/** @brief Render a score value as 16x24 digits at the given screen position and color. */
void Disp_Digit16x24(u32 Score_Buff, s16 Disp_X, s16 Disp_Y, s16 Color) {
    s16 i;
    s16 j;
    s32 xx;
    s16 First_Digit;
    s16 Digit[8];

    s16 s6;
    s32 s5;
    s16 s4;

    if (Score_Buff == 0) {
        score16x24_put(Disp_X, Disp_Y, 15, 0);
    }

    for (i = 7, xx = 10000000, s6 = First_Digit = -1; i > 0; i--, s5 = xx /= 10) {
        Digit[i] = Score_Buff / xx;
        Score_Buff -= xx * Digit[i];

        if (First_Digit < 0 && Digit[i]) {
            First_Digit = i;
        }
    }

    Digit[0] = Score_Buff;
    i = Disp_X - (First_Digit * 2);

    for (j = First_Digit; j >= 0; j--, s4 = i += 2) {
        score16x24_put(i, Disp_Y, Color, Digit[j]);
    }
}

/** @brief Display the appropriate Capcom copyright text based on the Country setting. */
void Disp_Copyright() {
    if (use_rmlui && rmlui_screen_copyright)
        return;
    s32 xres;

    switch (Country) {
    case 1:
    case 2:
    case 3:
    case 7:
    case 8:
        SSPutStrPro(1, 386, 208, 9, -1, "@CAPCOM CO., LTD. 1999, 2004 ALL RIGHTS RESERVED.");
        break;

    case 4:
    case 5:
    case 6:
        xres = SSPutStrPro(1, 386, 212, 9, -1, "@CAPCOM U.S.A., INC. 1999, 2004 ALL RIGHTS RESERVED.");
        SSPutStrPro(0, xres, 202, 9, -1, "@CAPCOM CO., LTD. 1999, 2004,");
        break;

    default:
        break;
    }
}
