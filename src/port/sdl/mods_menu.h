/**
 * @file mods_menu.h
 * @brief F3 mods overlay menu — centralizes all modding toggles.
 */
#ifndef MODS_MENU_H
#define MODS_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

void mods_menu_init(void);
void mods_menu_render(int window_width, int window_height);
void mods_menu_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* MODS_MENU_H */
