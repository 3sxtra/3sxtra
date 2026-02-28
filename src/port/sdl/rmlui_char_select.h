#pragma once
/**
 * @file rmlui_char_select.h
 * @brief RmlUi Character Select overlay — text elements (timer, char names,
 *        SA labels) overlaid on CPS3 sprite portraits.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_char_select_init(void);
void rmlui_char_select_update(void);
void rmlui_char_select_show(void);
void rmlui_char_select_hide(void);
void rmlui_char_select_shutdown(void);

#ifdef __cplusplus
}
#endif
