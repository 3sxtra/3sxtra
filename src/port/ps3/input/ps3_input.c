#include "port/ps3/ps3_sdl_stubs.h"
#include "../../sdl/input/sdl_pad.h"

#include <cell/pad.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

void SDLPad_Init(void) {
    // I-MED-01 Audit Fix: cellPadInit is now centralized in PS3App_FullInit.
    // Nothing to do here — pad is already initialized and pressure is enabled.
}

void SDLPad_HandleGamepadDeviceEvent(SDL_GamepadDeviceEvent* event) {
    (void)event;
    /* PS3 doesn't typically push connection events matching SDL's hotplugging, 
     * the connections are monitored synchronously through cellPadGetData or cellPadGetInfo2. */
}

bool SDLPad_IsGamepadConnected(int id) {
    CellPadInfo2 info;
    if (cellPadGetInfo2(&info) == CELL_PAD_OK) {
        /* Check if the specific id (port) is connected via port_status */
        if (id >= 0 && id < 7) {
            return (info.port_status[id] & CELL_PAD_STATUS_CONNECTED) != 0;
        }
    }
    return false;
}

static Sint16 scale_analog(uint8_t cell_axis) {
    /* PS3 axes are 0-255 (128 center). SDL/Engine expects -32768 to 32767. */
    int value = (int)cell_axis - 128;
    int scaled = value * 256; 

    /* Standard TRC Deadzone (approx 25%) to prevent stick drift */
    const int DEADZONE = 8000;
    if (scaled < DEADZONE && scaled > -DEADZONE) return 0;
    
    /* Scale the value outside the deadzone back to the full range */
    if (scaled >= DEADZONE) {
        scaled = (scaled - DEADZONE) * 32767 / (32767 - DEADZONE);
    } else {
        scaled = (scaled + DEADZONE) * -32768 / (-32768 + DEADZONE);
    }

    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    return (Sint16)scaled;
}

void SDLPad_GetButtonState(int id, SDLPad_ButtonState* state) {
    memset(state, 0, sizeof(*state));

    CellPadData pad_data;
    if (cellPadGetData(id, &pad_data) == CELL_PAD_OK && pad_data.len > 0) {
        /* Digital buttons 1 (offset 2): Select, L3, R3, Start, Up, Right, Down, Left */
        uint8_t d1 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL1];
        /* Digital buttons 2 (offset 3): L2, R2, L1, R1, Triangle, Circle, Cross, Square */
        uint8_t d2 = pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2];

        /* d1 mappings */
        state->back          = (d1 & CELL_PAD_CTRL_SELECT) != 0;
        state->left_stick    = (d1 & CELL_PAD_CTRL_L3) != 0;
        state->right_stick   = (d1 & CELL_PAD_CTRL_R3) != 0;
        state->start         = (d1 & CELL_PAD_CTRL_START) != 0;
        state->dpad_up       = (d1 & CELL_PAD_CTRL_UP) != 0;
        state->dpad_right    = (d1 & CELL_PAD_CTRL_RIGHT) != 0;
        state->dpad_down     = (d1 & CELL_PAD_CTRL_DOWN) != 0;
        state->dpad_left     = (d1 & CELL_PAD_CTRL_LEFT) != 0;

        /* d2 mappings */
        bool l2_pressed = (d2 & CELL_PAD_CTRL_L2) != 0;
        bool r2_pressed = (d2 & CELL_PAD_CTRL_R2) != 0;
        state->left_trigger  = l2_pressed ? 32767 : 0;
        state->right_trigger = r2_pressed ? 32767 : 0;
        
        state->left_shoulder = (d2 & CELL_PAD_CTRL_L1) != 0;
        state->right_shoulder= (d2 & CELL_PAD_CTRL_R1) != 0;
        state->north         = (d2 & CELL_PAD_CTRL_TRIANGLE) != 0;
        state->east          = (d2 & CELL_PAD_CTRL_CIRCLE) != 0;
        state->south         = (d2 & CELL_PAD_CTRL_CROSS) != 0;
        state->west          = (d2 & CELL_PAD_CTRL_SQUARE) != 0;

        /* Precise Analog/Pressure Data (if enough bytes returned) */
        if (pad_data.len >= 8) {
            /* Override digital trigger state with actual pressure if detected */
            uint8_t l2_press = pad_data.button[CELL_PAD_BTN_OFFSET_PRESS_L2];
            uint8_t r2_press = pad_data.button[CELL_PAD_BTN_OFFSET_PRESS_R2];
            
            // I-03 Audit Fix: Full-range scaling (0-255 → 0-32767) instead of *128 (capped at 32640)
            if (l2_press > 0) state->left_trigger  = (Sint16)((l2_press * 32767) / 255);
            if (r2_press > 0) state->right_trigger = (Sint16)((r2_press * 32767) / 255);

            state->right_stick_x = scale_analog(pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X]);
            state->right_stick_y = scale_analog(pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]);
            state->left_stick_x  = scale_analog(pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]);
            state->left_stick_y  = scale_analog(pad_data.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]);
        }
    }
}

void SDLPad_RumblePad(int id, bool low_freq_enabled, Uint8 high_freq_rumble) {
    // I-04 Audit Fix: Implement rumble via cellPadSetActDirect
    // Motor layout: [0]=large motor (low freq), [1]=small motor (high freq), [2-5]=reserved
    CellPadActParam act;
    memset(&act, 0, sizeof(act));
    act.motor[0] = low_freq_enabled ? 1 : 0;      // Large motor: 0=off, 1=on
    act.motor[1] = high_freq_rumble;               // Small motor: 0=off, 1-255=intensity
    cellPadSetActDirect(id, &act);
}
