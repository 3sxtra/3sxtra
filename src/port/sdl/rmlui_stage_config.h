/**
 * @file rmlui_stage_config.h
 * @brief RmlUi HD stage configuration menu — extern C API.
 *
 * Provides the same per-layer stage editing as stage_config_menu.h
 * but rendered via RmlUi data bindings instead of ImGui.
 * Toggled with F6 when --ui rmlui is active.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_stage_config_init(void);
void rmlui_stage_config_update(void);
void rmlui_stage_config_shutdown(void);

#ifdef __cplusplus
}
#endif
