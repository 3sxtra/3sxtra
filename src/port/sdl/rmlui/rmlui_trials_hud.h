#pragma once
/**
 * @file rmlui_trials_hud.h
 * @brief RmlUi trial mode HUD — step list, completion banner, gauge alert.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_trials_hud_init(void);
void rmlui_trials_hud_update(void);
void rmlui_trials_hud_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_trials_hud_init(void) {}
static inline void rmlui_trials_hud_update(void) {}
static inline void rmlui_trials_hud_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
