/**
 * @file sys_options.c
 * @brief Game option save/load/defaults/compare subsystem.
 *
 * Handles saving and loading game options to/from save_w[],
 * resetting to defaults, detecting option changes, copying
 * button mappings, and system-direction page lookup.
 *
 * Split from sys_sub.c for organizational clarity.
 */

#include "sf33rd/Source/Game/system/sys_options.h"
#include "game_state.h"
#include "common.h"
#include "main.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/menu/ex_data.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"

const s8 Time_Limit_Data[4] = { 30, 60, 99, -1 };

const s8 Battle_Number_Data[4] = { 0, 1, 2, 3 };

/** @brief Initialize game data: reset options to defaults, check analog stick override, init rankings. */
void Game_Data_Init() {
    s32 ix;

    Setup_Default_Game_Option();
    mpp_w.cutAnalogStickData = false;

    if ((flpad_adr[0][0].sw & 0x330) == 0x330) {
        mpp_w.cutAnalogStickData = true;
    } else if ((flpad_adr[0][1].sw & 0x330) == 0x330) {
        mpp_w.cutAnalogStickData = true;
    }

    if (mpp_w.cutAnalogStickData) {
        for (ix = 0; ix < SAVEW_COUNT; ix++) {
            save_w[ix].AnalogStick = 0;
        }
    }

    Ranking_Init();
    Copy_Save_w();
}

/** @brief Reset the I/O button conversion data to the default mapping for the given controller slot. */
void Setup_IO_ConvDataDefault(s32 id) {
    const u8 ioConvInitData[12] = { 0, 1, 2, 11, 3, 4, 5, 11, 0, 0, 0, 0 };
    s32 ix;

    for (ix = 0; ix < 12; ix++) {
        g_state.Convert_Buff[1][id][ix] = ioConvInitData[ix];
    }
}

/** @brief Copy current game option settings from g_state.Convert_Buff into save_w[SAVEW_ARCADE]. */
void Save_Game_Data() {
    s16 ix;

    save_w[SAVEW_ARCADE].Difficulty = g_state.Convert_Buff[0][0][0];
    save_w[SAVEW_ARCADE].Time_Limit = Time_Limit_Data[g_state.Convert_Buff[0][0][1]];
    save_w[SAVEW_ARCADE].Battle_Number[0] = Battle_Number_Data[g_state.Convert_Buff[0][0][2]];
    save_w[SAVEW_ARCADE].Battle_Number[1] = Battle_Number_Data[g_state.Convert_Buff[0][0][3]];
    save_w[SAVEW_ARCADE].Damage_Level = g_state.Convert_Buff[0][0][4];
    save_w[SAVEW_ARCADE].GuardCheck = g_state.Convert_Buff[0][0][5];
    save_w[SAVEW_ARCADE].AnalogStick = g_state.Convert_Buff[0][0][6];
    save_w[SAVEW_ARCADE].Handicap = g_state.Convert_Buff[0][0][7];
    save_w[SAVEW_ARCADE].Partner_Type[0] = g_state.Convert_Buff[0][0][8];
    save_w[SAVEW_ARCADE].Partner_Type[1] = g_state.Convert_Buff[0][0][9];
    mpp_w.useAnalogStickData = save_w[SAVEW_ARCADE].AnalogStick;
    save_w[SAVEW_TRAINING].GuardCheck = save_w[SAVEW_ARCADE].GuardCheck;
    save_w[SAVEW_EXTRA].GuardCheck = save_w[SAVEW_ARCADE].GuardCheck;

    for (ix = 0; ix < 8; ix++) {
        save_w[SAVEW_ARCADE].Pad_Infor[0].Shot[ix] = g_state.Convert_Buff[1][0][ix];
        save_w[SAVEW_ARCADE].Pad_Infor[1].Shot[ix] = g_state.Convert_Buff[1][1][ix];
    }

    save_w[SAVEW_ARCADE].Pad_Infor[0].Vibration = g_state.Convert_Buff[1][0][8];
    save_w[SAVEW_ARCADE].Pad_Infor[1].Vibration = g_state.Convert_Buff[1][1][8];
    save_w[SAVEW_TRAINING].Pad_Infor[0] = save_w[SAVEW_ARCADE].Pad_Infor[0];
    save_w[SAVEW_TRAINING].Pad_Infor[1] = save_w[SAVEW_ARCADE].Pad_Infor[1];
    save_w[SAVEW_EXTRA].Pad_Infor[0] = save_w[SAVEW_ARCADE].Pad_Infor[0];
    save_w[SAVEW_EXTRA].Pad_Infor[1] = save_w[SAVEW_ARCADE].Pad_Infor[1];
    save_w[SAVEW_ARCADE].Adjust_X = g_state.Convert_Buff[2][0][0];
    save_w[SAVEW_ARCADE].Adjust_Y = g_state.Convert_Buff[2][0][1];
    save_w[SAVEW_ARCADE].Screen_Size = g_state.Convert_Buff[2][0][2];
    save_w[SAVEW_ARCADE].Screen_Mode = g_state.Convert_Buff[2][0][3];
    save_w[SAVEW_ARCADE].Auto_Save = g_state.Convert_Buff[3][0][2];
    save_w[SAVEW_ARCADE].SoundMode = g_state.Convert_Buff[3][1][0];
    save_w[SAVEW_ARCADE].BGM_Level = g_state.Convert_Buff[3][1][1];
    save_w[SAVEW_ARCADE].SE_Level = g_state.Convert_Buff[3][1][2];
    save_w[SAVEW_ARCADE].BgmType = g_state.Convert_Buff[3][1][3];
}

/** @brief Copy game option settings from save_w[SAVEW_ARCADE] into g_state.Convert_Buff for display/editing. */
void Copy_Save_w() {
    s16 ix;

    g_state.Convert_Buff[0][0][0] = save_w[SAVEW_ARCADE].Difficulty;
    g_state.Convert_Buff[0][0][2] = save_w[SAVEW_ARCADE].Battle_Number[0];
    g_state.Convert_Buff[0][0][3] = save_w[SAVEW_ARCADE].Battle_Number[1];
    g_state.Convert_Buff[0][0][4] = save_w[SAVEW_ARCADE].Damage_Level;
    g_state.Convert_Buff[0][0][5] = save_w[SAVEW_ARCADE].GuardCheck;
    g_state.Convert_Buff[0][0][6] = save_w[SAVEW_ARCADE].AnalogStick;
    g_state.Convert_Buff[0][0][7] = save_w[SAVEW_ARCADE].Handicap;
    g_state.Convert_Buff[0][0][8] = save_w[SAVEW_ARCADE].Partner_Type[0];
    g_state.Convert_Buff[0][0][9] = save_w[SAVEW_ARCADE].Partner_Type[1];
    mpp_w.useAnalogStickData = save_w[SAVEW_ARCADE].AnalogStick;

    switch (save_w[SAVEW_ARCADE].Time_Limit) {
    case 30:
        g_state.Convert_Buff[0][0][1] = 0;
        break;
    case 60:
        g_state.Convert_Buff[0][0][1] = 1;
        break;
    case 99:
        g_state.Convert_Buff[0][0][1] = 2;
        break;
    default:
        g_state.Convert_Buff[0][0][1] = 3;
        break;
    }

    for (ix = 0; ix < 8; ix++) {
        g_state.Convert_Buff[1][0][ix] = save_w[SAVEW_ARCADE].Pad_Infor[0].Shot[ix];
        g_state.Convert_Buff[1][1][ix] = save_w[SAVEW_ARCADE].Pad_Infor[1].Shot[ix];
    }

    g_state.Convert_Buff[1][0][8] = save_w[SAVEW_ARCADE].Pad_Infor[0].Vibration;
    g_state.Convert_Buff[1][1][8] = save_w[SAVEW_ARCADE].Pad_Infor[1].Vibration;
    g_state.Convert_Buff[2][0][0] = save_w[SAVEW_ARCADE].Adjust_X;
    g_state.Convert_Buff[2][0][1] = save_w[SAVEW_ARCADE].Adjust_Y;
    g_state.Convert_Buff[2][0][2] = save_w[SAVEW_ARCADE].Screen_Size;
    g_state.Convert_Buff[2][0][3] = save_w[SAVEW_ARCADE].Screen_Mode;
    sys_w.screen_mode = save_w[SAVEW_ARCADE].Screen_Mode;
    g_state.Convert_Buff[3][0][2] = save_w[SAVEW_ARCADE].Auto_Save;
    g_state.Convert_Buff[3][1][0] = save_w[SAVEW_ARCADE].SoundMode;
    g_state.Convert_Buff[3][1][1] = save_w[SAVEW_ARCADE].BGM_Level;
    g_state.Convert_Buff[3][1][2] = save_w[SAVEW_ARCADE].SE_Level;
    g_state.Convert_Buff[3][1][3] = save_w[SAVEW_ARCADE].BgmType;
    for (ix = 0; ix < 20; ix++) {
        Ranking_Data[ix] = save_w[SAVEW_ARCADE].Ranking[ix];
    }
}

/** @brief Snapshot current g_state.Convert_Buff into g_state.Check_Buff for later change-detection comparison. */
void Copy_Check_w() {
    s16 ix;
    s16 ix2;

    for (ix = 0; ix < 4; ix++) {
        for (ix2 = 0; ix2 < 12; ix2++) {
            g_state.Check_Buff[ix][0][ix2] = g_state.Convert_Buff[ix][0][ix2];
            g_state.Check_Buff[ix][1][ix2] = g_state.Convert_Buff[ix][1][ix2];
        }
    }

    g_state.ck_ex_option = save_w[SAVEW_ARCADE].extra_option;
}

const struct _SAVE_W Game_Default_Data = {
    { { { 0, 1, 2, 11, 3, 4, 5, 11 }, 0, { 0, 0, 0 } }, { { 0, 1, 2, 11, 3, 4, 5, 11 }, 0, { 0, 0, 0 } } },
    2,
    99,
    { 1, 1 },
    1,
    1,
    { 0, 0 },
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
    15,
    15,
    0,
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { { 1, 3, 3, 0, 0, 1, 0, 0 },
        { 0, 0, 2, 2, 8, 8, 2, 0 },
        { 2, 2, 2, 2, 0, 0, 0, 0 },
        { 1, 1, 1, 1, 1, 1, 0, 0 } } },
    { { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 },
      { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 }, { { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0 },
    1,
    0
};

/** @brief Reset all 6 save_w slots to the Game_Default_Data template. */
void Setup_Default_Game_Option() {
    s16 ix;

    for (ix = 0; ix < SAVEW_COUNT; ix++) {
        save_w[ix] = Game_Default_Data;
        save_w[ix].sum = 0;
    }
}

/** @brief Compare g_state.Convert_Buff against g_state.Check_Buff; returns 1 if any option has been modified. */
s32 Check_Change_Contents() {
    s16 ix;
    s16 ix2;
    s16 page;

    g_state.Check_Buff[3][0][0] = g_state.Convert_Buff[3][0][0];

    for (ix = 4; ix < 12; ix++) {
        g_state.Check_Buff[3][1][ix] = g_state.Convert_Buff[3][1][ix];
    }

    for (ix = 0; ix < 4; ix++) {
        for (ix2 = 0; ix2 < 12; ix2++) {
            if (g_state.Convert_Buff[ix][0][ix2] != g_state.Check_Buff[ix][0][ix2]) {
                return 1;
            }

            if (g_state.Convert_Buff[ix][1][ix2] != g_state.Check_Buff[ix][1][ix2]) {
                return 1;
            }
        }
    }

    for (page = 0; page < 4; page++) {
        for (ix = Ex_Page_Data[page]; ix < 8; ix++) {
            g_state.ck_ex_option.contents[page][ix] = save_w[SAVEW_ARCADE].extra_option.contents[page][ix];
        }
    }

    for (ix = 0; ix < 4; ix++) {
        for (ix2 = 0; ix2 < 8; ix2++) {
            if (g_state.ck_ex_option.contents[ix][ix2] != save_w[SAVEW_ARCADE].extra_option.contents[ix][ix2]) {
                return 1;
            }
        }
    }

    return 0;
}

/** @brief Copy saved button-mapping data from save_w[SAVEW_ARCADE] into g_state.Convert_Buff for display. */
void Copy_Key_Disp_Work() {
    s16 ix;

    for (ix = 0; ix < 8; ix++) {
        g_state.Convert_Buff[1][0][ix] = save_w[SAVEW_ARCADE].Pad_Infor[0].Shot[ix];
        g_state.Convert_Buff[1][1][ix] = save_w[SAVEW_ARCADE].Pad_Infor[1].Shot[ix];
    }

    g_state.Convert_Buff[1][0][8] = save_w[SAVEW_ARCADE].Pad_Infor[0].Vibration;
    g_state.Convert_Buff[1][1][8] = save_w[SAVEW_ARCADE].Pad_Infor[1].Vibration;
}

/** @brief Compare extra-option settings between save_w[SAVEW_BASE] and save_w[SAVEW_ARCADE]; returns 1 if different. */
s32 Check_Extra_Setting() {
    s16 ix;
    s16 page;

    for (page = 0; page < 4; page++) {
        for (ix = Ex_Page_Data[page]; ix < 8; ix++) {
            save_w[SAVEW_ARCADE].extra_option.contents[page][ix] = save_w[SAVEW_BASE].extra_option.contents[page][ix];
        }
    }

    for (page = 0; page < 4; page++) {
        for (ix = 0; ix < 4; ix++) {
            if (save_w[SAVEW_ARCADE].extra_option.contents[page][ix] !=
                save_w[SAVEW_BASE].extra_option.contents[page][ix]) {
                return 1;
            }
        }
    }

    return 0;
}

/** @brief Determine the system-direction page index based on debug overrides or unlocked colors. */
s16 Check_SysDir_Page() {
    s16 ix;
    s16 count;

    if (CurrentSave()->Unlock_All) {
        return 9;
    }

    if (Debug_w[DEBUG_SYSTEM_DIRECTION]) {
        return Debug_w[DEBUG_SYSTEM_DIRECTION] + 6;
    }

    for (count = 0, ix = 0; ix < 20; ix++) {
        if (save_w[SAVEW_ARCADE].PL_Color[0][ix]) {
            count++;
        }
    }

    count /= 5;

    if (count > 3) {
        count = 3;
    }

    return count + 6;
}
