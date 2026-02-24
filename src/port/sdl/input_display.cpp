/**
 * @file input_display.cpp
 * @brief On-screen input history display using sprite-sheet icons.
 *
 * Tracks per-frame input state for both players and renders a scrolling
 * history of directional/button icons using UV-mapped regions of a
 * pre-loaded sprite sheet.
 */
#include "port/sdl/input_display.h"

#include "common.h"
#include "imgui.h"
#include "imgui_wrapper.h"

#include "port/sdl/sdl_app.h"
#include "port/sdl/training_menu.h"

#include <map>
#include <string>
#include <vector>

// Forward declare the global input buffer from the game
extern "C" u16 p1sw_buff;
extern "C" u16 p2sw_buff;

static void* capcom_icons_texture = nullptr;
static u32 last_input = 0;
static u32 last_input_p2 = 0;

// UV coordinates for each icon in the sprite sheet
// Assumes a 32x512 texture, so each icon is 32x32
const float ICON_WIDTH = 32.0f;
const float ICON_HEIGHT = 32.0f;
// Texture dimensions will be queried at runtime

#define UV_RECT(y_offset)                                                                                              \
    ImVec2(0.0f, (y_offset * ICON_HEIGHT) / 544.0f), ImVec2(0.5f, ((y_offset + 1) * ICON_HEIGHT) / 544.0f)

static const std::map<u32, std::pair<ImVec2, ImVec2>> action_to_uv = {
    // Directions (handle diagonals separately)
    { 0x4, { UV_RECT(0) } }, // Left
    { 0x8, { UV_RECT(1) } }, // Right
    { 0x1, { UV_RECT(2) } }, // Up
    { 0x2, { UV_RECT(3) } }, // Down
    // Diagonal Directions
    { 0x1 | 0x4, { UV_RECT(4) } }, // Up-Left
    { 0x1 | 0x8, { UV_RECT(5) } }, // Up-Right
    { 0x2 | 0x4, { UV_RECT(6) } }, // Down-Left
    { 0x2 | 0x8, { UV_RECT(7) } }, // Down-Right
    // Punches
    { 0x10, { UV_RECT(8) } },  // Light Punch
    { 0x20, { UV_RECT(9) } },  // Medium Punch
    { 0x40, { UV_RECT(10) } }, // Hard Punch
    // Kicks
    { 0x100, { UV_RECT(11) } }, // Light Kick
    { 0x200, { UV_RECT(12) } }, // Medium Kick
    { 0x400, { UV_RECT(13) } }, // Hard Kick
    // Other
    { 0x1000, { UV_RECT(14) } } // Start
};

struct InputInfo {
    u32 mask;
    s32 frame;
};

static std::vector<InputInfo> input_history;
static std::vector<InputInfo> input_history_p2;
const size_t MAX_HISTORY_SIZE = 10;
static s32 s_render_frame = 0;
static s32 s_last_input_frame_p1 = 0;
static s32 s_last_input_frame_p2 = 0;
const s32 INACTIVITY_TIMEOUT_FRAMES = 60; // ~1 second at 60fps

void input_display_init() {
    capcom_icons_texture = imgui_wrapper_get_capcom_icons_texture();
}

static const std::vector<u32> ordered_actions = {
    0x10,  // Light Punch
    0x20,  // Medium Punch
    0x40,  // Hard Punch
    0x100, // Light Kick
    0x200, // Medium Kick
    0x400, // Hard Kick
    0x1000 // Start
};

void input_display_render() {
    if (!capcom_icons_texture || !g_training_menu_settings.show_inputs)
        return;

    s_render_frame++;

    ImGuiIO& io = ImGui::GetIO();
    SDL_FRect game_rect = get_letterbox_rect((int)io.DisplaySize.x, (int)io.DisplaySize.y);

    float scale = (game_rect.h / 480.0f) * 0.85f; // 15% smaller than native
    if (scale <= 0.1f)
        scale = 0.1f;

    float original_font_scale = io.FontGlobalScale;
    io.FontGlobalScale = scale;

    float scaled_icon_width = ICON_WIDTH * scale;
    float scaled_icon_height = ICON_HEIGHT * scale;

    u32 current_input = p1sw_buff;

    // Only add non-neutral inputs that are different from the last one
    if (current_input != last_input) {
        input_history.push_back({ current_input, s_render_frame });
        s_last_input_frame_p1 = s_render_frame;

        if (input_history.size() > MAX_HISTORY_SIZE) {
            input_history.erase(input_history.begin());
        }
    }
    last_input = current_input;

    u32 current_input_p2 = p2sw_buff;
    if (current_input_p2 != last_input_p2) {
        input_history_p2.push_back({ current_input_p2, s_render_frame });
        s_last_input_frame_p2 = s_render_frame;

        if (input_history_p2.size() > MAX_HISTORY_SIZE) {
            input_history_p2.erase(input_history_p2.begin());
        }
    }
    last_input_p2 = current_input_p2;

    // Clear history after 2 seconds of inactivity
    if (!input_history.empty() && (s_render_frame - s_last_input_frame_p1) > INACTIVITY_TIMEOUT_FRAMES) {
        input_history.clear();
    }
    if (!input_history_p2.empty() && (s_render_frame - s_last_input_frame_p2) > INACTIVITY_TIMEOUT_FRAMES) {
        input_history_p2.clear();
    }

    auto render_history = [&](const std::vector<InputInfo>& history, bool is_p2) {
        // Iterate backwards: newest inputs first
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            const auto& info = *it;
            u32 input_mask = info.mask;

            // Calculate frame duration
            s32 next_frame;
            if (it == history.rbegin()) {        // Newest item
                next_frame = s_render_frame + 1; // +1 because current frame counts as 1
            } else {                             // Older items calculate against the next item chronologically
                auto next_it = it - 1;
                next_frame = next_it->frame;
            }
            s32 frame_diff = next_frame - info.frame;

            char diff_str[16] = "-";
            if (frame_diff < 999) {
                snprintf(diff_str, sizeof(diff_str), "%d", frame_diff);
            }

            // Height of the upcoming image row
            float max_row_height = scaled_icon_height;
            float text_y_offset = (max_row_height - ImGui::GetFontSize()) * 0.5f;

            // Layout: Right side (P2) -> Text Left, Icons Right // Left side (P1) -> Icons Left, Text Right
            bool first_icon_on_line = true;

            if (is_p2) {
                // To safely align text vertically without breaking ImGui bounds, use empty Dummy spacers
                ImGui::Dummy(ImVec2(0.0f, text_y_offset));
                ImGui::SameLine();
                ImGui::Text("%3s", diff_str);
                ImGui::SameLine();
                // Push cursor back up to draw images on same logic line
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - text_y_offset);
                first_icon_on_line = false; // Text already on this line, icons must use SameLine
            }

            // Handle directional inputs first
            u32 directional = input_mask & 0xF;
            if (directional != 0) {
                auto dir_uv = action_to_uv.find(directional);
                if (dir_uv != action_to_uv.end()) {
                    if (!first_icon_on_line) {
                        ImGui::SameLine(0.0f, 4.0f * scale);
                    }
                    ImGui::Image(capcom_icons_texture,
                                 ImVec2(scaled_icon_width, scaled_icon_height),
                                 dir_uv->second.first,
                                 dir_uv->second.second);
                    first_icon_on_line = false;
                }
            }

            // Handle action inputs (punches, kicks, etc.)
            u32 actions = input_mask & ~0xF;
            if (actions != 0) {
                for (const auto& action_bit : ordered_actions) {
                    if (actions & action_bit) {
                        auto act_uv = action_to_uv.find(action_bit);
                        if (act_uv != action_to_uv.end()) {
                            if (!first_icon_on_line) {
                                ImGui::SameLine(0.0f, 4.0f * scale); // 4px scaled spacing
                            }
                            ImGui::Image(capcom_icons_texture,
                                         ImVec2(scaled_icon_width, scaled_icon_height),
                                         act_uv->second.first,
                                         act_uv->second.second);
                            first_icon_on_line = false;
                        }
                    }
                }
            }

            // Layout: Left side (P1) -> Icons Left, Text Right
            if (!is_p2) {
                ImGui::SameLine();
                float current_y = ImGui::GetCursorPosY();
                ImGui::SetCursorPosY(current_y + text_y_offset);
                ImGui::Text("%-3s", diff_str);
                // Advance to the next row cleanly by placing a dummy at the bottom of the tallest element
                ImGui::SetCursorPosY(current_y + max_row_height);
                ImGui::Dummy(ImVec2(0.0f, 4.0f * scale)); // Vertical spacing between rows
            } else {
                // For P2, also inject the vertical spacing correctly
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f * scale);
                ImGui::Dummy(ImVec2(0.0f, 0.0f)); // Required by ImGui after SetCursorPos to grow window bounds
            }
        }
    };

    // Removed safety cap on max scale so it smoothly scales on 4K/high-res desktop without breaking physical game
    // proportions.

    // Render the input history
    // P1 on Left
    ImGui::SetNextWindowPos(ImVec2(game_rect.x + (10 * scale), game_rect.y + (100 * scale)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(120 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    // Explicitly kill borders to get rid of red line boundary artifact
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("Input Display P1",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoBackground)) {
        render_history(input_history, false);
    }
    ImGui::End();

    // P2 on Right
    // Pivot (1,0) means pos is Top-Right corner.
    ImGui::SetNextWindowPos(ImVec2(game_rect.x + game_rect.w - (10 * scale), game_rect.y + (100 * scale)),
                            ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(120 * scale, 400 * scale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Input Display P2",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoBackground)) {
        render_history(input_history_p2, true);
    }
    ImGui::End();

    ImGui::PopStyleVar();

    io.FontGlobalScale = original_font_scale;
}

void input_display_shutdown() {
    // In the current architecture, textures are managed by the imgui_wrapper,
    // so we don't need to explicitly unload them here. That will be handled
    // in `imgui_wrapper_shutdown`.
}
