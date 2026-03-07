#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void control_mapping_init();
void control_mapping_update();
void control_mapping_render(int window_width, int window_height);
void control_mapping_shutdown();
bool control_mapping_is_active();

#ifdef __cplusplus
}
#endif
