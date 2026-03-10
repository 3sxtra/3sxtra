/**
 * @file rmlui_training_hud.h
 * @brief Always-on RmlUi training HUD overlay — text overlays for stun, life, meter, frame advantage.
 *
 * Displayed during training mode (not toggled by F7).
 * Data is pushed from Lua via engine.set_hud_text().
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Register the training_hud data model and load the training_hud.rml document.
void rmlui_training_hud_init(void);

/// Per-frame update: push dirty data to RmlUI.
void rmlui_training_hud_update(void);

/// Clean up.
void rmlui_training_hud_shutdown(void);

/// Set a named HUD text field from Lua. Thread-safe for single-thread use.
/// field: one of "p1_stun", "p2_stun", "p1_life", "p2_life", etc.
void rmlui_training_hud_set_text(const char* field, const char* value);

/// Set a named HUD gauge from Lua.
/// field: e.g. "p1_parry_fwd_validity"
/// fill: 0.0-1.0
void rmlui_training_hud_set_gauge(const char* field, float fill);

#ifdef __cplusplus
}
#endif
