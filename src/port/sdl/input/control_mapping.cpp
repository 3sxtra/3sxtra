/**
 * @file control_mapping.cpp
 * @brief ImGui-based controller mapping UI and input configuration persistence.
 *
 * Manages gamepad/keyboard input binding definitions, device detection,
 * profile save/load via the config system, and renders the full-screen
 * control-mapping overlay using ImGui.
 */
#include "port/sdl/input/control_mapping.h"
#include "control_mapping_bindings.h"
#include "port/config/paths.h"
#include "port/input_definition.h"
#include "sdl_pad.h"
#include "sf33rd/Source/Game/io/ioconv.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Device {
    int id;
    std::string name;
    std::string guid;
};

static const std::string MAPPINGS_FILE = "mappings.ini";

struct Mapping {
    std::string action;
    InputID input_id;
};

enum class MappingState { Idle, Waiting, WaitingForKeyRelease, Done };

static std::vector<Device> availableDevices;
static std::unique_ptr<Device> p1Device = nullptr;
static std::unique_ptr<Device> p2Device = nullptr;

static MappingState p1MappingState = MappingState::Idle;
static MappingState p2MappingState = MappingState::Idle;
static int p1_mapping_action_index = 0;
static int p2_mapping_action_index = 0;

static std::map<int, std::vector<Mapping>> player_mappings;

// Helper to refresh the device list
static void refresh_devices() {
    availableDevices.clear();
    int maxDevices = SDLPad_GetMaxDevices();
    for (int i = 0; i < maxDevices; ++i) {
        if (!SDLPad_IsGamepadConnected(i))
            continue;

        char guid[64] = { 0 };
        SDLPad_GetDeviceGUID(i, guid, sizeof(guid));

        bool p1_has_device = p1Device && p1Device->id == i;
        bool p2_has_device = p2Device && p2Device->id == i;

        // Verify GUID matches if claimed
        if (p1_has_device && p1Device->guid != guid)
            p1_has_device = false;
        if (p2_has_device && p2Device->guid != guid)
            p2_has_device = false;

        if (!p1_has_device && !p2_has_device) {
            availableDevices.push_back({ i, SDLPad_GetDeviceName(i), guid });
        }
    }
}

// Get the persistent config directory for this app
static std::string get_mappings_file_path() {
    static std::string cached_path;
    if (cached_path.empty()) {
        const char* pref_path = Paths_GetPrefPath();
        if (pref_path) {
            cached_path = std::string(pref_path) + "mappings.ini";
        } else {
            // Fallback to current directory
            cached_path = "mappings.ini";
        }
    }
    return cached_path;
}

static void save_mappings() {
    std::string filepath = get_mappings_file_path();
    std::ofstream file(filepath);
    if (!file.is_open()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open mappings file for writing: %s", filepath.c_str());
        return;
    }

    if (p1Device) {
        file << "p1_device_id=" << p1Device->id << std::endl;
        file << "p1_device_name=" << p1Device->name << std::endl;
        file << "p1_device_guid=" << p1Device->guid << std::endl;
    }
    if (p2Device) {
        file << "p2_device_id=" << p2Device->id << std::endl;
        file << "p2_device_name=" << p2Device->name << std::endl;
        file << "p2_device_guid=" << p2Device->guid << std::endl;
    }
    for (const auto& pair : player_mappings) {
        for (const auto& mapping : pair.second) {
            file << "p" << pair.first << "_mapping=" << mapping.action << "," << get_input_name(mapping.input_id)
                 << std::endl;
        }
    }
    SDL_Log("Mappings saved to: %s", filepath.c_str());
}

static void load_mappings() {
    p1Device.reset();
    p2Device.reset();
    player_mappings.clear();

    std::string filepath = get_mappings_file_path();
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SDL_Log("No mappings file found at: %s", filepath.c_str());
        return;
    }

    SDL_Log("Loading mappings from: %s", filepath.c_str());

    std::string line;
    int p1_device_id = -1, p2_device_id = -1;
    std::string p1_device_name, p2_device_name;
    std::string p1_device_guid, p2_device_guid;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key, value;
        std::getline(ss, key, '=');
        std::getline(ss, value);

        try {
            if (key == "p1_device_id")
                p1_device_id = std::stoi(value);
            else if (key == "p1_device_name")
                p1_device_name = value;
            else if (key == "p1_device_guid")
                p1_device_guid = value;
            else if (key == "p2_device_id")
                p2_device_id = std::stoi(value);
            else if (key == "p2_device_name")
                p2_device_name = value;
            else if (key == "p2_device_guid")
                p2_device_guid = value;
            else if (key == "p1_mapping") {
                std::stringstream mapping_ss(value);
                std::string action, input_str;
                std::getline(mapping_ss, action, ',');
                std::getline(mapping_ss, input_str);
                player_mappings[1].push_back({ action, get_input_id(input_str) });
            } else if (key == "p2_mapping") {
                std::stringstream mapping_ss(value);
                std::string action, input_str;
                std::getline(mapping_ss, action, ',');
                std::getline(mapping_ss, input_str);
                player_mappings[2].push_back({ action, get_input_id(input_str) });
            }
        } catch (const std::invalid_argument& e) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mappings parse error on line '%s': %s", line.c_str(), e.what());
        }
    }

    int maxDevices = SDLPad_GetMaxDevices();

    // Track which device indices have been claimed to prevent duplicates
    std::set<int> claimed_indices;

    // First pass: Try GUID match
    for (int i = 0; i < maxDevices; ++i) {
        if (!SDLPad_IsGamepadConnected(i))
            continue;

        char guid[64] = { 0 };
        SDLPad_GetDeviceGUID(i, guid, sizeof(guid));
        const char* connected_name = SDLPad_GetDeviceName(i);

        if (!p1Device && !p1_device_guid.empty() && p1_device_guid == guid) {
            p1Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P1 claimed device %d (%s) - GUID match", i, connected_name);
        }

        if (claimed_indices.count(i))
            continue;

        if (!p2Device && !p2_device_guid.empty() && p2_device_guid == guid) {
            p2Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P2 claimed device %d (%s) - GUID match", i, connected_name);
        }
    }

    // Second pass: Try exact id+name match (legacy/fallback)
    for (int i = 0; i < maxDevices; ++i) {
        if (claimed_indices.count(i))
            continue;
        if (!SDLPad_IsGamepadConnected(i))
            continue;

        const char* connected_name = SDLPad_GetDeviceName(i);
        char guid[64] = { 0 };
        SDLPad_GetDeviceGUID(i, guid, sizeof(guid));

        if (!p1Device && p1_device_id == i && p1_device_name == connected_name) {
            p1Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P1 claimed device %d (%s) - Legacy ID+Name match", i, connected_name);
        }

        if (claimed_indices.count(i))
            continue;

        if (!p2Device && p2_device_id == i && p2_device_name == connected_name) {
            p2Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P2 claimed device %d (%s) - Legacy ID+Name match", i, connected_name);
        }
    }

    // Third pass: Match by name only for devices not yet claimed
    for (int i = 0; i < maxDevices; ++i) {
        if (claimed_indices.count(i))
            continue;
        if (!SDLPad_IsGamepadConnected(i))
            continue;

        const char* connected_name = SDLPad_GetDeviceName(i);
        char guid[64] = { 0 };
        SDLPad_GetDeviceGUID(i, guid, sizeof(guid));

        if (!p1Device && !p1_device_name.empty() && p1_device_name == connected_name) {
            p1Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P1 claimed device %d (%s) - name match", i, connected_name);
        }

        if (claimed_indices.count(i))
            continue;

        if (!p2Device && !p2_device_name.empty() && p2_device_name == connected_name) {
            p2Device = std::make_unique<Device>(Device { i, connected_name, guid });
            claimed_indices.insert(i);
            SDL_Log("P2 claimed device %d (%s) - name match", i, connected_name);
        }
    }
}

// Helper to detect device type from name
static std::string detect_device_type(const std::string& device_name) {
    std::string lower_name = device_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    // Check for specific controller types
    if (lower_name.find("xbox series") != std::string::npos || lower_name.find("xbox one") != std::string::npos ||
        lower_name.find("xbox 360") != std::string::npos) {
        if (lower_name.find("series") != std::string::npos)
            return "xbox_series";
        if (lower_name.find("one") != std::string::npos)
            return "xbox_one";
        if (lower_name.find("360") != std::string::npos)
            return "xbox_360";
        return "xbox_series"; // Default to Series X
    }

    if (lower_name.find("dualsense") != std::string::npos || lower_name.find("ps5") != std::string::npos ||
        lower_name.find("playstation 5") != std::string::npos) {
        return "ps5";
    }

    if (lower_name.find("dualshock 4") != std::string::npos || lower_name.find("ps4") != std::string::npos ||
        lower_name.find("playstation 4") != std::string::npos) {
        return "ps4";
    }

    if (lower_name.find("dualshock 3") != std::string::npos || lower_name.find("ps3") != std::string::npos ||
        lower_name.find("playstation 3") != std::string::npos) {
        return "ps3";
    }

    if (lower_name.find("switch") != std::string::npos || lower_name.find("nintendo") != std::string::npos ||
        lower_name.find("joy-con") != std::string::npos) {
        return "switch";
    }

    if (lower_name.find("keyboard") != std::string::npos) {
        return "keyboard";
    }

    if (lower_name.find("steam") != std::string::npos || lower_name.find("deck") != std::string::npos) {
        return "steam_deck";
    }

    // Default to generic controller (placeholder for now)
    return "generic";
}

// Helper to get icon path for device type
static std::string get_device_icon_path(const std::string& device_type) {
    if (device_type == "xbox_series") {
        return "assets/controllers/xbox_series.png";
    } else if (device_type == "xbox_one") {
        return "assets/controllers/xbox_one.png";
    } else if (device_type == "xbox_360") {
        return "assets/controllers/xbox_360.png";
    } else if (device_type == "ps5") {
        return "assets/controllers/ps5.png";
    } else if (device_type == "ps4") {
        return "assets/controllers/ps4.png";
    } else if (device_type == "ps3") {
        return "assets/controllers/ps3.png";
    } else if (device_type == "switch") {
        return "assets/controllers/switch.png";
    } else if (device_type == "steam_deck") {
        return "assets/controllers/steam_deck.png";
    } else if (device_type == "keyboard") {
        return "assets/keyboard.png";
    } else {
        // Generic/unknown controller
        return "assets/controller.png";
    }
}

extern "C" void control_mapping_init() {
    load_mappings();
}

static void handle_player_mapping_update(int player_num, Device* playerDevice, MappingState& mappingState,
                                         int& mapping_action_index) {
    if (!playerDevice) {
        return;
    }

    // Auto-reset from Done state after a brief display
    if (mappingState == MappingState::Done) {
        // Reset to Idle so user can remap or controls are ready
        mappingState = MappingState::Idle;
        return;
    }

    if (mappingState == MappingState::Waiting) {
        // Check for keyboard scancodes first
        int scancode;
        if (SDLPad_GetLastScancode(playerDevice->id, &scancode)) {
            if (mapping_action_index >= 0 && mapping_action_index < get_game_actions_count()) {
                // Clear old mappings on first capture
                if (mapping_action_index == 0) {
                    player_mappings[player_num].clear();
                }
                InputID id = (InputID)(INPUT_ID_KEY_BASE + scancode);
                player_mappings[player_num].push_back({ game_actions[mapping_action_index], id });
                mappingState = MappingState::WaitingForKeyRelease;
            } else {
                mappingState = MappingState::Done;
                save_mappings();
            }
            return;
        }

        // Check for generic joystick inputs
        int joy_input;
        if (SDLPad_GetLastJoystickInput(playerDevice->id, &joy_input)) {
            if (mapping_action_index >= 0 && mapping_action_index < get_game_actions_count()) {
                // Clear old mappings on first capture
                if (mapping_action_index == 0) {
                    player_mappings[player_num].clear();
                }
                player_mappings[player_num].push_back({ game_actions[mapping_action_index], (InputID)joy_input });
                mappingState = MappingState::WaitingForKeyRelease;
            } else {
                mappingState = MappingState::Done;
                save_mappings();
            }
            return;
        }

        char input_name[64];
        if (SDLPad_GetLastInput(playerDevice->id, input_name, sizeof(input_name))) {
            if (mapping_action_index >= 0 && mapping_action_index < get_game_actions_count()) {
                // Clear old mappings on first capture
                if (mapping_action_index == 0) {
                    player_mappings[player_num].clear();
                }
                player_mappings[player_num].push_back({ game_actions[mapping_action_index], get_input_id(input_name) });
                mappingState = MappingState::WaitingForKeyRelease;
                SDLPad_UpdatePreviousStateForDevice(playerDevice->id);
            } else {
                // Out of range; finalize mapping to avoid undefined behavior
                mappingState = MappingState::Done;
                save_mappings();
            }
        }
    } else if (mappingState == MappingState::WaitingForKeyRelease) {
        if (!SDLPad_IsAnyInputActive(playerDevice->id)) {
            mapping_action_index++;
            if (mapping_action_index >= get_game_actions_count()) {
                mappingState = MappingState::Done;
                save_mappings();
            } else {
                mappingState = MappingState::Waiting;
            }
        }
    }
}

static void check_connections() {
    int maxDevices = SDLPad_GetMaxDevices();

    auto check_device = [&](std::unique_ptr<Device>& device, const char* label) {
        if (!device)
            return;

        if (SDLPad_IsGamepadConnected(device->id)) {
            char guid[64] = { 0 };
            SDLPad_GetDeviceGUID(device->id, guid, sizeof(guid));
            if (device->guid == guid) {
                return;
            }
        }

        bool recovered = false;
        for (int i = 0; i < maxDevices; ++i) {
            if (!SDLPad_IsGamepadConnected(i))
                continue;

            char guid[64] = { 0 };
            SDLPad_GetDeviceGUID(i, guid, sizeof(guid));

            if (device->guid == guid) {
                if (device->id != i) {
                    SDL_Log("%s device re-acquired at index %d (was %d)", label, i, device->id);
                    device->id = i;
                }
                recovered = true;
                break;
            }
        }

        if (!recovered) {
            if (device->id != -1) {
                SDL_Log("%s device lost (index %d)", label, device->id);
                device->id = -1;
            }
        }
    };

    check_device(p1Device, "P1");
    check_device(p2Device, "P2");
}

extern "C" void control_mapping_update() {
    check_connections();
    handle_player_mapping_update(1, p1Device.get(), p1MappingState, p1_mapping_action_index);
    handle_player_mapping_update(2, p2Device.get(), p2MappingState, p2_mapping_action_index);
}

extern "C" void control_mapping_render(int window_width, int window_height) {
    (void)window_width;
    (void)window_height;
}

extern "C" bool control_mapping_is_active() {
    return p1MappingState == MappingState::Waiting || p2MappingState == MappingState::Waiting ||
           p1MappingState == MappingState::WaitingForKeyRelease || p2MappingState == MappingState::WaitingForKeyRelease;
}

extern "C" void control_mapping_shutdown() {
    // No-op — textures managed by TextureUtil.
}

extern "C" {

int ControlMapping_GetPlayerDeviceID(int player_num) {
    if (player_num == 1 && p1Device) {
        return p1Device->id;
    }
    if (player_num == 2 && p2Device) {
        return p2Device->id;
    }
    return -1;
}

InputID ControlMapping_GetPlayerMapping(int player_num, const char* action) {
    if (player_mappings.find(player_num) != player_mappings.end()) {
        for (const auto& mapping : player_mappings.at(player_num)) {
            if (mapping.action == action) {
                return mapping.input_id;
            }
        }
    }
    return INPUT_ID_UNKNOWN;
}

} // extern "C"

// ── Accessor functions for RmlUi module ─────────────────────────

extern "C" {

const char* ControlMapping_GetDeviceName(int player_num) {
    if (player_num == 1 && p1Device)
        return p1Device->name.c_str();
    if (player_num == 2 && p2Device)
        return p2Device->name.c_str();
    return nullptr;
}

bool ControlMapping_HasDevice(int player_num) {
    if (player_num == 1)
        return p1Device != nullptr;
    if (player_num == 2)
        return p2Device != nullptr;
    return false;
}

void ControlMapping_ClaimDevice(int player_num, int device_index) {
    refresh_devices();
    for (auto it = availableDevices.begin(); it != availableDevices.end(); ++it) {
        if (it->id == device_index) {
            if (player_num == 1) {
                p1Device = std::make_unique<Device>(*it);
                p1MappingState = MappingState::Idle;
            } else {
                p2Device = std::make_unique<Device>(*it);
                p2MappingState = MappingState::Idle;
            }
            availableDevices.erase(it);
            save_mappings();
            return;
        }
    }
}

void ControlMapping_UnclaimDevice(int player_num) {
    if (player_num == 1) {
        p1Device.reset();
        p1MappingState = MappingState::Idle;
    } else {
        p2Device.reset();
        p2MappingState = MappingState::Idle;
    }
    save_mappings();
}

void ControlMapping_StartMapping(int player_num) {
    if (player_num == 1 && p1Device) {
        p1MappingState = MappingState::Waiting;
        p1_mapping_action_index = 0;
    } else if (player_num == 2 && p2Device) {
        p2MappingState = MappingState::Waiting;
        p2_mapping_action_index = 0;
    }
}

void ControlMapping_ResetMappings(int player_num) {
    player_mappings[player_num].clear();
    if (player_num == 1)
        p1MappingState = MappingState::Idle;
    else
        p2MappingState = MappingState::Idle;
    save_mappings();
}

int ControlMapping_GetMappingState(int player_num) {
    MappingState s = (player_num == 1) ? p1MappingState : p2MappingState;
    return (int)s;
}

int ControlMapping_GetMappingActionIndex(int player_num) {
    return (player_num == 1) ? p1_mapping_action_index : p2_mapping_action_index;
}

int ControlMapping_GetAvailableDeviceCount() {
    refresh_devices();
    return (int)availableDevices.size();
}

const char* ControlMapping_GetAvailableDeviceName(int index) {
    if (index < 0 || index >= (int)availableDevices.size())
        return nullptr;
    return availableDevices[index].name.c_str();
}

int ControlMapping_GetAvailableDeviceId(int index) {
    if (index < 0 || index >= (int)availableDevices.size())
        return -1;
    return availableDevices[index].id;
}

int ControlMapping_GetPlayerMappingCount(int player_num) {
    auto it = player_mappings.find(player_num);
    if (it == player_mappings.end())
        return 0;
    return (int)it->second.size();
}

const char* ControlMapping_GetPlayerMappingAction(int player_num, int index) {
    auto it = player_mappings.find(player_num);
    if (it == player_mappings.end() || index < 0 || index >= (int)it->second.size())
        return nullptr;
    return it->second[index].action.c_str();
}

const char* ControlMapping_GetPlayerMappingInput(int player_num, int index) {
    auto it = player_mappings.find(player_num);
    if (it == player_mappings.end() || index < 0 || index >= (int)it->second.size())
        return nullptr;
    static std::string s_input_name_buf; // static buffer for c_str() lifetime
    s_input_name_buf = get_input_name(it->second[index].input_id);
    return s_input_name_buf.c_str();
}

const char* ControlMapping_GetDeviceIconPath(int player_num) {
    const char* name = ControlMapping_GetDeviceName(player_num);
    if (!name)
        return nullptr;
    std::string device_type = detect_device_type(name);
    static std::string s_icon_path_buf;
    s_icon_path_buf = get_device_icon_path(device_type);
    return s_icon_path_buf.c_str();
}

const char* ControlMapping_GetAvailableDeviceIconPath(int index) {
    if (index < 0 || index >= (int)availableDevices.size())
        return nullptr;
    std::string device_type = detect_device_type(availableDevices[index].name);
    static std::string s_avail_icon_path_buf;
    s_avail_icon_path_buf = get_device_icon_path(device_type);
    return s_avail_icon_path_buf.c_str();
}

} // extern "C"
