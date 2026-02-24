/**
 * @file ioconv.c
 * @brief Input conversion and controller processing.
 *
 * Converts raw controller inputs to game-level button/lever states.
 *
 * Part of the io module.
 */

#include "sf33rd/Source/Game/io/ioconv.h"
#include "common.h"
#include "main.h"
#include "port/input_definition.h"
#include "port/sdl/control_mapping_bindings.h"
#include "port/sdl/sdl_pad.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/work_sys.h"

IO io_w;

u32 ioconv_table[24][2] = { { 0x1, 0x1 },
                            { 0x2, 0x2 },
                            { 0x4, 0x4 },
                            { 0x8, 0x8 },
                            { 0x100, 0x10 },
                            { 0x200, 0x20 },
                            { 0x400, 0x40 },
                            { 0x800, 0x80 },
                            { 0x10, 0x100 },
                            { 0x20, 0x200 },
                            { 0x40, 0x400 },
                            { 0x80, 0x800 },
                            { 0x2000, 0x1000 },
                            { 0x1000, 0x2000 },
                            { 0x8000, 0x4000 },
                            { 0x4000, 0x8000 },
                            /* Entries 16-23: PS2 analog-to-digital slots — unused in the mapped input path */
                            { 0x0, 0x10000 },
                            { 0x0, 0x20000 },
                            { 0x0, 0x40000 },
                            { 0x0, 0x80000 },
                            { 0x0, 0x100000 },
                            { 0x0, 0x200000 },
                            { 0x0, 0x400000 },
                            { 0x0, 0x800000 } };

const char* game_actions[] = { "Up",         "Down",       "Left",        "Right",     "Light Punch", "Medium Punch",
                               "Hard Punch", "Light Kick", "Medium Kick", "Hard Kick", "Start",       "Select" };

/** @brief Return the number of mappable game actions. */
s32 get_game_actions_count() {
    return sizeof(game_actions) / sizeof(game_actions[0]);
}

/** @brief Map an action name string to its corresponding button flag. */
u32 get_action_flag(const char* action) {
    if (strcmp(action, "Up") == 0)
        return 0x1;
    if (strcmp(action, "Down") == 0)
        return 0x2;
    if (strcmp(action, "Left") == 0)
        return 0x4;
    if (strcmp(action, "Right") == 0)
        return 0x8;
    if (strcmp(action, "Light Punch") == 0)
        return 0x10;
    if (strcmp(action, "Medium Punch") == 0)
        return 0x20;
    if (strcmp(action, "Hard Punch") == 0)
        return 0x40;
    if (strcmp(action, "Light Kick") == 0)
        return 0x100;
    if (strcmp(action, "Medium Kick") == 0)
        return 0x200;
    if (strcmp(action, "Hard Kick") == 0)
        return 0x400;
    if (strcmp(action, "Start") == 0)
        return 0x1000;
    if (strcmp(action, "Select") == 0)
        return 0x2000;
    return 0;
}

/** @brief Test whether a given input ID is active in the button state. */
static bool is_input_active(SDLPad_ButtonState* state, InputID input_id) {
    switch (input_id) {
    case INPUT_ID_DPAD_UP:
        return state->dpad_up;
    case INPUT_ID_DPAD_DOWN:
        return state->dpad_down;
    case INPUT_ID_DPAD_LEFT:
        return state->dpad_left;
    case INPUT_ID_DPAD_RIGHT:
        return state->dpad_right;
    case INPUT_ID_START:
        return state->start;
    case INPUT_ID_BACK:
        return state->back;
    case INPUT_ID_LEFT_STICK:
        return state->left_stick;
    case INPUT_ID_RIGHT_STICK:
        return state->right_stick;
    case INPUT_ID_LEFT_SHOULDER:
        return state->left_shoulder;
    case INPUT_ID_RIGHT_SHOULDER:
        return state->right_shoulder;
    case INPUT_ID_BUTTON_SOUTH:
        return state->south;
    case INPUT_ID_BUTTON_EAST:
        return state->east;
    case INPUT_ID_BUTTON_WEST:
        return state->west;
    case INPUT_ID_BUTTON_NORTH:
        return state->north;
    case INPUT_ID_LEFT_TRIGGER:
        return state->left_trigger > 8000;
    case INPUT_ID_RIGHT_TRIGGER:
        return state->right_trigger > 8000;
    case INPUT_ID_LEFT_STICK_X_PLUS:
        return state->left_stick_x > 8000;
    case INPUT_ID_LEFT_STICK_X_MINUS:
        return state->left_stick_x < -8000;
    case INPUT_ID_LEFT_STICK_Y_PLUS:
        return state->left_stick_y > 8000;
    case INPUT_ID_LEFT_STICK_Y_MINUS:
        return state->left_stick_y < -8000;
    case INPUT_ID_RIGHT_STICK_X_PLUS:
        return state->right_stick_x > 8000;
    case INPUT_ID_RIGHT_STICK_X_MINUS:
        return state->right_stick_x < -8000;
    case INPUT_ID_RIGHT_STICK_Y_PLUS:
        return state->right_stick_y > 8000;
    case INPUT_ID_RIGHT_STICK_Y_MINUS:
        return state->right_stick_y < -8000;
    default:
        return false;
    }
}

/** @brief Convert raw pad states into game I/O words (per frame). */
void keyConvert() {
    IOPad* pad;
    u32 currSw;
    s32 i;
    s32 j;
    s32 repeat_on = 0;

    if (Debug_w[DEBUG_AUTO_RAPID_SHOT] && mpp_w.inGame && (Game_pause == 0)) {
        repeat_on = 1;
    }

    if ((save_w[Present_Mode].extra_option.contents[0][4]) && mpp_w.inGame && (Game_pause == 0)) {
        repeat_on = 1;

        if ((task[TASK_MENU].condition == 1) && (task[TASK_MENU].r_no[0] != 10)) {
            repeat_on = 0;
        }
    }

    for (i = 0; i < 2; i++) {
        s32 device_id = ControlMapping_GetPlayerDeviceID(i + 1);
        if (device_id != -1) {
            // --- F1 Mapped Device Path ---
            // When the player has a device assigned via the F1 Controller Setup menu,
            // we read each game action directly from the SDL layer using the stored
            // InputID mappings.  This path bypasses the legacy flPAD-based conversion
            // (ioconv_table) and the in-game Button Config screen (Pad_Infor), so
            // changes made in the in-game options will have no effect for this player
            // while F1 mappings are active.
            bool is_keyboard = SDLPad_IsKeyboard(device_id);
            bool is_joystick = SDLPad_IsJoystick(device_id);
            SDLPad_ButtonState state;
            // Always get state for now (used for virtual mappings fallback)
            SDLPad_GetButtonState(device_id, &state);

            const bool* key_state = NULL;
            s32 num_keys = 0;
            if (is_keyboard) {
                key_state = SDL_GetKeyboardState(&num_keys);
            }

            // Set Interface_Type to indicate controller is connected
            // 0 = disconnected, 2 = controller connected
            Interface_Type[i] = 2;

            // Log device info once per player per session
            static bool logged_device_info[2] = { false, false };
            if (!logged_device_info[i]) {
                SDL_Log("P%d device_id=%d, is_keyboard=%d, is_joystick=%d", i + 1, device_id, is_keyboard, is_joystick);
                logged_device_info[i] = true;
            }

            io_w.sw[i] = 0;
            u32 hw_sw = 0;

            for (s32 k = 0; k < sizeof(game_actions) / sizeof(game_actions[0]); ++k) {
                const char* action = game_actions[k];
                InputID input_id = (InputID)ControlMapping_GetPlayerMapping(i + 1, action);

                bool active = false;
                if (input_id != INPUT_ID_UNKNOWN) {
                    if (is_keyboard && is_keyboard_input(input_id)) {
                        s32 scancode = (s32)input_id - INPUT_ID_KEY_BASE;
                        if (scancode >= 0 && scancode < num_keys) {
                            active = key_state[scancode];
                        }
                    } else if (is_joystick && is_joystick_input(input_id)) {
                        s32 val = (s32)input_id;
                        if (val >= INPUT_ID_JOY_HAT_BASE) {
                            s32 hat = (val - INPUT_ID_JOY_HAT_BASE) / 4;
                            s32 dir = (val - INPUT_ID_JOY_HAT_BASE) % 4;
                            active = SDLPad_GetJoystickHat(device_id, hat, dir);
                        } else if (val >= INPUT_ID_JOY_AXIS_BASE) {
                            s32 axis = (val - INPUT_ID_JOY_AXIS_BASE) / 2;
                            s32 sign = (val - INPUT_ID_JOY_AXIS_BASE) % 2;
                            active = SDLPad_GetJoystickAxis(device_id, axis, sign);
                        } else if (val >= INPUT_ID_JOY_BTN_BASE) {
                            s32 btn = val - INPUT_ID_JOY_BTN_BASE;
                            active = SDLPad_GetJoystickButton(device_id, btn);
                        }
                    } else {
                        // Check virtual button state (works for Gamepad AND legacy Keyboard mappings)
                        active = is_input_active(&state, input_id);
                    }
                }

                if (active) {
                    u32 game_flag = get_action_flag(action);
                    io_w.sw[i] |= game_flag;

                    // Map Game Action -> Standard PS2 HW Bit for menu compatibility
                    switch (game_flag) {
                    case 0x1:
                        hw_sw |= SWK_UP;
                        break;
                    case 0x2:
                        hw_sw |= SWK_DOWN;
                        break;
                    case 0x4:
                        hw_sw |= SWK_LEFT;
                        break;
                    case 0x8:
                        hw_sw |= SWK_RIGHT;
                        break;
                    case 0x10:
                        hw_sw |= SWK_WEST;
                        break; // LP -> Square
                    case 0x20:
                        hw_sw |= SWK_NORTH;
                        break; // MP -> Triangle
                    case 0x40:
                        hw_sw |= SWK_RIGHT_SHOULDER;
                        break; // HP -> R1
                    case 0x100:
                        hw_sw |= SWK_SOUTH;
                        break; // LK -> Cross
                    case 0x200:
                        hw_sw |= SWK_EAST;
                        break; // MK -> Circle
                    case 0x400:
                        hw_sw |= SWK_RIGHT_TRIGGER;
                        break; // HK -> R2
                    case 0x1000:
                        hw_sw |= SWK_START;
                        break; // Start -> Start
                    case 0x2000:
                        hw_sw |= SWK_BACK;
                        break; // Select -> Select
                    }
                }
            }

            // Populate driver pad struct (FLPAD) to reuse game logic (repeat, analog processing)
            FLPAD* drv_pad = &flpad_adr[0][i];

            drv_pad->kind = 0x1; // Digital/DualShock (Connected)
            drv_pad->state = 0;  // Stable

            drv_pad->sw_old = drv_pad->sw;
            drv_pad->sw = hw_sw;
            drv_pad->sw_new = drv_pad->sw & ~drv_pad->sw_old;
            drv_pad->sw_off = ~drv_pad->sw & drv_pad->sw_old;
            drv_pad->sw_chg = drv_pad->sw ^ drv_pad->sw_old;

            if (!is_keyboard) {
                drv_pad->stick[0].x = state.left_stick_x;
                drv_pad->stick[0].y = state.left_stick_y;
                drv_pad->stick[1].x = state.right_stick_x;
                drv_pad->stick[1].y = state.right_stick_y;
                // Update analog derived values (pow, ang, etc)
                flupdate_pad_stick_dir(&drv_pad->stick[0]);
                flupdate_pad_stick_dir(&drv_pad->stick[1]);

                // Promote analog stick to D-pad directions when digital directions are absent
                if (mpp_w.useAnalogStickData) {
                    if (!(hw_sw & 0xF)) {
                        hw_sw |= (drv_pad->sw >> 16) & 0xF;
                    }
                    if (!(hw_sw & 0xF)) {
                        hw_sw |= (drv_pad->sw >> 20) & 0xF;
                    }
                }
            } else {
                // Zero sticks for keyboard
                drv_pad->stick[0].x = 0;
                drv_pad->stick[0].y = 0;
                drv_pad->stick[0].pow = 0;
                drv_pad->stick[0].rad = 0.0f;
                drv_pad->stick[1].x = 0;
                drv_pad->stick[1].y = 0;
                drv_pad->stick[1].pow = 0;
                drv_pad->stick[1].rad = 0.0f;
            }

            // Repeat logic (copied from legacy block)
            flPADSetRepeatSw(drv_pad, 0xFF000F, 15, 3);
            if (repeat_on) {
                flPADSetRepeatSw(drv_pad, 0x3FF0, 2, 1);
            } else {
                flPADSetRepeatSw(drv_pad, 0x3FF0, 10, 2);
            }

            // Copy to IO struct (Game Input)
            pad = &io_w.data[i];
            pad->state = drv_pad->state;
            pad->anstate = drv_pad->anstate;
            pad->kind = drv_pad->kind;
            pad->sw = drv_pad->sw;
            pad->sw_old = drv_pad->sw_old;
            pad->sw_new = drv_pad->sw_new;
            pad->sw_off = drv_pad->sw_off;
            pad->sw_chg = drv_pad->sw_chg;
            pad->sw_repeat = drv_pad->sw_repeat;
            pad->stick[0] = drv_pad->stick[0];
            pad->stick[1] = drv_pad->stick[1];

        } else {
            // Original logic
            flPADSetRepeatSw(&flpad_adr[0][i], 0xFF000F, 15, 3);

            if (repeat_on) {
                flPADSetRepeatSw(&flpad_adr[0][i], 0x3FF0, 2, 1);
            } else {
                flPADSetRepeatSw(&flpad_adr[0][i], 0x3FF0, 10, 2);
            }

            pad = &io_w.data[i];
            pad->state = flpad_adr[0][i].state;
            pad->anstate = flpad_adr[0][i].anstate;
            pad->kind = flpad_adr[0][i].kind;
            pad->sw = flpad_adr[0][i].sw;
            pad->sw_old = flpad_adr[0][i].sw_old;
            pad->sw_new = flpad_adr[0][i].sw_new;
            pad->sw_off = flpad_adr[0][i].sw_off;
            pad->sw_chg = flpad_adr[0][i].sw_chg;
            pad->sw_repeat = flpad_adr[0][i].sw_repeat;
            pad->stick[0] = flpad_adr[0][i].stick[0];
            pad->stick[1] = flpad_adr[0][i].stick[1];

            if (mpp_w.useAnalogStickData) {
                if (!(flpad_adr[0][i].sw & 0xF)) {
                    pad->sw |= (pad->sw >> 16) & 0xF;
                    pad->sw_old |= (pad->sw_old >> 16) & 0xF;
                    pad->sw_new |= (pad->sw_new >> 16) & 0xF;
                    pad->sw_off |= (pad->sw_off >> 16) & 0xF;
                    pad->sw_chg |= (pad->sw_chg >> 16) & 0xF;
                    pad->sw_repeat |= (pad->sw_repeat >> 16) & 0xF;
                }

                if (!(flpad_adr[0][i].sw & 0xF)) {
                    pad->sw |= (pad->sw >> 20) & 0xF;
                    pad->sw_old |= (pad->sw_old >> 20) & 0xF;
                    pad->sw_new |= (pad->sw_new >> 20) & 0xF;
                    pad->sw_off |= (pad->sw_off >> 20) & 0xF;
                    pad->sw_chg |= (pad->sw_chg >> 20) & 0xF;
                    pad->sw_repeat |= (pad->sw_repeat >> 20) & 0xF;
                }
            }

            if (pad->kind == 0 || pad->kind == 0x8000) {
                Interface_Type[i] = 0;
            } else {
                Interface_Type[i] = 2;
            }

            io_w.sw[i] = 0;

            // Block game inputs from being converted when debug menu is active.
            if (debug_menu_active) {
                continue;
            }

            currSw = pad->sw;

            for (j = 0; j < 4; j++) {
                if (currSw & ioconv_table[j][1]) {
                    io_w.sw[i] |= ioconv_table[j][0];
                }
            }

            for (j = 12; j < 16; j++) {
                if (currSw & ioconv_table[j][1]) {
                    io_w.sw[i] |= ioconv_table[j][0];
                }
            }

            if (repeat_on) {
                currSw = pad->sw_repeat;
            }

            for (j = 4; j < 12; j++) {
                if (currSw & ioconv_table[j][1]) {
                    io_w.sw[i] |= ioconv_table[j][0];
                }
            }
        }
    }

    p1sw_buff = io_w.sw[0];
    p2sw_buff = io_w.sw[1];
}
