#pragma once
/**
 * @file rmlui_training_menus.h
 * @brief RmlUi Training Sub-Menus — replaces CPS3 effect_61/A3 items
 *        in Training_Mode(), Normal_Training(), Dummy_Setting(),
 *        Training_Option(), Blocking_Training(), and Blocking_Tr_Option()
 *        with HTML/CSS overlays.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_training_menus_init(void);
void rmlui_training_menus_update(void);

/* Training Mode selector (4 items: Normal/Parrying/Trials/Exit) */
void rmlui_training_mode_show(void);
void rmlui_training_mode_hide(void);

/* Normal Training pause menu (8 items) */
void rmlui_normal_training_show(void);
void rmlui_normal_training_hide(void);

/* Dummy Setting sub-menu */
void rmlui_dummy_setting_show(void);
void rmlui_dummy_setting_hide(void);

/* Training Option sub-menu */
void rmlui_training_option_show(void);
void rmlui_training_option_hide(void);

/* Blocking Training pause menu (6 items) */
void rmlui_blocking_training_show(void);
void rmlui_blocking_training_hide(void);

/* Blocking Training Option sub-menu */
void rmlui_blocking_tr_option_show(void);
void rmlui_blocking_tr_option_hide(void);

void rmlui_training_menus_shutdown(void);

#ifdef __cplusplus
}
#endif
