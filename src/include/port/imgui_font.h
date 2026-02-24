#ifndef PORT_IMGUI_FONT_H
#define PORT_IMGUI_FONT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the ImGui Font system
bool ImGuiFont_Init(void);

// Load the default Japanese font
bool ImGuiFont_LoadJapaneseFont(const char* fontPath, float size);

#ifdef __cplusplus
}
#endif

#endif // PORT_IMGUI_FONT_H
