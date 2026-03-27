/**
 * @file control_mapping_bindings.h
 * @brief Per-player device and action mapping queries.
 */
#pragma once

#include "port/input_definition.h"

#ifdef __cplusplus
extern "C" {
#endif

int ControlMapping_GetPlayerDeviceID(int player_num);
InputID ControlMapping_GetPlayerMapping(int player_num, const char* action);
const char* ControlMapping_GetPlayerMappingIconURI(int player_num, int index);

#ifdef __cplusplus
}
#endif
