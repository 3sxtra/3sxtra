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

#if defined(ENABLE_RMLUI) && defined(ENABLE_LIBRASHADER)

void rmlui_shader_menu_init(void);
void rmlui_shader_menu_update(void);
void rmlui_shader_menu_shutdown(void);

#else /* !ENABLE_RMLUI || !ENABLE_LIBRASHADER */

static inline void rmlui_shader_menu_init(void) {}
static inline void rmlui_shader_menu_update(void) {}
static inline void rmlui_shader_menu_shutdown(void) {}

#endif /* ENABLE_RMLUI && ENABLE_LIBRASHADER */

#ifdef __cplusplus
}
#endif
