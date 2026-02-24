/**
 * @file stage_config_menu.cpp
 * @brief ImGui overlay for editing HD stage configuration (F6).
 */
#include "port/sdl/stage_config_menu.h"
#include "imgui.h"
#include "port/modded_stage.h"
#include "port/stage_config.h"
#include <cstdio>

bool show_stage_config_menu = false;

extern "C" void stage_config_menu_init(void) {
    // No special init needed currently
}

extern "C" void stage_config_menu_shutdown(void) {
    // No special cleanup needed
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

extern "C" void stage_config_menu_render(int window_width, int window_height) {
    if (!show_stage_config_menu)
        return;

    /* Match other menus font scaling */
    float font_scale = (float)window_height / 480.0f;
    ImGui::GetIO().FontGlobalScale = font_scale;

    ImGui::SetNextWindowSize(ImVec2(400 * font_scale, 350 * font_scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("HD Stage Config (F6)", &show_stage_config_menu)) {

        int stage_idx = ModdedStage_GetLoadedStageIndex();
        if (stage_idx < 0) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No active HD stage loaded.");
            if (ImGui::Button("Close"))
                show_stage_config_menu = false;
            ImGui::End();
            ImGui::GetIO().FontGlobalScale = 1.0f;
            return;
        }

        ImGui::Text("Creating config for Stage %02d", stage_idx);

        if (ImGui::Button("Save Config")) {
            StageConfig_Save(stage_idx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Config")) {
            StageConfig_Load(stage_idx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults")) {
            ImGui::OpenPopup("Confirm Reset");
        }

        if (ImGui::BeginPopupModal("Confirm Reset", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Reset all layers to defaults?");
            ImGui::Text("Unsaved changes will be lost.");
            ImGui::Separator();
            if (ImGui::Button("Reset", ImVec2(120, 0))) {
                StageConfig_Init();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        if (ImGui::BeginTabBar("Layers")) {
            for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Layer %d", i);
                if (ImGui::BeginTabItem(label)) {
                    StageLayerConfig* layer = &g_stage_config.layers[i];

                    ImGui::Checkbox("Enabled", &layer->enabled);
                    ImGui::InputText("Filename", layer->filename, sizeof(layer->filename));

                    const char* modes[] = { "Fit Height (Default)", "Stretch", "Native", "Manual" };
                    int mode = (int)layer->scale_mode;
                    if (ImGui::Combo("Scale Mode", &mode, modes, 4)) {
                        layer->scale_mode = (LayerScaleMode)mode;
                    }

                    if (layer->scale_mode == SCALE_MODE_MANUAL) {
                        ImGui::DragFloat("Scale X", &layer->scale_factor_x, 0.01f, 0.1f, 10.0f);
                        ImGui::DragFloat("Scale Y", &layer->scale_factor_y, 0.01f, 0.1f, 10.0f);
                    } else if (layer->scale_mode == SCALE_MODE_FIT_HEIGHT) {
                        ImGui::TextDisabled("Scale is auto-calculated based on height.");
                    }

                    ImGui::Separator();
                    ImGui::Text("Parallax (1.0 = Follows Camera)");
                    HelpMarker("Multiplies the native camera speed. 1.0 moves at the same speed as the foreground. "
                               "Lower values move slower (background), higher values move faster (foreground).");
                    ImGui::DragFloat("Para X", &layer->parallax_x, 0.01f, 0.0f, 2.0f);
                    ImGui::DragFloat("Para Y", &layer->parallax_y, 0.01f, 0.0f, 2.0f);

                    ImGui::SliderInt("Original Layer ID", &layer->original_bg_index, -1, 7, "%d");
                    HelpMarker("Which original game layer's speed to use.\n-1 = No movement/Manual\n0-7 = Track "
                               "specific original layer's speed.");

                    ImGui::Separator();
                    ImGui::DragFloat("Offset X", &layer->offset_x, 1.0f);
                    HelpMarker("Base position offset in pixels.");
                    ImGui::DragFloat("Offset Y", &layer->offset_y, 1.0f);

                    ImGui::Separator();
                    ImGui::DragInt("Z-Index", &layer->z_index, 1);

                    ImGui::Checkbox("Loop X", &layer->loop_x);
                    ImGui::SameLine();
                    ImGui::Checkbox("Loop Y", &layer->loop_y);

                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    /* Reset global font scale */
    ImGui::GetIO().FontGlobalScale = 1.0f;
}
