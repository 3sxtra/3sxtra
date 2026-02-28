/**
 * @file rmlui_mods_menu.h
 * @brief RmlUi mods overlay menu — data-bound HTML/CSS replacement for mods_menu.cpp.
 *
 * Provides the same functionality as the ImGui mods menu (F3 toggle) but uses
 * RmlUi documents + data bindings. Active when ui-mode=rmlui.
 */
#ifndef RMLUI_MODS_MENU_H
#define RMLUI_MODS_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

/// Register the mods data model and load the mods.rml document.
/// Call once after rmlui_wrapper_init().
void rmlui_mods_menu_init(void);

/// Per-frame update: dirty-check game state and push changes to the data model.
/// Call each frame when the mods menu is visible.
void rmlui_mods_menu_update(void);

/// Clean up the mods data model.
void rmlui_mods_menu_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* RMLUI_MODS_MENU_H */
