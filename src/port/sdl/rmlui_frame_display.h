/**
 * @file rmlui_frame_display.h
 * @brief RmlUi frame meter overlay — data-bound replacement for frame_display.cpp.
 *
 * Shows a color-coded frame bar with startup/active/recovery/hitstun states
 * and advantage stats. Active when ui-mode=rmlui.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Register the frame_display data model and load the document.
/// Call once after rmlui_wrapper_init().
void rmlui_frame_display_init(void);

/// Per-frame update: record frame states, rebuild bar, dirty-check data model.
/// Call each frame (handles visibility internally via show_frame_meter setting).
void rmlui_frame_display_update(void);

/// Clean up the data model.
void rmlui_frame_display_shutdown(void);

#ifdef __cplusplus
}
#endif
