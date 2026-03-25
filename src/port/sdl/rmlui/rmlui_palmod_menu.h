/**
 * @file rmlui_palmod_menu.h
 * @brief RmlUi palette modification overlay menu — F10 toggle.
 *
 * Provides in-game palette cycling for both players via an RmlUi data model.
 * Palette changes are visual-only (write to ColorRAM + ghost update) and
 * persist across sessions via Config_SetInt / Config_GetInt.
 */
#ifndef RMLUI_PALMOD_MENU_H
#define RMLUI_PALMOD_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

/// Register the palmod data model and load the palmod.rml document.
/// Call once after rmlui_wrapper_init().
void rmlui_palmod_menu_init(void);

/// Per-frame update: dirty-check palette state and push changes to the data model.
void rmlui_palmod_menu_update(void);

/// Flush any pending config changes to disk (call on menu close).
void rmlui_palmod_menu_flush_config(void);

/// Clean up the palmod data model.
void rmlui_palmod_menu_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_palmod_menu_init(void) {}
static inline void rmlui_palmod_menu_update(void) {}
static inline void rmlui_palmod_menu_flush_config(void) {}
static inline void rmlui_palmod_menu_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif

#endif /* RMLUI_PALMOD_MENU_H */
