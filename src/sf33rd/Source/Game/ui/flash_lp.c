/**
 * @file flash_lp.c
 * @brief Win-mark flash lamp animation.
 *
 * Drives the alternating flash on victory markers during gameplay.
 * Skipped in training modes.
 *
 * Part of the ui module.
 */

#include "sf33rd/Source/Game/ui/flash_lp.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#define LAMP_FLASH_COUNT 2

const u8 Lamp_Flash_Data[LAMP_FLASH_COUNT][2] = { { 0x07, 0x6F }, { 0x1E, 0x03 } };

/** @brief Animate the win-mark lamps — alternate flash colors each frame. */
void Flash_Lamp() {
    u8 ix;
    u8 ix2p;
    u8 mark;
    u8 color;

    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
        return;
    }

    if (omop_cockpit == 0) {
        return;
    }

    if (!Game_pause) {
        switch (Lamp_No) {
        case 0:
            Lamp_No = 1;
            Lamp_Index = 1;
            Lamp_Timer = 1;
            /* fallthrough */

        case 1:
            if (--Lamp_Timer == 0) {
                if (++Lamp_Index > 1) {
                    Lamp_Index = 0;
                }

                if (Lamp_Index < LAMP_FLASH_COUNT) {
                    Lamp_Color = Lamp_Flash_Data[Lamp_Index][0];
                    Lamp_Timer = Lamp_Flash_Data[Lamp_Index][1];
                }
            }

            break;
        }
    }

    for (ix = 0; ix <= save_w[Present_Mode].Battle_Number[Play_Type]; ix++) {
        mark = flash_win_type[0][ix];

        if (flash_win_type[0][ix] == 0) {
            color = 7;
        } else {
            color = Lamp_Color;
        }

        if (flash_win_type[0][ix] == sync_win_type[0][ix]) {
            scfont_sqput(vmark_tbl[ix], 4, color, 0, mark * 2, 26, 2, 1, 2);
        }

        mark = flash_win_type[1][ix];
        ix2p = ix + 4;

        if (flash_win_type[1][ix] == 0) {
            color = 7;
        } else {
            color = Lamp_Color;
        }

        if (flash_win_type[1][ix] == sync_win_type[1][ix]) {
            scfont_sqput(vmark_tbl[ix2p], 4, color, 0, mark * 2, 26, 2, 1, 2);
        }
    }
}
