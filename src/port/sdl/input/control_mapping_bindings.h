/**
 * @file control_mapping_bindings.h
 * @brief Per-player device and action mapping queries.
 */
#pragma once

#include "port/input_definition.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PLATFORM_PS3

int ControlMapping_GetPlayerDeviceID(int player_num);
InputID ControlMapping_GetPlayerMapping(int player_num, const char* action);
const char* ControlMapping_GetPlayerMappingIconURI(int player_num, int index);

#else

static inline int ControlMapping_GetPlayerDeviceID(int player_num) {
    (void)player_num;
    return -1; // -1 forces ioconv to fallback to native hardware logic
}

static inline InputID ControlMapping_GetPlayerMapping(int player_num, const char* action) {
    (void)player_num;
    (void)action;
    return INPUT_ID_UNKNOWN;
}

static inline const char* ControlMapping_GetPlayerMappingIconURI(int player_num, int index) {
    (void)player_num;
    (void)index;
    return "";
}

#endif

#ifdef __cplusplus
}
#endif
