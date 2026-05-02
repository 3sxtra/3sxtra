#include "port/I_System.h"
#include <SDL3/SDL.h>

void I_MemSet(void* dst, int val, size_t size) {
    SDL_memset(dst, val, size);
}

void I_MemCopy(void* dst, const void* src, size_t size) {
    SDL_memcpy(dst, src, size);
}

void I_Log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args);
    va_end(args);
}

void I_LogDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, fmt, args);
    va_end(args);
}

void I_Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
    va_end(args);
}
