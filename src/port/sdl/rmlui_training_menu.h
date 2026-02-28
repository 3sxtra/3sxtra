/**
 * @file rmlui_training_menu.h
 * @brief RmlUi training options overlay — data-bound replacement for training_menu.cpp.
 *
 * Provides the same Training Options (F7) functionality as the ImGui version,
 * using RmlUi documents + data bindings. Active when ui-mode=rmlui.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Register the training data model and load the training.rml document.
/// Call once after rmlui_wrapper_init().
void rmlui_training_menu_init(void);

/// Per-frame update: dirty-check settings and push changes to the data model.
/// Call each frame when the training menu is visible.
void rmlui_training_menu_update(void);

/// Clean up the training data model.
void rmlui_training_menu_shutdown(void);

#ifdef __cplusplus
}
#endif
