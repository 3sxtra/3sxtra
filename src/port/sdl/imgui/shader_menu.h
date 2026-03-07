#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void shader_menu_init();
void shader_menu_render(int window_width, int window_height);
void shader_menu_shutdown();

#ifdef __cplusplus
}
#endif
