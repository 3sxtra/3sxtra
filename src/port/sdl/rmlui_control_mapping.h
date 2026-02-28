/**
 * @file rmlui_control_mapping.h
 * @brief RmlUi control mapping overlay — extern C API.
 *
 * RmlUi version of the controller setup screen (F1).
 * Shows device assignment, mapping state, and bound actions.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_control_mapping_init(void);
void rmlui_control_mapping_update(void);
void rmlui_control_mapping_shutdown(void);

#ifdef __cplusplus
}
#endif
