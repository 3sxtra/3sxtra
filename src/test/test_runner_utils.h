/**
 * @file test_runner_utils.h
 * @brief Shared IO helpers for the replay test runner.
 *
 * Provides SDL3-idiomatic read functions for CPS3 RAM dump files
 * (big-endian byte order) and a helper to build frame file paths.
 */

#ifndef TEST_RUNNER_UTILS_H
#define TEST_RUNNER_UTILS_H

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

/** Build a frame RAM-dump path: "<states_path>/frame_XXXXXXXX.ram" (caller must SDL_free). */
char* ram_path(int index);

Uint8 read_u8(SDL_IOStream* io, Sint64 offset);
Uint16 read_u16(SDL_IOStream* io, Sint64 offset);
Sint16 read_s16(SDL_IOStream* io, Sint64 offset);

#endif /* TEST_RUNNER_UTILS_H */
