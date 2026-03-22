/**
 * @file rmlui_attract_overlay.h
 * @brief RmlUi attract demo overlay — small logo + "PRESS START" during CPU fights.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_attract_overlay_init(void);
void rmlui_attract_overlay_show(void);
void rmlui_attract_overlay_hide(void);
void rmlui_attract_overlay_show_logo(void);
void rmlui_attract_overlay_hide_logo(void);
void rmlui_attract_overlay_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_attract_overlay_init(void) {}
static inline void rmlui_attract_overlay_show(void) {}
static inline void rmlui_attract_overlay_hide(void) {}
static inline void rmlui_attract_overlay_show_logo(void) {}
static inline void rmlui_attract_overlay_hide_logo(void) {}
static inline void rmlui_attract_overlay_shutdown(void) {}

#endif

#ifdef __cplusplus
}
#endif
