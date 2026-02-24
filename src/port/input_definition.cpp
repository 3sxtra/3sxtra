/**
 * @file input_definition.cpp
 * @brief Centralized input ID ↔ string name mapping.
 *
 * Provides bidirectional conversion between `InputID` enum values and
 * human-readable string names, used for config serialization and UI display.
 * Handles keyboard scancodes, joystick buttons/axes, and gamepad buttons.
 */
#include "port/input_definition.h"
#include <SDL3/SDL.h>
#include <map>
#include <string>

// Bidirectional mapping to convert between InputIDs and their string names.
// This is done to centralize the definitions and avoid string comparisons
// in performance-critical code.
static const std::map<InputID, std::string> id_to_name_map = { { INPUT_ID_DPAD_UP, "DPad Up" },
                                                               { INPUT_ID_DPAD_DOWN, "DPad Down" },
                                                               { INPUT_ID_DPAD_LEFT, "DPad Left" },
                                                               { INPUT_ID_DPAD_RIGHT, "DPad Right" },
                                                               { INPUT_ID_START, "Start" },
                                                               { INPUT_ID_BACK, "Back" },
                                                               { INPUT_ID_LEFT_STICK, "Left Stick" },
                                                               { INPUT_ID_RIGHT_STICK, "Right Stick" },
                                                               { INPUT_ID_LEFT_SHOULDER, "Left Shoulder" },
                                                               { INPUT_ID_RIGHT_SHOULDER, "Right Shoulder" },
                                                               { INPUT_ID_BUTTON_SOUTH, "Button South" },
                                                               { INPUT_ID_BUTTON_EAST, "Button East" },
                                                               { INPUT_ID_BUTTON_WEST, "Button West" },
                                                               { INPUT_ID_BUTTON_NORTH, "Button North" },
                                                               { INPUT_ID_LEFT_TRIGGER, "Left Trigger" },
                                                               { INPUT_ID_RIGHT_TRIGGER, "Right Trigger" },
                                                               { INPUT_ID_LEFT_STICK_X_PLUS, "Left Stick X+" },
                                                               { INPUT_ID_LEFT_STICK_X_MINUS, "Left Stick X-" },
                                                               { INPUT_ID_LEFT_STICK_Y_PLUS, "Left Stick Y+" },
                                                               { INPUT_ID_LEFT_STICK_Y_MINUS, "Left Stick Y-" },
                                                               { INPUT_ID_RIGHT_STICK_X_PLUS, "Right Stick X+" },
                                                               { INPUT_ID_RIGHT_STICK_X_MINUS, "Right Stick X-" },
                                                               { INPUT_ID_RIGHT_STICK_Y_PLUS, "Right Stick Y+" },
                                                               { INPUT_ID_RIGHT_STICK_Y_MINUS, "Right Stick Y-" } };

// Reverse map for efficient name-to-ID lookups.
static std::map<std::string, InputID> name_to_id_map;

// Ensures the reverse map is populated before it's ever used.
/** @brief Lazily populate the reverse (name→ID) lookup map on first access. */
static bool ensure_reverse_map() {
    if (name_to_id_map.empty()) {
        for (const auto& pair : id_to_name_map) {
            name_to_id_map[pair.second] = pair.first;
        }
    }
    return true;
}

/** @brief Convert an InputID to its human-readable name (handles keys/joystick/gamepad). */
std::string get_input_name(InputID id) {
    if (is_keyboard_input(id)) {
        SDL_Scancode scancode = (SDL_Scancode)((int)id - INPUT_ID_KEY_BASE);
        const char* name = SDL_GetScancodeName(scancode);
        if (name && *name) {
            return std::string("Key ") + name;
        }
    } else if (is_joystick_input(id)) {
        int val = (int)id;
        if (val >= INPUT_ID_JOY_HAT_BASE) {
            int hat = (val - INPUT_ID_JOY_HAT_BASE) / 4;
            int dir = (val - INPUT_ID_JOY_HAT_BASE) % 4;
            const char* dir_str = "Up";
            if (dir == 1)
                dir_str = "Right";
            if (dir == 2)
                dir_str = "Down";
            if (dir == 3)
                dir_str = "Left";
            return "Joy Hat " + std::to_string(hat) + " " + dir_str;
        } else if (val >= INPUT_ID_JOY_AXIS_BASE) {
            int axis = (val - INPUT_ID_JOY_AXIS_BASE) / 2;
            int sign = (val - INPUT_ID_JOY_AXIS_BASE) % 2;
            return "Joy Axis " + std::to_string(axis) + (sign ? "-" : "+");
        } else if (val >= INPUT_ID_JOY_BTN_BASE) {
            int btn = val - INPUT_ID_JOY_BTN_BASE;
            return "Joy Button " + std::to_string(btn);
        }
    }

    static const std::string unknown = "Unknown";
    auto it = id_to_name_map.find(id);
    if (it != id_to_name_map.end()) {
        return it->second;
    }
    return unknown;
}

/** @brief Convert a human-readable name back to its InputID (inverse of get_input_name). */
InputID get_input_id(const std::string& name) {
    if (name.rfind("Key ", 0) == 0) {
        std::string keyName = name.substr(4);
        SDL_Scancode scancode = SDL_GetScancodeFromName(keyName.c_str());
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            return (InputID)(INPUT_ID_KEY_BASE + (int)scancode);
        }
    } else if (name.rfind("Joy ", 0) == 0) {
        if (name.rfind("Joy Button ", 0) == 0) {
            int btn = std::stoi(name.substr(11));
            return (InputID)(INPUT_ID_JOY_BTN_BASE + btn);
        } else if (name.rfind("Joy Axis ", 0) == 0) {
            size_t signPos = name.find_last_of("+-");
            if (signPos != std::string::npos) {
                int axis = std::stoi(name.substr(9, signPos - 9));
                bool is_minus = (name[signPos] == '-');
                return (InputID)(INPUT_ID_JOY_AXIS_BASE + axis * 2 + (is_minus ? 1 : 0));
            }
        } else if (name.rfind("Joy Hat ", 0) == 0) {
            size_t spacePos = name.find(' ', 8);
            if (spacePos != std::string::npos) {
                int hat = std::stoi(name.substr(8, spacePos - 8));
                std::string dirStr = name.substr(spacePos + 1);
                int dir = 0;
                if (dirStr == "Right")
                    dir = 1;
                else if (dirStr == "Down")
                    dir = 2;
                else if (dirStr == "Left")
                    dir = 3;
                return (InputID)(INPUT_ID_JOY_HAT_BASE + hat * 4 + dir);
            }
        }
    }

    ensure_reverse_map();
    auto it = name_to_id_map.find(name);
    if (it != name_to_id_map.end()) {
        return it->second;
    }
    return INPUT_ID_UNKNOWN;
}

/** @brief Return true if the ID falls in the keyboard scancode range. */
extern "C" bool is_keyboard_input(InputID id) {
    return (int)id >= INPUT_ID_KEY_BASE && (int)id < INPUT_ID_JOY_BASE;
}

/** @brief Return true if the ID falls in the joystick range (buttons/axes/hats). */
extern "C" bool is_joystick_input(InputID id) {
    return (int)id >= INPUT_ID_JOY_BASE;
}
