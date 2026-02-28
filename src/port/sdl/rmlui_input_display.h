/**
 * @file rmlui_input_display.h
 * @brief RmlUi input history overlay — data-bound replacement for input_display.cpp.
 *
 * Shows per-player input history using text notation (arrows + button labels)
 * rendered via RmlUi data bindings. Active when ui-mode=rmlui.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Register the input_display data model and load the document.
/// Call once after rmlui_wrapper_init().
void rmlui_input_display_init(void);

/// Per-frame update: track inputs, rebuild history, dirty-check data model.
/// Call each frame (handles visibility internally via show_inputs setting).
void rmlui_input_display_update(void);

/// Clean up the data model.
void rmlui_input_display_shutdown(void);

#ifdef __cplusplus
}
#endif
