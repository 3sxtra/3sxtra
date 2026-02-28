#pragma once
/**
 * @file rmlui_gameover.h
 * @brief RmlUi Game Over / Results Screen — replaces CPS3 effect_76+L1
 *        text objects in GameOver_2nd() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_gameover_init(void);
void rmlui_gameover_update(void);
void rmlui_gameover_show(void);
void rmlui_gameover_hide(void);
void rmlui_gameover_shutdown(void);

#ifdef __cplusplus
}
#endif
