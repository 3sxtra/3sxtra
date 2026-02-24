/**
 * @file shader_menu.cpp
 * @brief ImGui shader and broadcast settings menu.
 *
 * Provides an F2-toggled overlay for selecting internal vs Libretro
 * shaders, adjusting scale modes, browsing .slangp presets, and
 * enabling/disabling video broadcast output.
 */
#include "port/sdl/shader_menu.h"
#include "imgui.h"
#include "port/broadcast.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

extern BroadcastConfig broadcast_config;

// Forward declarations for accessing sdl_app.c state
extern "C" {
// Shader mode getters/setters
int SDLApp_GetScaleMode();
void SDLApp_SetScaleMode(int mode);
const char* SDLApp_GetScaleModeName(int mode);

// Obsolete shader modes removed

bool SDLApp_GetShaderModeLibretro();
void SDLApp_SetShaderModeLibretro(bool libretro);

int SDLApp_GetCurrentPresetIndex();
void SDLApp_SetCurrentPresetIndex(int index);
int SDLApp_GetAvailablePresetCount();
const char* SDLApp_GetPresetName(int index);

void SDLApp_LoadPreset(int index);

// VSync control
void SDLApp_SetVSync(bool enabled);
bool SDLApp_IsVSyncEnabled();
}

// UI state
static char search_filter[256] = "";

static void render_centered_text(const char* text) {
    ImVec2 text_size = ImGui::CalcTextSize(text);
    float window_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - text_size.x) * 0.5f);
    ImGui::TextUnformatted(text);
}

extern "C" void shader_menu_init() {
    // Nothing to initialize yet
}

extern "C" void shader_menu_render(int window_width, int window_height) {
    // Apply a global font scale based on window height.
    // This ensures all text (main window, popups, child windows) scales consistently.
    float font_scale = (float)window_height / 480.0f;
    ImGui::GetIO().FontGlobalScale = font_scale;

    ImVec2 window_size((float)window_width, (float)window_height);
    ImVec2 window_pos(0, 0);

    ImGui::SetNextWindowPos(window_pos);
    ImGui::SetNextWindowSize(window_size);
    ImGui::Begin("Shader Configuration",
                 NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Title
    render_centered_text("SHADER CONFIGURATION");
    ImGui::Separator();

    // Shader Mode Toggle
    ImGui::Spacing();
    ImGui::Text("Shader System:");
    ImGui::SameLine();

    bool is_libretro = SDLApp_GetShaderModeLibretro();
    const char* mode_items[] = { "Internal Shaders", "Libretro Shaders" };
    int current_mode = is_libretro ? 1 : 0;

    if (ImGui::Combo("##ShaderSystem", &current_mode, mode_items, 2)) {
        SDLApp_SetShaderModeLibretro(current_mode == 1);
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (!is_libretro) {
        // ===== INTERNAL SHADERS SECTION =====

        if (ImGui::CollapsingHeader("Scale Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            int current_scale = SDLApp_GetScaleMode();

            for (int i = 0; i < 5; i++) { // SCALEMODE_COUNT = 5
                const char* name = SDLApp_GetScaleModeName(i);
                if (name) {
                    if (ImGui::RadioButton(name, current_scale == i)) {
                        SDLApp_SetScaleMode(i);
                    }
                }
            }
        }

        ImGui::Spacing();

    } else {
        // ===== LIBRETRO SHADERS SECTION =====

        ImGui::Text("Search:");
        ImGui::SameLine();
        ImGui::InputText("##Search", search_filter, sizeof(search_filter));

        ImGui::Spacing();

        int preset_count = SDLApp_GetAvailablePresetCount();
        int current_preset = SDLApp_GetCurrentPresetIndex();

        if (preset_count == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No Libretro shader presets found!");
        } else {
            ImGui::Text("Available Presets (%d):", preset_count);

            if (ImGui::BeginChild("PresetList", ImVec2(0, window_height * 0.6f), true)) {
                std::string filter_lower = search_filter;
                std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

                for (int i = 0; i < preset_count; i++) {
                    const char* preset_name = SDLApp_GetPresetName(i);
                    if (!preset_name)
                        continue;

                    // Apply search filter
                    if (filter_lower.length() > 0) {
                        std::string name_lower = preset_name;
                        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                        if (name_lower.find(filter_lower) == std::string::npos) {
                            continue;
                        }
                    }

                    bool is_selected = (i == current_preset);

                    if (is_selected) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                    }

                    if (ImGui::Selectable(preset_name, is_selected)) {
                        SDLApp_SetCurrentPresetIndex(i);
                        SDLApp_LoadPreset(i);
                    }

                    if (is_selected) {
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemVisible()) {
                            ImGui::SetScrollHereY(0.5f);
                        }
                    }
                }

                ImGui::EndChild();
            }

            ImGui::Spacing();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ===== DISPLAY SECTION =====
    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool vsync = SDLApp_IsVSyncEnabled();
        if (ImGui::Checkbox("VSync", &vsync)) {
            SDLApp_SetVSync(vsync);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Prevents screen tearing. Does not affect game speed.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ===== BROADCAST SECTION =====
    if (ImGui::CollapsingHeader("Video Broadcast", ImGuiTreeNodeFlags_None)) {
        bool enabled = broadcast_config.enabled;
        if (ImGui::Checkbox("Enable Broadcast", &enabled)) {
            broadcast_config.enabled = enabled;
            // Broadcast_Update() will catch this in the next frame
        }

        if (enabled) {
            ImGui::Indent();

            int source = (int)broadcast_config.source;
            const char* source_items[] = { "Native (384x224)", "Final Output (Scaled)" };
            if (ImGui::Combo("Source", &source, source_items, 2)) {
                broadcast_config.source = (BroadcastSource)source;
            }

            if (broadcast_config.source == BROADCAST_SOURCE_FINAL) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                                   "Warning: Final Output broadcast may have performance impact.");
            }

            // bool show_ui = broadcast_config.show_ui;
            // if (ImGui::Checkbox("Show UI in Broadcast", &show_ui)) {
            //    broadcast_config.show_ui = show_ui;
            // }

            ImGui::Unindent();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    render_centered_text("F2 Close  |  F8 Scale Mode  |  F9 Next Preset");

    ImGui::End();

    // Reset global font scale to avoid affecting other ImGui elements outside this menu
    ImGui::GetIO().FontGlobalScale = 1.0f;
}

extern "C" void shader_menu_shutdown() {
    // Nothing to clean up yet
}
