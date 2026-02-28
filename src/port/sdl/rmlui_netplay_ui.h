/**
 * @file rmlui_netplay_ui.h
 * @brief RmlUi netplay overlay — HUD, diagnostics, and toast notifications.
 *
 * Data-bound replacement for the ImGui rendering in sdl_netplay_ui.cpp.
 * The lobby state machine and C extern API remain in sdl_netplay_ui.cpp;
 * this module only handles the RmlUi presentation layer.
 * Active when ui-mode=rmlui.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Register the netplay data model and load the document.
/// Call once after rmlui_wrapper_init().
void rmlui_netplay_ui_init(void);

/// Per-frame update: sync HUD/diagnostics/toast state, dirty-check data model.
/// Call each frame (handles visibility internally).
void rmlui_netplay_ui_update(void);

/// Clean up the data model.
void rmlui_netplay_ui_shutdown(void);

#ifdef __cplusplus
}
#endif
