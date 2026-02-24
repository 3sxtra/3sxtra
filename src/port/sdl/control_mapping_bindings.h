#pragma once

#include "port/input_definition.h"

#ifdef __cplusplus
extern "C" {
#endif

int ControlMapping_GetPlayerDeviceID(int player_num);
InputID ControlMapping_GetPlayerMapping(int player_num, const char* action);

#ifdef __cplusplus
}
#endif
