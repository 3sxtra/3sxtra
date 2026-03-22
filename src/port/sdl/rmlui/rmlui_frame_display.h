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
#ifdef ENABLE_RMLUI

void rmlui_frame_display_init(void);

/// Per-frame update: record frame states, rebuild bar, dirty-check data model.
/// Call each frame (handles visibility internally via show_frame_meter setting).
void rmlui_frame_display_update(void);

/// Clean up the data model.
void rmlui_frame_display_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_frame_display_init(void) {}
static inline void rmlui_frame_display_update(void) {}
static inline void rmlui_frame_display_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
