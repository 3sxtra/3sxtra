/**
 * @file control_mapping.cpp
 * @brief ImGui-based controller mapping UI and input configuration persistence.
 *
 * Manages gamepad/keyboard input binding definitions, device detection,
 * profile save/load via the config system, and renders the full-screen
 * control-mapping overlay using ImGui.
 */
#include "port/sdl/control_mapping.h"
#include "control_mapping_bindings.h"
#include "imgui.h"
#include "imgui_wrapper.h"
#include "port/input_definition.h"
#include "port/paths.h"
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

// Device icon textures
static std::map<std::string, void*> device_icon_textures;

// Helper to render centered text using ImGui.
static float render_centered_text(const char* text, bool dry_run = false) {
    if (text == nullptr)
        return 0.0f;
    float height = ImGui::GetTextLineHeightWithSpacing();
    if (!dry_run) {
        float window_width = ImGui::GetContentRegionAvail().x;
        float text_width = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - text_width) / 2);
        ImGui::Text("%s", text);
    }
    return height;
}

static float get_separator_height() {
    return ImGui::GetStyle().ItemSpacing.y * 2.0f + 1.0f; // Approximate separator height
}
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

static void* capcom_icons_texture = nullptr;

// UV coordinates for each icon in the sprite sheet

const float ICON_HEIGHT = 32.0f;
// Texture dimensions will be queried at runtime

#define UV_RECT(y_offset)                                                                                              \
    ImVec2(0.0f, (y_offset * ICON_HEIGHT) / 544.0f), ImVec2(0.5f, ((y_offset + 1) * ICON_HEIGHT) / 544.0f)

static const std::map<std::string, std::pair<ImVec2, ImVec2>> action_to_uv = {
    // Directions
    { "Left", { UV_RECT(0) } },
    { "Right", { UV_RECT(1) } },
    { "Up", { UV_RECT(2) } },
    { "Down", { UV_RECT(3) } },
    // Punches
    { "Light Punch", { UV_RECT(8) } },
    { "Medium Punch", { UV_RECT(9) } },
    { "Hard Punch", { UV_RECT(10) } },
    // Kicks
    { "Light Kick", { UV_RECT(11) } },
    { "Medium Kick", { UV_RECT(12) } },
    { "Hard Kick", { UV_RECT(13) } },
    // Other
    { "Start", { UV_RECT(14) } },
    { "Select", { UV_RECT(15) } },
    // Neutral (no directional input)
    { "Neutral", { UV_RECT(16) } }
};

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

// Helper to load device icon texture
static void* get_device_icon_texture(const std::string& device_name) {
    std::string device_type = detect_device_type(device_name);

    // Check if already loaded
    if (device_icon_textures.find(device_type) != device_icon_textures.end()) {
        return device_icon_textures[device_type];
    }

    // Load the texture
    std::string icon_path = get_device_icon_path(device_type);
    if (!icon_path.empty()) {
        const char* base_path = Paths_GetBasePath();
        if (base_path) {
            std::string full_path = std::string(base_path) + icon_path;
            void* texture = imgui_wrapper_load_texture(full_path.c_str());
            if (texture) {
                device_icon_textures[device_type] = texture;
                return texture;
            }
        }
    }

    return nullptr;
}

extern "C" void control_mapping_init() {
    load_mappings();
    capcom_icons_texture = imgui_wrapper_get_capcom_icons_texture();
}

static float handle_player_column(int player_num, std::unique_ptr<Device>& playerDevice, MappingState& mappingState,
                                  int& mapping_action_index, float icon_size, bool dry_run = false) {
    float total_height = 0.0f;
    std::string player_str = "P" + std::to_string(player_num);
    std::string title = player_str + "'s Device";

    total_height += render_centered_text(title.c_str(), dry_run);

    if (!dry_run)
        ImGui::Separator();
    total_height += get_separator_height();

    if (playerDevice) {
        // Render device icon instead of name
        void* device_texture = get_device_icon_texture(playerDevice->name);
        if (device_texture && !dry_run) {
            int tex_w = 0, tex_h = 0;
            imgui_wrapper_get_texture_size(device_texture, &tex_w, &tex_h);

            // Calculate icon size to fit nicely (smaller than action icons)
            float device_icon_height = icon_size * 2.0f; // Make it bigger for visibility
            float aspect = (float)tex_w / (float)tex_h;
            float device_icon_width = device_icon_height * aspect;

            // Center the icon
            float window_width = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - device_icon_width) / 2);
            ImGui::Image(device_texture, ImVec2(device_icon_width, device_icon_height));
            total_height += device_icon_height + ImGui::GetStyle().ItemSpacing.y;
        } else {
            // Fallback to text if no icon available
            if (!dry_run)
                ImGui::Text("%s", playerDevice->name.c_str());
            total_height += ImGui::GetTextLineHeightWithSpacing();
        }

        if (!dry_run) {
            if (ImGui::Button(("Unclaim##" + player_str).c_str())) {
                playerDevice.reset();
                mappingState = MappingState::Idle;
                save_mappings();
            }
        }
        total_height += ImGui::GetFrameHeightWithSpacing();

        if (mappingState == MappingState::Idle) {
            if (!dry_run) {
                if (ImGui::Button(("Map Controls##" + player_str).c_str())) {
                    mappingState = MappingState::Waiting;
                    mapping_action_index = 0;
                }
            }
            total_height += ImGui::GetFrameHeightWithSpacing();
        }
        if (!dry_run) {
            std::string reset_popup_id = "ConfirmReset##" + player_str;
            if (ImGui::Button(("Reset to Defaults##" + player_str).c_str())) {
                ImGui::OpenPopup(reset_popup_id.c_str());
            }
            if (ImGui::BeginPopupModal(reset_popup_id.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Reset all %s mappings to defaults?", player_str.c_str());
                ImGui::Spacing();
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    player_mappings[player_num].clear();
                    mappingState = MappingState::Idle;
                    save_mappings();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        total_height += ImGui::GetFrameHeightWithSpacing();
    }
    return total_height;
}

static float render_prompt(MappingState state, int action_index, float icon_size, bool dry_run = false) {
    float total_height = 0.0f;
    if (state == MappingState::Waiting) {
        if (action_index < get_game_actions_count()) {
            const char* action = game_actions[action_index];
            if (action_to_uv.count(action)) {
                if (!dry_run) {
                    auto uv = action_to_uv.at(action);
                    int tex_w = 0, tex_h = 0;
                    imgui_wrapper_get_texture_size(capcom_icons_texture, &tex_w, &tex_h);
                    // Since we are cropping to half width (0.5), the aspect ratio is based on 32px width
                    float aspect = 1.0f;
                    if (capcom_icons_texture)
                        ImGui::Image(capcom_icons_texture, ImVec2(icon_size * aspect, icon_size), uv.first, uv.second);
                    ImGui::SameLine();
                }
            }
            if (!dry_run)
                ImGui::TextWrapped("Press a button for %s", action);
            // Height is max of icon and text, roughly
            total_height += std::max(icon_size, ImGui::GetTextLineHeightWithSpacing());
        } else {
            if (!dry_run)
                ImGui::Text("Waiting for input...");
            total_height += ImGui::GetTextLineHeightWithSpacing();
        }
    } else if (state == MappingState::WaitingForKeyRelease) {
        if (!dry_run)
            ImGui::Text("Please release all inputs.");
        total_height += ImGui::GetTextLineHeightWithSpacing();
    } else if (state == MappingState::Done) {
        if (!dry_run)
            ImGui::Text("Mapping Complete!");
        total_height += ImGui::GetTextLineHeightWithSpacing();
    } else {
        if (!dry_run)
            ImGui::Text("Assign & Map");
        total_height += ImGui::GetTextLineHeightWithSpacing();
    }
    return total_height;
}

static float show_mappings(int player_num, float icon_size, bool dry_run = false) {
    float total_height = 0.0f;
    if (player_mappings.find(player_num) != player_mappings.end()) {
        for (const auto& mapping : player_mappings.at(player_num)) {
            if (action_to_uv.count(mapping.action)) {
                if (!dry_run) {
                    auto uv = action_to_uv.at(mapping.action);
                    int tex_w = 0, tex_h = 0;
                    imgui_wrapper_get_texture_size(capcom_icons_texture, &tex_w, &tex_h);
                    // Since we are cropping to half width (0.5), the aspect ratio is based on 32px width
                    float aspect = 1.0f;
                    if (capcom_icons_texture)
                        ImGui::Image(capcom_icons_texture, ImVec2(icon_size * aspect, icon_size), uv.first, uv.second);
                    ImGui::SameLine();
                }
            }
            if (!dry_run)
                ImGui::TextWrapped("%s", get_input_name(mapping.input_id).c_str());
            total_height += std::max(icon_size, ImGui::GetTextLineHeightWithSpacing());
        }
    }
    return total_height;
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

static float render_available_devices(bool dry_run = false) {
    float total_height = 0.0f;
    total_height += render_centered_text("Available Devices", dry_run);

    if (!dry_run)
        ImGui::Separator();
    total_height += get_separator_height();

    for (auto it = availableDevices.begin(); it != availableDevices.end();) {
        if (!dry_run) {
            // Render device icon instead of name
            void* device_texture = get_device_icon_texture(it->name);
            float window_width = ImGui::GetContentRegionAvail().x;

            if (device_texture) {
                int tex_w = 0, tex_h = 0;
                imgui_wrapper_get_texture_size(device_texture, &tex_w, &tex_h);

                // Calculate icon size - bigger for central column
                float device_icon_height = 100.0f; // Increased size for better visibility
                float aspect = (float)tex_w / (float)tex_h;
                float device_icon_width = device_icon_height * aspect;

                // Center the icon
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - device_icon_width) / 2);
                ImGui::Image(device_texture, ImVec2(device_icon_width, device_icon_height));
            } else {
                // Fallback to text if no icon available
                float text_width = ImGui::CalcTextSize(it->name.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - text_width) / 2);
                ImGui::Text("%s", it->name.c_str());
            }

            // Buttons on next line

            // Calculate button group width for centering (properly this time)
            std::string p1_label = "<<##p1_" + std::to_string(it->id);
            std::string p2_label = ">>##p2_" + std::to_string(it->id);
            // CalcTextSize only on visible text ("<<" and ">>")
            float p1_btn_w = ImGui::CalcTextSize("<<").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float p2_btn_w = ImGui::CalcTextSize(">>").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_btn_width = p1_btn_w + spacing + p2_btn_w;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - total_btn_width) / 2);

            // Check button clicks before processing to prevent double-claiming
            bool p1_clicked = ImGui::Button(p1_label.c_str());
            ImGui::SameLine();
            bool p2_clicked = ImGui::Button(p2_label.c_str());

            // Only process one claim per frame
            if (p1_clicked && !p2_clicked) {
                p1Device = std::make_unique<Device>(*it);
                it = availableDevices.erase(it);
                save_mappings();
            } else if (p2_clicked && !p1_clicked) {
                p2Device = std::make_unique<Device>(*it);
                it = availableDevices.erase(it);
                save_mappings();
            } else {
                // If both clicked or neither clicked, just advance
                ++it;
            }
        } else {
            // In dry run, account for text line AND button line
            ++it;
        }
        total_height += ImGui::GetTextLineHeightWithSpacing(); // Name
        total_height += ImGui::GetFrameHeightWithSpacing();    // Buttons
    }
    return total_height;
}

extern "C" void control_mapping_render(int window_width, int window_height) {
    refresh_devices();

    ImVec2 window_size(window_width, window_height);
    ImVec2 window_pos(0, 0);

    ImGui::SetNextWindowPos(window_pos);
    ImGui::SetNextWindowSize(window_size);
    ImGui::Begin("Symmetrical Control Mapper",
                 NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float font_scale = window_height / 480.0f;
    ImGui::SetWindowFontScale(font_scale);

    // Title header
    ImGui::Spacing();
    render_centered_text("CONTROLLER SETUP");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float icon_size = window_height * 0.05f;

    // Reserve space for footer so table doesn't overflow
    float footer_h = ImGui::GetTextLineHeightWithSpacing() * 2.0f + get_separator_height();
    float table_h = ImGui::GetContentRegionAvail().y - footer_h;
    if (table_h < 100.0f)
        table_h = 100.0f;

    // Subtle styling: no hard grid lines, just column separators and row shading
    ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX;

    if (ImGui::BeginTable("ControlMapping", 5, table_flags, ImVec2(0, table_h))) {
        // Set explicit column proportions: Prompt(28%) | Device(15%) | Available(14%) | Device(15%) | Prompt(28%)
        float total_w = (float)window_width;
        ImGui::TableSetupColumn("P1Prompt", ImGuiTableColumnFlags_WidthStretch, 0.28f * total_w);
        ImGui::TableSetupColumn("P1Device", ImGuiTableColumnFlags_WidthStretch, 0.15f * total_w);
        ImGui::TableSetupColumn("Available", ImGuiTableColumnFlags_WidthStretch, 0.14f * total_w);
        ImGui::TableSetupColumn("P2Device", ImGuiTableColumnFlags_WidthStretch, 0.15f * total_w);
        ImGui::TableSetupColumn("P2Prompt", ImGuiTableColumnFlags_WidthStretch, 0.28f * total_w);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, table_h);

        // Column 1: P1 Prompt
        ImGui::TableNextColumn();
        ImGui::Spacing();
        render_centered_text("P1 Mappings");
        ImGui::Separator();
        ImGui::Spacing();
        render_prompt(p1MappingState, p1_mapping_action_index, icon_size);
        ImGui::Separator();
        show_mappings(1, icon_size);

        // Column 2: P1 Device
        ImGui::TableNextColumn();
        ImGui::Spacing();
        handle_player_column(1, p1Device, p1MappingState, p1_mapping_action_index, icon_size);

        // Column 3: Available Devices
        ImGui::TableNextColumn();
        ImGui::Spacing();
        render_available_devices();

        // Column 4: P2 Device
        ImGui::TableNextColumn();
        ImGui::Spacing();
        handle_player_column(2, p2Device, p2MappingState, p2_mapping_action_index, icon_size);

        // Column 5: P2 Prompt
        ImGui::TableNextColumn();
        ImGui::Spacing();
        render_centered_text("P2 Mappings");
        ImGui::Separator();
        ImGui::Spacing();
        render_prompt(p2MappingState, p2_mapping_action_index, icon_size);
        ImGui::Separator();
        show_mappings(2, icon_size);

        ImGui::EndTable();
    }

    // Footer
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    render_centered_text("F1: Close | Note: In-game Button Config is inactive while mappings are set here");

    ImGui::End();
}

extern "C" bool control_mapping_is_active() {
    return p1MappingState == MappingState::Waiting || p2MappingState == MappingState::Waiting ||
           p1MappingState == MappingState::WaitingForKeyRelease || p2MappingState == MappingState::WaitingForKeyRelease;
}

extern "C" void control_mapping_shutdown() {
    // Textures are now managed by the imgui_wrapper, so this is no longer needed.
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
    if (player_num == 1 && p1Device) return p1Device->name.c_str();
    if (player_num == 2 && p2Device) return p2Device->name.c_str();
    return nullptr;
}

bool ControlMapping_HasDevice(int player_num) {
    if (player_num == 1) return p1Device != nullptr;
    if (player_num == 2) return p2Device != nullptr;
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
    if (player_num == 1) p1MappingState = MappingState::Idle;
    else p2MappingState = MappingState::Idle;
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
    if (index < 0 || index >= (int)availableDevices.size()) return nullptr;
    return availableDevices[index].name.c_str();
}

int ControlMapping_GetAvailableDeviceId(int index) {
    if (index < 0 || index >= (int)availableDevices.size()) return -1;
    return availableDevices[index].id;
}

int ControlMapping_GetPlayerMappingCount(int player_num) {
    auto it = player_mappings.find(player_num);
    if (it == player_mappings.end()) return 0;
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

} // extern "C"

