#pragma once
/**
 * @file rmlui_pause_overlay.h
 * @brief RmlUi pause text overlay — "1P PAUSE" / "2P PAUSE" blink
 *        and controller-disconnected message.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_pause_overlay_init(void);
void rmlui_pause_overlay_update(void);
void rmlui_pause_overlay_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_pause_overlay_init(void) {}
static inline void rmlui_pause_overlay_update(void) {}
static inline void rmlui_pause_overlay_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
