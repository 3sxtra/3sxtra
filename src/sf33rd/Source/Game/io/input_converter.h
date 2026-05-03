/**
 * @file input_converter.h
 * @brief Public API for input conversion and controller processing.
 *
 * Part of the io module.
 */

#ifndef INPUT_CONVERTER_H
#define INPUT_CONVERTER_H

#include "sf33rd/AcrSDK/common/pad.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u8 state;
    u8 anstate;
    u16 kind;
    u32 sw;
    u32 input_old;
    u32 input_pressed;
    u32 input_released;
    u32 input_changed;
    u32 sw_repeat;
    PAD_STICK stick[2];
} IOPad;

typedef struct {
    IOPad data[2];
    u16 sw[2];
} IO;

extern IO io_w;

extern const char* game_actions[];
int get_game_actions_count();
u32 get_action_flag(const char* action);

void keyConvert();

#ifdef __cplusplus
}
#endif

#endif
