#pragma once
/**
 * @file rmlui_extra_option.h
 * @brief RmlUi Extra Option screen — replaces CPS3 Dir_Move_Sub/Setup_Next_Page
 *        effect pipeline with a 4-page HTML/CSS toggle table overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_extra_option_init(void);
void rmlui_extra_option_update(void);
void rmlui_extra_option_show(void);
void rmlui_extra_option_hide(void);
void rmlui_extra_option_shutdown(void);

#ifdef __cplusplus
}
#endif
