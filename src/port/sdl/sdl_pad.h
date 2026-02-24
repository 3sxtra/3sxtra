#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct SDLPad_ButtonState {
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;

    bool start;
    bool back;

    bool left_stick;
    bool right_stick;

    bool left_shoulder;
    bool right_shoulder;

    bool south; // A
    bool east;  // B
    bool west;  // X
    bool north; // Y

    Sint16 left_trigger;
    Sint16 right_trigger;

    Sint16 left_stick_x;
    Sint16 left_stick_y;

    Sint16 right_stick_x;
    Sint16 right_stick_y;
} SDLPad_ButtonState;

#ifdef __cplusplus
extern "C" {
#endif

void SDLPad_Init();
void SDLPad_HandleGamepadDeviceEvent(SDL_GamepadDeviceEvent* event);
void SDLPad_HandleGamepadButtonEvent(SDL_GamepadButtonEvent* event);
void SDLPad_HandleGamepadAxisMotionEvent(SDL_GamepadAxisEvent* event);
void SDLPad_HandleJoystickDeviceEvent(SDL_JoyDeviceEvent* event);
void SDLPad_HandleJoystickButtonEvent(SDL_JoyButtonEvent* event);
void SDLPad_HandleJoystickAxisEvent(SDL_JoyAxisEvent* event);
void SDLPad_HandleJoystickHatEvent(SDL_JoyHatEvent* event);
void SDLPad_HandleKeyboardEvent(SDL_KeyboardEvent* event);
bool SDLPad_IsGamepadConnected(int id);
void SDLPad_GetButtonState(int id, SDLPad_ButtonState* state);
void SDLPad_RumblePad(int id, bool low_freq_enabled, Uint8 high_freq_rumble);
int SDLPad_GetNumConnectedDevices();
int SDLPad_GetMaxDevices();
const char* SDLPad_GetDeviceName(int index);
bool SDLPad_GetDeviceGUID(int index, char* out_guid, int guid_len);
bool SDLPad_GetLastInput(int device_index, char* out_button_name, int name_len);
void SDLPad_UpdatePreviousState();
void SDLPad_UpdatePreviousStateForDevice(int device_index);
bool SDLPad_IsAnyInputActive(int device_index);
bool SDLPad_IsKeyboard(int device_index);
bool SDLPad_IsJoystick(int device_index);
bool SDLPad_GetLastScancode(int device_index, int* out_scancode);
bool SDLPad_GetLastJoystickInput(int device_index, int* out_input_id);
void SDLPad_ClearKeyboardState();

int SDLPad_GetKeyboardDeviceIndex();

bool SDLPad_GetJoystickButton(int device_index, int button);
bool SDLPad_GetJoystickAxis(int device_index, int axis, int sign); // sign: 0 for +, 1 for -
bool SDLPad_GetJoystickHat(int device_index, int hat, int dir);    // dir: 0=Up, 1=Right, 2=Down, 3=Left

#ifdef __cplusplus
}
#endif
