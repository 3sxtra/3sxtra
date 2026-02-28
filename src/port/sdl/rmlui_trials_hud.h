#pragma once
/**
 * @file rmlui_trials_hud.h
 * @brief RmlUi trial mode HUD — step list, completion banner, gauge alert.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_trials_hud_init(void);
void rmlui_trials_hud_update(void);
void rmlui_trials_hud_shutdown(void);

#ifdef __cplusplus
}
#endif
