/**
 * @file rmlui_shader_menu.h
 * @brief RmlUi shader configuration menu — extern C API.
 *
 * Provides the same shader/broadcast settings as shader_menu.h
 * but rendered via RmlUi data bindings instead of ImGui.
 * Toggled with F2 when --ui rmlui is active.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_shader_menu_init(void);
void rmlui_shader_menu_update(void);
void rmlui_shader_menu_shutdown(void);

#ifdef __cplusplus
}
#endif
