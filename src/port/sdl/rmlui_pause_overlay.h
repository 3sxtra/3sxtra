#pragma once
/**
 * @file rmlui_pause_overlay.h
 * @brief RmlUi pause text overlay — "1P PAUSE" / "2P PAUSE" blink
 *        and controller-disconnected message.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_pause_overlay_init(void);
void rmlui_pause_overlay_update(void);
void rmlui_pause_overlay_shutdown(void);

#ifdef __cplusplus
}
#endif
