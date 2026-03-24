/**
 * @file test_runner_utils.c
 * @brief Shared IO helpers for the replay test runner.
 */

#include "test/test_runner_utils.h"
#include "main.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

char* ram_path(int index) {
    char* result = NULL;
    SDL_asprintf(&result, "%s/frame_%08d.ram", configuration.test.states_path, index);
    return result;
}

Uint8 read_u8(SDL_IOStream* io, Sint64 offset) {
    Uint8 result = 0;
    SDL_SeekIO(io, offset, SDL_IO_SEEK_SET);
    SDL_ReadU8(io, &result);
    return result;
}

Uint16 read_u16(SDL_IOStream* io, Sint64 offset) {
    Uint16 result = 0;
    SDL_SeekIO(io, offset, SDL_IO_SEEK_SET);
    SDL_ReadU16BE(io, &result);
    return result;
}

Sint16 read_s16(SDL_IOStream* io, Sint64 offset) {
    Sint16 result = 0;
    SDL_SeekIO(io, offset, SDL_IO_SEEK_SET);
    SDL_ReadS16BE(io, &result);
    return result;
}
