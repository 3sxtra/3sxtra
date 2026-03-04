#pragma once
/**
 * @file rmlui_gameover.h
 * @brief RmlUi Game Over / Results Screen — replaces CPS3 effect_76+L1
 *        text objects in GameOver_1st()/GameOver_2nd() with an HTML/CSS overlay.
 *
 * Phase 1 (banner):  Red "GAME OVER" bar during transition.
 * Phase 2 (results): Character, score, and rounds display.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_gameover_init(void);
void rmlui_gameover_update(void);
void rmlui_gameover_show_banner(void);  /* Phase 1: red banner transition */
void rmlui_gameover_show_results(void); /* Phase 2: recap/results screen */
void rmlui_gameover_show(void);         /* Alias for show_results */
void rmlui_gameover_hide(void);
void rmlui_gameover_shutdown(void);

#ifdef __cplusplus
}
#endif
