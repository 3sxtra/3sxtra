/**
 * @file sdl_pad.c
 * @brief SDL3 gamepad and keyboard input handling.
 *
 * Manages SDL gamepad/keyboard devices, processes input events (button,
 * axis, hat), and maintains per-device state arrays for both digital
 * buttons and analog axes. Supports up to 4 input sources and provides
 * rumble passthrough.
 */
#include "port/sdl/sdl_pad.h"
#include "port/input_definition.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SOURCES_MAX 4

typedef enum SDLPad_InputType {
    SDLPAD_INPUT_NONE = 0,
    SDLPAD_INPUT_GAMEPAD,
    SDLPAD_INPUT_KEYBOARD,
    SDLPAD_INPUT_JOYSTICK
} SDLPad_InputType;

typedef struct SDLPad_GamepadInputSource {
    Uint32 type;
    SDL_Gamepad* gamepad;
} SDLPad_GamepadInputSource;

typedef struct SDLPad_KeyboardInputSource {
    Uint32 type;
} SDLPad_KeyboardInputSource;

typedef struct SDLPad_JoystickInputSource {
    Uint32 type;
    SDL_Joystick* joystick;
} SDLPad_JoystickInputSource;

typedef union SDLPad_InputSource {
    Uint32 type;
    SDLPad_GamepadInputSource gamepad;
    SDLPad_KeyboardInputSource keyboard;
    SDLPad_JoystickInputSource joystick;
} SDLPad_InputSource;

static SDLPad_InputSource input_sources[INPUT_SOURCES_MAX] = { 0 };
static int connected_input_sources = 0;
static int keyboard_index = -1;
static SDLPad_ButtonState button_state[INPUT_SOURCES_MAX] = { 0 };
static SDLPad_ButtonState prev_button_state[INPUT_SOURCES_MAX] = { 0 };
static int last_scancode[INPUT_SOURCES_MAX] = { 0 };
static int last_joy_input[INPUT_SOURCES_MAX] = { 0 };

static int input_source_index_from_joystick_id(SDL_JoystickID id) {
    for (int i = 0; i < INPUT_SOURCES_MAX; i++) {
        const SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_GAMEPAD) {
            if (SDL_GetGamepadID(input_source->gamepad.gamepad) == id) {
                return i;
            }
        } else if (input_source->type == SDLPAD_INPUT_JOYSTICK) {
            if (SDL_GetJoystickID(input_source->joystick.joystick) == id) {
                return i;
            }
        }
    }

    return -1;
}

static void setup_keyboard() {
    if (keyboard_index >= 0) {
        return;
    }

    for (int i = 0; i < SDL_arraysize(input_sources); i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_NONE) {
            input_source->type = SDLPAD_INPUT_KEYBOARD;
            keyboard_index = i;
            connected_input_sources += 1;
            break;
        }
    }
}

static void remove_keyboard() {
    if (keyboard_index < 0) {
        return;
    }

    for (int i = 0; i < SDL_arraysize(input_sources); i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_KEYBOARD) {
            input_source->type = SDLPAD_INPUT_NONE;
            keyboard_index = -1;
            connected_input_sources -= 1;
            break;
        }
    }
}

static void handle_gamepad_added_event(SDL_GamepadDeviceEvent* event) {
    // Check if this device is really a gamepad or just recognized as one
    const char* device_name = SDL_GetGamepadNameForID(event->which);
    bool is_actually_gamepad = SDL_IsGamepad(event->which);
    SDL_Log("Gamepad added event: device_id=%d, name=%s, is_gamepad=%d",
            event->which,
            device_name ? device_name : "Unknown",
            is_actually_gamepad);

    // Remove keyboard to potentially make space for the new gamepad
    remove_keyboard();

    if (connected_input_sources >= INPUT_SOURCES_MAX) {
        return;
    }

    const SDL_Gamepad* gamepad = SDL_OpenGamepad(event->which);
    if (!gamepad) {
        SDL_Log("Failed to open gamepad device_id=%d", event->which);
        return;
    }

    for (int i = 0; i < INPUT_SOURCES_MAX; i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type != SDLPAD_INPUT_NONE) {
            continue;
        }

        input_source->type = SDLPAD_INPUT_GAMEPAD;
        input_source->gamepad.gamepad = gamepad;
        SDL_Log("Opened gamepad at input_source index %d", i);
        break;
    }

    connected_input_sources += 1;

    // Setup keyboard again, if there's a free slot
    setup_keyboard();
}

static void handle_gamepad_removed_event(SDL_GamepadDeviceEvent* event) {
    const int index = input_source_index_from_joystick_id(event->which);

    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_GAMEPAD) {
        return;
    }

    SDLPad_InputSource* input_source = &input_sources[index];
    SDL_CloseGamepad(input_source->gamepad.gamepad);
    input_source->type = SDLPAD_INPUT_NONE;
    memset(&button_state[index], 0, sizeof(SDLPad_ButtonState));
    connected_input_sources -= 1;

    // Setup keyboard in the newly freed slot
    setup_keyboard();
}

void SDLPad_Init() {
    setup_keyboard();
}

void SDLPad_HandleGamepadDeviceEvent(SDL_GamepadDeviceEvent* event) {
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        handle_gamepad_added_event(event);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        handle_gamepad_removed_event(event);
        break;

    default:
        // Do nothing
        break;
    }
}

void SDLPad_HandleJoystickDeviceEvent(SDL_JoyDeviceEvent* event) {
    if (event->type == SDL_EVENT_JOYSTICK_ADDED) {
        if (SDL_IsGamepad(event->which))
            return;

        if (input_source_index_from_joystick_id(event->which) >= 0)
            return;

        remove_keyboard();
        if (connected_input_sources >= INPUT_SOURCES_MAX)
            return;

        SDL_Joystick* joystick = SDL_OpenJoystick(event->which);
        if (!joystick)
            return;

        for (int i = 0; i < INPUT_SOURCES_MAX; i++) {
            if (input_sources[i].type == SDLPAD_INPUT_NONE) {
                input_sources[i].type = SDLPAD_INPUT_JOYSTICK;
                input_sources[i].joystick.joystick = joystick;
                break;
            }
        }
        connected_input_sources++;
        setup_keyboard();
    } else if (event->type == SDL_EVENT_JOYSTICK_REMOVED) {
        int index = input_source_index_from_joystick_id(event->which);
        if (index >= 0 && input_sources[index].type == SDLPAD_INPUT_JOYSTICK) {
            SDL_CloseJoystick(input_sources[index].joystick.joystick);
            input_sources[index].type = SDLPAD_INPUT_NONE;
            connected_input_sources--;
            setup_keyboard();
        }
    }
}

void SDLPad_HandleJoystickButtonEvent(SDL_JoyButtonEvent* event) {
    int index = input_source_index_from_joystick_id(event->which);
    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_JOYSTICK)
        return;

    if (event->down) {
        last_joy_input[index] = INPUT_ID_JOY_BTN_BASE + event->button;
    } else {
        // Clear stale value on release so it isn't re-read after button is no longer held
        if (last_joy_input[index] == INPUT_ID_JOY_BTN_BASE + event->button) {
            last_joy_input[index] = 0;
        }
    }
}

void SDLPad_HandleJoystickAxisEvent(SDL_JoyAxisEvent* event) {
    int index = input_source_index_from_joystick_id(event->which);
    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_JOYSTICK)
        return;

    if (abs(event->value) > 24000) { // Threshold
        int axis_idx = event->axis * 2 + (event->value < 0 ? 1 : 0);
        last_joy_input[index] = INPUT_ID_JOY_AXIS_BASE + axis_idx;
    }
}

void SDLPad_HandleJoystickHatEvent(SDL_JoyHatEvent* event) {
    int index = input_source_index_from_joystick_id(event->which);
    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_JOYSTICK)
        return;

    if (event->value != SDL_HAT_CENTERED) {
        int hat_idx = event->hat * 4;
        if (event->value & SDL_HAT_UP)
            hat_idx += 0;
        else if (event->value & SDL_HAT_RIGHT)
            hat_idx += 1;
        else if (event->value & SDL_HAT_DOWN)
            hat_idx += 2;
        else if (event->value & SDL_HAT_LEFT)
            hat_idx += 3;
        last_joy_input[index] = INPUT_ID_JOY_HAT_BASE + hat_idx;
    }
}

void SDLPad_HandleGamepadButtonEvent(SDL_GamepadButtonEvent* event) {
    const int index = input_source_index_from_joystick_id(event->which);

    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_GAMEPAD) {
        return;
    }

    SDLPad_ButtonState* state = &button_state[index];

    switch (event->button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        state->south = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_EAST:
        state->east = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_WEST:
        state->west = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_NORTH:
        state->north = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_BACK:
        state->back = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_START:
        state->start = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        state->left_stick = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        state->right_stick = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        state->left_shoulder = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        state->right_shoulder = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        state->dpad_up = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        state->dpad_down = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        state->dpad_left = event->down;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        state->dpad_right = event->down;
        break;
    }
}

void SDLPad_HandleGamepadAxisMotionEvent(SDL_GamepadAxisEvent* event) {
    const int index = input_source_index_from_joystick_id(event->which);

    if (index < 0 || input_sources[index].type != SDLPAD_INPUT_GAMEPAD) {
        return;
    }

    SDLPad_ButtonState* state = &button_state[index];

    switch (event->axis) {
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        state->left_trigger = event->value;
        break;

    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        state->right_trigger = event->value;
        break;

    case SDL_GAMEPAD_AXIS_LEFTX:
        state->left_stick_x = event->value;
        break;

    case SDL_GAMEPAD_AXIS_LEFTY:
        state->left_stick_y = event->value;
        break;

    case SDL_GAMEPAD_AXIS_RIGHTX:
        state->right_stick_x = event->value;
        break;

    case SDL_GAMEPAD_AXIS_RIGHTY:
        state->right_stick_y = event->value;
        break;
    }
}

void SDLPad_HandleKeyboardEvent(SDL_KeyboardEvent* event) {
    if (keyboard_index < 0) {
        return;
    }

    // Skip all F-keys that are reserved for overlay toggles:
    // F1=Controller Setup, F2=Shader Menu, F3=Mods Menu, F4=Shader Mode,
    // F5=Frame Rate Uncap, F6=Stage Config, F7=Training Menu,
    // F8=Scale Mode, F9=Preset Cycle, F11=Fullscreen
    switch (event->key) {
    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F11:
        return;
    default:
        break;
    }

    if (event->down) {
        last_scancode[keyboard_index] = event->scancode;
    } else {
        if (last_scancode[keyboard_index] == event->scancode) {
            last_scancode[keyboard_index] = 0;
        }
    }

    SDLPad_ButtonState* state = &button_state[keyboard_index];

    switch (event->key) {
    case SDLK_W:
        state->dpad_up = event->down;
        break;

    case SDLK_A:
        state->dpad_left = event->down;
        break;

    case SDLK_S:
        state->dpad_down = event->down;
        break;

    case SDLK_D:
        state->dpad_right = event->down;
        break;

    case SDLK_I:
        state->north = event->down;
        break;

    case SDLK_J:
        state->south = event->down;
        break;

    case SDLK_K:
        state->east = event->down;
        break;

    case SDLK_U:
        state->west = event->down;
        break;

    case SDLK_P:
        state->left_shoulder = event->down;
        break;

    case SDLK_O:
        state->right_shoulder = event->down;
        break;

    case SDLK_SEMICOLON:
        state->left_trigger = event->down ? SDL_MAX_SINT16 : 0;
        break;

    case SDLK_L:
        state->right_trigger = event->down ? SDL_MAX_SINT16 : 0;
        break;

    case SDLK_9:
        state->left_stick = event->down;
        break;

    case SDLK_0:
        state->right_stick = event->down;
        break;

    case SDLK_BACKSPACE:
        state->back = event->down;
        break;

    case SDLK_RETURN:
        state->start = event->down;
        break;
    }
}

bool SDLPad_IsGamepadConnected(int id) {
    if (id < 0 || id >= INPUT_SOURCES_MAX)
        return false;
    return input_sources[id].type != SDLPAD_INPUT_NONE;
}

void SDLPad_GetButtonState(int id, SDLPad_ButtonState* state) {
    if (id < 0 || id >= INPUT_SOURCES_MAX || !state) {
        return;
    }
    memcpy(state, &button_state[id], sizeof(SDLPad_ButtonState));
}

void SDLPad_RumblePad(int id, bool low_freq_enabled, Uint8 high_freq_rumble) {
    const SDLPad_InputSource* input_source = &input_sources[id];

    if (input_source->type != SDLPAD_INPUT_GAMEPAD) {
        return;
    }

    const Uint16 low_freq_rumble = low_freq_enabled ? UINT16_MAX : 0;
    const Uint16 high_freq_rumble_adjusted = ((float)high_freq_rumble / UINT8_MAX) * UINT16_MAX;
    const Uint32 duration = high_freq_rumble_adjusted > 0 ? 500 : 200;

    SDL_RumbleGamepad(input_source->gamepad.gamepad, low_freq_rumble, high_freq_rumble_adjusted, duration);
}

int SDLPad_GetNumConnectedDevices() {
    return connected_input_sources;
}

int SDLPad_GetMaxDevices() {
    return INPUT_SOURCES_MAX;
}

bool SDLPad_GetDeviceGUID(int index, char* out_guid, int guid_len) {
    if (index < 0 || index >= INPUT_SOURCES_MAX || !out_guid || guid_len <= 0) {
        return false;
    }

    const SDLPad_InputSource* input_source = &input_sources[index];
    SDL_GUID guid;
    char guid_str[33];

    switch (input_source->type) {
    case SDLPAD_INPUT_GAMEPAD: {
        SDL_JoystickID id = SDL_GetGamepadID(input_source->gamepad.gamepad);
        guid = SDL_GetGamepadGUIDForID(id);
        SDL_GUIDToString(guid, guid_str, sizeof(guid_str));
        snprintf(out_guid, guid_len, "%s", guid_str);
        return true;
    }
    case SDLPAD_INPUT_JOYSTICK:
        guid = SDL_GetJoystickGUID(input_source->joystick.joystick);
        SDL_GUIDToString(guid, guid_str, sizeof(guid_str));
        snprintf(out_guid, guid_len, "%s", guid_str);
        return true;
    case SDLPAD_INPUT_KEYBOARD:
        snprintf(out_guid, guid_len, "KEYBOARD_DEVICE");
        return true;
    default:
        return false;
    }
}

const char* SDLPad_GetDeviceName(int index) {
    if (index < 0 || index >= INPUT_SOURCES_MAX) {
        return "Invalid Index";
    }

    const SDLPad_InputSource* input_source = &input_sources[index];

    switch (input_source->type) {
    case SDLPAD_INPUT_GAMEPAD:
        return SDL_GetGamepadName(input_source->gamepad.gamepad);
    case SDLPAD_INPUT_JOYSTICK:
        return SDL_GetJoystickName(input_source->joystick.joystick);
    case SDLPAD_INPUT_KEYBOARD:
        return "Keyboard";
    default:
        return "Disconnected";
    }
}

int SDLPad_GetKeyboardDeviceIndex() {
    return keyboard_index;
}

#define CHECK_BUTTON(field, name)                                                                                      \
    if (current_state->field && !prev_state->field) {                                                                  \
        snprintf(out_button_name, name_len, name);                                                                     \
        return true;                                                                                                   \
    }
#define CHECK_AXIS(field, name_pos, name_neg, threshold)                                                               \
    if (current_state->field > threshold && prev_state->field <= threshold) {                                          \
        snprintf(out_button_name, name_len, name_pos);                                                                 \
        return true;                                                                                                   \
    }                                                                                                                  \
    if (current_state->field < -threshold && prev_state->field >= -threshold) {                                        \
        snprintf(out_button_name, name_len, name_neg);                                                                 \
        return true;                                                                                                   \
    }

bool SDLPad_GetLastInput(int device_index, char* out_button_name, int name_len) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;

    SDLPad_ButtonState* current_state = &button_state[device_index];
    SDLPad_ButtonState* prev_state = &prev_button_state[device_index];

    CHECK_BUTTON(dpad_up, "DPad Up");
    CHECK_BUTTON(dpad_down, "DPad Down");
    CHECK_BUTTON(dpad_left, "DPad Left");
    CHECK_BUTTON(dpad_right, "DPad Right");
    CHECK_BUTTON(start, "Start");
    CHECK_BUTTON(back, "Back");
    CHECK_BUTTON(left_stick, "Left Stick");
    CHECK_BUTTON(right_stick, "Right Stick");
    CHECK_BUTTON(left_shoulder, "Left Shoulder");
    CHECK_BUTTON(right_shoulder, "Right Shoulder");
    CHECK_BUTTON(south, "Button South");
    CHECK_BUTTON(east, "Button East");
    CHECK_BUTTON(west, "Button West");
    CHECK_BUTTON(north, "Button North");

    CHECK_AXIS(left_trigger, "Left Trigger", "Left Trigger", 8000);
    CHECK_AXIS(right_trigger, "Right Trigger", "Right Trigger", 8000);
    CHECK_AXIS(left_stick_x, "Left Stick X+", "Left Stick X-", 8000);
    CHECK_AXIS(left_stick_y, "Left Stick Y+", "Left Stick Y-", 8000);
    CHECK_AXIS(right_stick_x, "Right Stick X+", "Right Stick X-", 8000);
    CHECK_AXIS(right_stick_y, "Right Stick Y+", "Right Stick Y-", 8000);

    return false;
}

void SDLPad_UpdatePreviousState() {
    memcpy(prev_button_state, button_state, sizeof(button_state));
}

void SDLPad_UpdatePreviousStateForDevice(int device_index) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX) {
        return;
    }
    memcpy(&prev_button_state[device_index], &button_state[device_index], sizeof(SDLPad_ButtonState));
}

bool SDLPad_IsAnyInputActive(int device_index) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX) {
        return false;
    }

    if (input_sources[device_index].type == SDLPAD_INPUT_KEYBOARD) {
        int num_keys;
        const bool* key_state = SDL_GetKeyboardState(&num_keys);
        for (int i = 0; i < num_keys; i++) {
            if (key_state[i])
                return true;
        }
    } else if (input_sources[device_index].type == SDLPAD_INPUT_JOYSTICK) {
        // TODO: Check generic joystick active inputs if needed?
        // Currently ioconv polls, but Mapping UI waits for release.
        // We can check buttons at least.
        SDL_Joystick* joy = input_sources[device_index].joystick.joystick;
        int buttons = SDL_GetNumJoystickButtons(joy);
        for (int i = 0; i < buttons; i++) {
            if (SDL_GetJoystickButton(joy, i))
                return true;
        }
        int axes = SDL_GetNumJoystickAxes(joy);
        for (int i = 0; i < axes; i++) {
            if (abs(SDL_GetJoystickAxis(joy, i)) > 20000)
                return true;
        }
    }

    const SDLPad_ButtonState* state = &button_state[device_index];

    // Check digital buttons
    if (state->dpad_up || state->dpad_down || state->dpad_left || state->dpad_right || state->start || state->back ||
        state->left_stick || state->right_stick || state->left_shoulder || state->right_shoulder || state->south ||
        state->east || state->west || state->north) {
        return true;
    }

    // Check triggers with a threshold
    const Sint16 trigger_threshold = 8000;
    if (state->left_trigger > trigger_threshold || state->right_trigger > trigger_threshold) {
        return true;
    }

    // Check analog sticks with a threshold
    const Sint16 stick_threshold = 8000;
    if (state->left_stick_x > stick_threshold || state->left_stick_x < -stick_threshold ||
        state->left_stick_y > stick_threshold || state->left_stick_y < -stick_threshold ||
        state->right_stick_x > stick_threshold || state->right_stick_x < -stick_threshold ||
        state->right_stick_y > stick_threshold || state->right_stick_y < -stick_threshold) {
        return true;
    }

    return false;
}

bool SDLPad_IsKeyboard(int device_index) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    return input_sources[device_index].type == SDLPAD_INPUT_KEYBOARD;
}

bool SDLPad_IsJoystick(int device_index) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    return input_sources[device_index].type == SDLPAD_INPUT_JOYSTICK;
}

bool SDLPad_GetLastScancode(int device_index, int* out_scancode) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    if (input_sources[device_index].type != SDLPAD_INPUT_KEYBOARD)
        return false;

    if (last_scancode[device_index] != 0 && last_scancode[device_index] != SDL_SCANCODE_UNKNOWN) {
        if (out_scancode)
            *out_scancode = last_scancode[device_index];
        return true;
    }
    return false;
}

bool SDLPad_GetLastJoystickInput(int device_index, int* out_input_id) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    if (input_sources[device_index].type != SDLPAD_INPUT_JOYSTICK)
        return false;

    if (last_joy_input[device_index] != 0) {
        if (out_input_id)
            *out_input_id = last_joy_input[device_index];
        // Clear it after reading? UI reads it once to map.
        // If we clear it, next frame returns false.
        // This mimics 'down' event.
        last_joy_input[device_index] = 0;
        return true;
    }
    return false;
}

bool SDLPad_GetJoystickButton(int device_index, int button) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    if (input_sources[device_index].type != SDLPAD_INPUT_JOYSTICK)
        return false;
    return SDL_GetJoystickButton(input_sources[device_index].joystick.joystick, button);
}

bool SDLPad_GetJoystickAxis(int device_index, int axis, int sign) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    if (input_sources[device_index].type != SDLPAD_INPUT_JOYSTICK)
        return false;
    Sint16 val = SDL_GetJoystickAxis(input_sources[device_index].joystick.joystick, axis);
    if (sign == 0)
        return val > 16000; // +
    else
        return val < -16000; // -
}

bool SDLPad_GetJoystickHat(int device_index, int hat, int dir) {
    if (device_index < 0 || device_index >= INPUT_SOURCES_MAX)
        return false;
    if (input_sources[device_index].type != SDLPAD_INPUT_JOYSTICK)
        return false;
    Uint8 val = SDL_GetJoystickHat(input_sources[device_index].joystick.joystick, hat);
    if (dir == 0)
        return val & SDL_HAT_UP;
    if (dir == 1)
        return val & SDL_HAT_RIGHT;
    if (dir == 2)
        return val & SDL_HAT_DOWN;
    if (dir == 3)
        return val & SDL_HAT_LEFT;
    return false;
}

void SDLPad_ClearKeyboardState() {
    if (keyboard_index >= 0 && keyboard_index < INPUT_SOURCES_MAX) {
        last_scancode[keyboard_index] = 0;
        memset(&button_state[keyboard_index], 0, sizeof(SDLPad_ButtonState));
    }
}
