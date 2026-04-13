/* PS3 pad → TARPAD bridge (tarPADInit/Read/Destroy)
 * Maps cellPad buttons to SWK bitmask used by the game engine.
 *
 * P8 Audit Fix: Configurable button remap table replaces hardcoded mapping.
 */
#include "sf33rd/AcrSDK/ps2/ps2PAD.h"
#include <cell/pad.h>
#include <string.h>

TARPAD tarpad_root[2];
PS2Slot ps2slot[2];

/* P8: Button remap table — maps CellPad control bits to SWK bitmask values.
 * Each entry: { cell_bit_mask (d1 or d2), is_d2, swk_bit }
 * The table is initialized to the default PS3 mapping and can be modified
 * at runtime via PS3Pad_SetButtonMap() after loading saved config. */
typedef struct {
    uint16_t cell_mask; /* CellPad button mask (e.g. CELL_PAD_CTRL_CROSS) */
    uint8_t is_d2;      /* 0 = digital1 byte, 1 = digital2 byte */
    uint32_t swk_bit;   /* Mapped SWK output bit */
} PadButtonMapping;

#define PAD_REMAP_COUNT 16
static PadButtonMapping s_btn_remap[2][PAD_REMAP_COUNT] = {
    /* Player 1 — default mapping */
    {
        { CELL_PAD_CTRL_UP, 0, SWK_UP },
        { CELL_PAD_CTRL_DOWN, 0, SWK_DOWN },
        { CELL_PAD_CTRL_LEFT, 0, SWK_LEFT },
        { CELL_PAD_CTRL_RIGHT, 0, SWK_RIGHT },
        { CELL_PAD_CTRL_CROSS, 1, SWK_SOUTH },
        { CELL_PAD_CTRL_CIRCLE, 1, SWK_EAST },
        { CELL_PAD_CTRL_SQUARE, 1, SWK_WEST },
        { CELL_PAD_CTRL_TRIANGLE, 1, SWK_NORTH },
        { CELL_PAD_CTRL_L1, 1, SWK_LEFT_SHOULDER },
        { CELL_PAD_CTRL_R1, 1, SWK_RIGHT_SHOULDER },
        { CELL_PAD_CTRL_L2, 1, SWK_LEFT_TRIGGER },
        { CELL_PAD_CTRL_R2, 1, SWK_RIGHT_TRIGGER },
        { CELL_PAD_CTRL_L3, 0, SWK_LEFT_STICK },
        { CELL_PAD_CTRL_R3, 0, SWK_RIGHT_STICK },
        { CELL_PAD_CTRL_START, 0, SWK_START },
        { CELL_PAD_CTRL_SELECT, 0, SWK_BACK },
    },
    /* Player 2 — same default mapping */
    {
        { CELL_PAD_CTRL_UP, 0, SWK_UP },
        { CELL_PAD_CTRL_DOWN, 0, SWK_DOWN },
        { CELL_PAD_CTRL_LEFT, 0, SWK_LEFT },
        { CELL_PAD_CTRL_RIGHT, 0, SWK_RIGHT },
        { CELL_PAD_CTRL_CROSS, 1, SWK_SOUTH },
        { CELL_PAD_CTRL_CIRCLE, 1, SWK_EAST },
        { CELL_PAD_CTRL_SQUARE, 1, SWK_WEST },
        { CELL_PAD_CTRL_TRIANGLE, 1, SWK_NORTH },
        { CELL_PAD_CTRL_L1, 1, SWK_LEFT_SHOULDER },
        { CELL_PAD_CTRL_R1, 1, SWK_RIGHT_SHOULDER },
        { CELL_PAD_CTRL_L2, 1, SWK_LEFT_TRIGGER },
        { CELL_PAD_CTRL_R2, 1, SWK_RIGHT_TRIGGER },
        { CELL_PAD_CTRL_L3, 0, SWK_LEFT_STICK },
        { CELL_PAD_CTRL_R3, 0, SWK_RIGHT_STICK },
        { CELL_PAD_CTRL_START, 0, SWK_START },
        { CELL_PAD_CTRL_SELECT, 0, SWK_BACK },
    }
};

/* P8: Allow save system to override button mapping at runtime */
void PS3Pad_SetButtonMap(int player, int remap_idx, uint16_t cell_mask, uint8_t is_d2, uint32_t swk_bit) {
    if (player < 0 || player > 1)
        return;
    if (remap_idx < 0 || remap_idx >= PAD_REMAP_COUNT)
        return;
    s_btn_remap[player][remap_idx].cell_mask = cell_mask;
    s_btn_remap[player][remap_idx].is_d2 = is_d2;
    s_btn_remap[player][remap_idx].swk_bit = swk_bit;
}

void flPADConfigSetACRtoXX(s32 padnum, s16 a, s16 b, s16 c) {
    (void)padnum;
    (void)a;
    (void)b;
    (void)c;
}

s32 tarPADInit(void) {
    memset(tarpad_root, 0, sizeof(tarpad_root));
    memset(ps2slot, 0, sizeof(ps2slot));
    for (int i = 0; i < 2; i++) {
        ps2slot[i].state = 1;
        ps2slot[i].port = i;
    }
    return 1;
}

void tarPADDestroy(void) {}

void tarPADRead(void) {
    int pad_out = 0;

    for (int p = 0; p < 7 && pad_out < 2; p++) {
        CellPadData pad_data;
        if (cellPadGetData(p, &pad_data) == CELL_PAD_OK && pad_data.len > 0) {
            u32 io = 0;

            uint8_t d1 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL1];
            uint8_t d2 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2];

            /* P8: Use remap table instead of hardcoded if/else chain */
            for (int b = 0; b < PAD_REMAP_COUNT; b++) {
                PadButtonMapping* m = &s_btn_remap[pad_out][b];
                uint8_t src = m->is_d2 ? d2 : d1;
                if (src & m->cell_mask) {
                    io |= m->swk_bit;
                }
            }

            tarpad_root[pad_out].sw = io;

            if (pad_data.len >= 8) {
                // M-05 Audit Fix: Match PS2 TARPAD convention: stick[0] = left, stick[1] = right
                tarpad_root[pad_out].stick[0].x = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] - 128;
                tarpad_root[pad_out].stick[0].y = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] - 128;
                tarpad_root[pad_out].stick[1].x = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] - 128;
                tarpad_root[pad_out].stick[1].y = pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] - 128;
            }

            // Mark as connected and ready
            tarpad_root[pad_out].state = 1;
            tarpad_root[pad_out].conn.port = pad_out;
            tarpad_root[pad_out].kind = 1;

            pad_out++;
        }
    }

    // Explicitly disconnect any remaining engine slots (no phantom inputs)
    for (; pad_out < 2; pad_out++) {
        tarpad_root[pad_out].sw = 0;
        tarpad_root[pad_out].stick[0].x = 0;
        tarpad_root[pad_out].stick[0].y = 0;
        tarpad_root[pad_out].stick[1].x = 0;
        tarpad_root[pad_out].stick[1].y = 0;
        tarpad_root[pad_out].state = 0; // KEY FIX: Actually tell the engine it's disconnected
        tarpad_root[pad_out].kind = 0;
    }
}
