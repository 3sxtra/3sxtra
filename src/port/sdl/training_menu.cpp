/**
 * @file training_menu.cpp
 * @brief ImGui overlay for editing Training Options (F7).
 * Replicates the options from the original Lua script.
 */
#include "port/sdl/training_menu.h"
#include "imgui.h"
#include "port/config.h"
#include <cstdio>

bool show_training_menu = false;
TrainingMenuSettings g_training_menu_settings = { .show_hitboxes = true,
                                                  .show_pushboxes = true,
                                                  .show_hurtboxes = true,
                                                  .show_attackboxes = true,
                                                  .show_throwboxes = true,
                                                  .show_advantage = false,
                                                  .show_stun = true,
                                                  .show_inputs = true,
                                                  .show_frame_meter = true };

extern "C" void training_menu_init(void) {
    g_training_menu_settings.show_hitboxes = Config_GetBool(CFG_KEY_TRAINING_HITBOXES);
    g_training_menu_settings.show_pushboxes = Config_GetBool(CFG_KEY_TRAINING_PUSHBOXES);
    g_training_menu_settings.show_hurtboxes = Config_GetBool(CFG_KEY_TRAINING_HURTBOXES);
    g_training_menu_settings.show_attackboxes = Config_GetBool(CFG_KEY_TRAINING_ATTACKBOXES);
    g_training_menu_settings.show_throwboxes = Config_GetBool(CFG_KEY_TRAINING_THROWBOXES);
    g_training_menu_settings.show_advantage = Config_GetBool(CFG_KEY_TRAINING_ADVANTAGE);
    g_training_menu_settings.show_stun = Config_GetBool(CFG_KEY_TRAINING_STUN);
    g_training_menu_settings.show_inputs = Config_GetBool(CFG_KEY_TRAINING_INPUTS);
    g_training_menu_settings.show_frame_meter = Config_GetBool(CFG_KEY_TRAINING_FRAME_METER);
}

extern "C" void training_menu_shutdown(void) {
    // Cleanup for training menu
}

// Helper to center text
static void render_centered_text(const char* text) {
    ImVec2 text_size = ImGui::CalcTextSize(text);
    float window_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - text_size.x) * 0.5f);
    ImGui::TextUnformatted(text);
}

static void HelpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

extern "C" void training_menu_render(int window_width, int window_height) {
    if (!show_training_menu)
        return;

    /* Match other menus font scaling */
    float font_scale = (float)window_height / 480.0f;
    ImGui::GetIO().FontGlobalScale = font_scale;

    ImGui::SetNextWindowSize(ImVec2(400 * font_scale, 350 * font_scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Training Options (F7)", &show_training_menu)) {

        render_centered_text("TRAINING OPTIONS");
        ImGui::Separator();
        if (ImGui::Checkbox("Master Hitboxes Toggle", &g_training_menu_settings.show_hitboxes)) {
            Config_SetBool(CFG_KEY_TRAINING_HITBOXES, g_training_menu_settings.show_hitboxes);
            Config_Save();
        }
        HelpMarker("Master switch to enable rendering collision data overlays.");

        if (g_training_menu_settings.show_hitboxes) {
            ImGui::Indent();
            if (ImGui::Checkbox("Pushboxes (Green)", &g_training_menu_settings.show_pushboxes)) {
                Config_SetBool(CFG_KEY_TRAINING_PUSHBOXES, g_training_menu_settings.show_pushboxes);
                Config_Save();
            }
            HelpMarker("Shows character mass / collision boundary (Green).");

            if (ImGui::Checkbox("Hurtboxes (Blue)", &g_training_menu_settings.show_hurtboxes)) {
                Config_SetBool(CFG_KEY_TRAINING_HURTBOXES, g_training_menu_settings.show_hurtboxes);
                Config_Save();
            }
            HelpMarker("Shows vulnerable areas where characters take damage (Blue).");

            if (ImGui::Checkbox("Hitboxes (Red)", &g_training_menu_settings.show_attackboxes)) {
                Config_SetBool(CFG_KEY_TRAINING_ATTACKBOXES, g_training_menu_settings.show_attackboxes);
                Config_Save();
            }
            HelpMarker("Shows active attacking areas that deal damage (Red).");

            if (ImGui::Checkbox("Throwboxes (Yellow/Pink)", &g_training_menu_settings.show_throwboxes)) {
                Config_SetBool(CFG_KEY_TRAINING_THROWBOXES, g_training_menu_settings.show_throwboxes);
                Config_Save();
            }
            HelpMarker("Shows throw grabs (Yellow) and throwable vulnerability bounds (Pink).");
            ImGui::Unindent();
        }

        ImGui::Spacing();
        if (ImGui::Checkbox("Show Frame Advantage", &g_training_menu_settings.show_advantage)) {
            Config_SetBool(CFG_KEY_TRAINING_ADVANTAGE, g_training_menu_settings.show_advantage);
            Config_Save();
        }
        HelpMarker("Display +/- frame advantage numbers upon attack completion.");

        if (ImGui::Checkbox("Show Stun Timer", &g_training_menu_settings.show_stun)) {
            Config_SetBool(CFG_KEY_TRAINING_STUN, g_training_menu_settings.show_stun);
            Config_Save();
        }
        HelpMarker("Show the numeric stun countdown over the character's head.");

        if (ImGui::Checkbox("Show Input History", &g_training_menu_settings.show_inputs)) {
            Config_SetBool(CFG_KEY_TRAINING_INPUTS, g_training_menu_settings.show_inputs);
            Config_Save();
        }
        HelpMarker("Display a scrolling history of player inputs with frame durations.");

        if (ImGui::Checkbox("Show Frame Meter", &g_training_menu_settings.show_frame_meter)) {
            Config_SetBool(CFG_KEY_TRAINING_FRAME_METER, g_training_menu_settings.show_frame_meter);
            Config_Save();
        }
        HelpMarker("Display a visual timeline of frame data (Startup, Active, Recovery).");

        ImGui::Spacing();
        ImGui::Separator();

        render_centered_text("Press F7 to close this menu");
    }
    ImGui::End();

    /* Reset global font scale */
    ImGui::GetIO().FontGlobalScale = 1.0f;
}
