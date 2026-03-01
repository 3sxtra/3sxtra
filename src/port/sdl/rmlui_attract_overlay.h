/**
 * @file rmlui_attract_overlay.h
 * @brief RmlUi attract demo overlay — small logo + "PRESS START" during CPU fights.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_attract_overlay_init(void);
void rmlui_attract_overlay_show(void);
void rmlui_attract_overlay_hide(void);
void rmlui_attract_overlay_show_logo(void);
void rmlui_attract_overlay_hide_logo(void);
void rmlui_attract_overlay_shutdown(void);

#ifdef __cplusplus
}
#endif
