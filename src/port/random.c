#include "port/random.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#else
#include <sys/random.h>
#endif

void Sys_RandomBytes(void* buf, size_t len) {
    bool success = false;
    uint8_t* bytes = (uint8_t*)buf;

#ifdef _WIN32
    if (BCryptGenRandom(NULL, bytes, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        success = true;
    }
#elif defined(__APPLE__)
    arc4random_buf(bytes, len);
    success = true;
#else
    if (getrandom(bytes, len, 0) == (ssize_t)len) {
        success = true;
    }
#endif

    if (!success) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Random] CSPRNG failed, falling back to PRNG");
        for (size_t i = 0; i < len; i += 4) {
            uint32_t r = SDL_rand_bits();
            size_t remaining = len - i;
            if (remaining > 4)
                remaining = 4;
            memcpy(bytes + i, &r, remaining);
        }
    }
}
