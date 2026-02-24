#pragma once

#include <stdbool.h>

// This enum represents all possible inputs that can be mapped.
// It is used to quickly convert between string representations and a
// performant integer ID.
typedef enum {
    INPUT_ID_UNKNOWN = -1,
    INPUT_ID_DPAD_UP,
    INPUT_ID_DPAD_DOWN,
    INPUT_ID_DPAD_LEFT,
    INPUT_ID_DPAD_RIGHT,
    INPUT_ID_START,
    INPUT_ID_BACK,
    INPUT_ID_LEFT_STICK,
    INPUT_ID_RIGHT_STICK,
    INPUT_ID_LEFT_SHOULDER,
    INPUT_ID_RIGHT_SHOULDER,
    INPUT_ID_BUTTON_SOUTH,
    INPUT_ID_BUTTON_EAST,
    INPUT_ID_BUTTON_WEST,
    INPUT_ID_BUTTON_NORTH,
    INPUT_ID_LEFT_TRIGGER,
    INPUT_ID_RIGHT_TRIGGER,
    INPUT_ID_LEFT_STICK_X_PLUS,
    INPUT_ID_LEFT_STICK_X_MINUS,
    INPUT_ID_LEFT_STICK_Y_PLUS,
    INPUT_ID_LEFT_STICK_Y_MINUS,
    INPUT_ID_RIGHT_STICK_X_PLUS,
    INPUT_ID_RIGHT_STICK_X_MINUS,
    INPUT_ID_RIGHT_STICK_Y_PLUS,
    INPUT_ID_RIGHT_STICK_Y_MINUS,
    INPUT_ID_COUNT
} InputID;

// Base offset for keyboard scancodes to avoid collision with gamepad inputs
#define INPUT_ID_KEY_BASE 1000

// Base offsets for generic joystick inputs
#define INPUT_ID_JOY_BASE 2000
#define INPUT_ID_JOY_BTN_BASE (INPUT_ID_JOY_BASE + 0)
#define INPUT_ID_JOY_AXIS_BASE (INPUT_ID_JOY_BASE + 500)
#define INPUT_ID_JOY_HAT_BASE (INPUT_ID_JOY_BASE + 1000)

#ifdef __cplusplus
extern "C" {
#endif

// Check if an ID represents a keyboard key
bool is_keyboard_input(InputID id);
// Check if an ID represents a generic joystick input
bool is_joystick_input(InputID id);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

// Provides the string name for a given InputID.
std::string get_input_name(InputID id);

// Provides the InputID for a given string name.
InputID get_input_id(const std::string& name);

#endif // __cplusplus
