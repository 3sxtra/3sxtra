#ifndef TEST_RUNNER_COMPARE_H
#define TEST_RUNNER_COMPARE_H

#include <SDL3/SDL_iostream.h>

#if DEBUG

void compare_values(SDL_IOStream* io, Uint64 frame);
void sync_values(SDL_IOStream* io);

#else

static inline void compare_values(SDL_IOStream* io, Uint64 frame) {
    (void)io;
    (void)frame;
}
static inline void sync_values(SDL_IOStream* io) {
    (void)io;
}

#endif

#endif
