/**
 * @file rmlui_dev_overlay.h
 * @brief RmlUi developer overlay — live element position & style inspector.
 *
 * Provides an in-game overlay for fast-tracking RmlUi development by
 * live-adjusting element positions and styles without recompilation.
 * Toggled with F10 when --ui rmlui is active.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_dev_overlay_init(void);
void rmlui_dev_overlay_update(void);
void rmlui_dev_overlay_shutdown(void);

extern bool show_dev_overlay;

#else /* !ENABLE_RMLUI */

static inline void rmlui_dev_overlay_init(void) {}
static inline void rmlui_dev_overlay_update(void) {}
static inline void rmlui_dev_overlay_shutdown(void) {}
static const bool show_dev_overlay = false;

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
