/**
 * @file imgui_font.cpp
 * @brief ImGui font initialization and Japanese glyph range loader.
 */
#include "port/imgui_font.h"
#include "imgui.h"
#include <stdio.h>

/** @brief Verify that ImGui is initialized (context exists). */
bool ImGuiFont_Init(void) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return false;
    }
    return true;
}

/** @brief Load a TTF font with Japanese glyph coverage into ImGui. */
bool ImGuiFont_LoadJapaneseFont(const char* fontPath, float size) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    // Check if file exists to prevent ImGui from asserting and crashing
    FILE* f = fopen(fontPath, "rb");
    if (!f) {
        fprintf(stderr, "Failed to load font from: %s (file not found)\n", fontPath);
        return false;
    }
    fclose(f);

    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, size, NULL, io.Fonts->GetGlyphRangesJapanese());

    if (font == nullptr) {
        fprintf(stderr, "Failed to load font from: %s\n", fontPath);
        return false;
    }

    return true;
}
