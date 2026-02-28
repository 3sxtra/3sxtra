#pragma once
/**
 * @file rmlui_game_hud.h
 * @brief RmlUi in-game fight HUD — data model for health, timer, stun,
 *        SA gauge, combo, player names, and win pips.
 *
 * Mirrors the CPS3 Disp_Cockpit rendering as an HTML/CSS overlay.
 * All Phase 3 per-component toggle globals are also defined in the
 * companion rmlui_game_hud.cpp and declared in rmlui_phase3_toggles.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the fight HUD data model and load the document. */
void rmlui_game_hud_init(void);

/** Per-frame dirty-check sync — call once per frame during gameplay. */
void rmlui_game_hud_update(void);

/** Destroy the data model and unload the document. */
void rmlui_game_hud_shutdown(void);

#ifdef __cplusplus
}
#endif
