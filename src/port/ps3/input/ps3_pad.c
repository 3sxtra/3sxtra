/* PS3 pad → TARPAD bridge (tarPADInit/Read/Destroy)
 * Maps cellPad buttons to SWK bitmask used by the game engine.
 */
#include "sf33rd/AcrSDK/ps2/ps2PAD.h"
#include <cell/pad.h>
#include <string.h>

TARPAD tarpad_root[2];
PS2Slot ps2slot[2];

void flPADConfigSetACRtoXX(s32 padnum, s16 a, s16 b, s16 c) {
    (void)padnum;
    (void)a;
    (void)b;
    (void)c;
    /* Handled by generic input mapping or skipped for native PS3 mapping */
}

s32 tarPADInit(void) {
    // I-MED-01 Audit Fix: cellPadInit is now centralized in PS3App_FullInit.
    // Just initialize the pad state structures.
    memset(tarpad_root, 0, sizeof(tarpad_root));
    memset(ps2slot, 0, sizeof(ps2slot));
    for (int i = 0; i < 2; i++) {
        ps2slot[i].state = 1;
        ps2slot[i].port = i;
    }
    return 1;
}

void tarPADDestroy(void) {
    // I-02 Audit Fix: Don't call cellPadEnd() here — SDLPad_GetButtonState may still
    // need pad access. The pad subsystem is torn down at process exit.
}

void tarPADRead(void) {
    for (int i = 0; i < 2; i++) {
        CellPadData pad_data;
        if (cellPadGetData(i, &pad_data) == CELL_PAD_OK && pad_data.len > 0) {
            u32 io = 0;

            // H-07 Audit Fix: Use proper d1/d2 byte offsets for button mapping
            uint8_t d1 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL1];
            uint8_t d2 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2];

            if (d1 & CELL_PAD_CTRL_UP) io |= SWK_UP;
            if (d1 & CELL_PAD_CTRL_DOWN) io |= SWK_DOWN;
            if (d1 & CELL_PAD_CTRL_LEFT) io |= SWK_LEFT;
            if (d1 & CELL_PAD_CTRL_RIGHT) io |= SWK_RIGHT;
            
            if (d2 & CELL_PAD_CTRL_CROSS) io |= SWK_SOUTH;
            if (d2 & CELL_PAD_CTRL_CIRCLE) io |= SWK_EAST;
            if (d2 & CELL_PAD_CTRL_SQUARE) io |= SWK_WEST;
            if (d2 & CELL_PAD_CTRL_TRIANGLE) io |= SWK_NORTH;
            
            if (d2 & CELL_PAD_CTRL_L1) io |= SWK_LEFT_SHOULDER;
            if (d2 & CELL_PAD_CTRL_R1) io |= SWK_RIGHT_SHOULDER;
            if (d2 & CELL_PAD_CTRL_L2) io |= SWK_LEFT_TRIGGER;
            if (d2 & CELL_PAD_CTRL_R2) io |= SWK_RIGHT_TRIGGER;
            if (d1 & CELL_PAD_CTRL_L3) io |= SWK_LEFT_STICK;
            if (d1 & CELL_PAD_CTRL_R3) io |= SWK_RIGHT_STICK;
            if (d1 & CELL_PAD_CTRL_START) io |= SWK_START;
            if (d1 & CELL_PAD_CTRL_SELECT) io |= SWK_BACK;

            tarpad_root[i].sw = io;

            if (pad_data.len >= 8) {
                tarpad_root[i].stick[0].x = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] - 128;
                tarpad_root[i].stick[0].y = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] - 128;
                tarpad_root[i].stick[1].x = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] - 128;
                tarpad_root[i].stick[1].y = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] - 128;
            }

            tarpad_root[i].state = 1;
            tarpad_root[i].conn.port = i;
            tarpad_root[i].kind = 1;
        } else {
            tarpad_root[i].sw = 0;
            tarpad_root[i].stick[0].x = 0;
            tarpad_root[i].stick[0].y = 0;
            tarpad_root[i].stick[1].x = 0;
            tarpad_root[i].stick[1].y = 0;
        }
    }


}
