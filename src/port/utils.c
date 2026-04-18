#include "port/utils.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dbghelp.h>
#define SYMBOL_NAME_MAX 256
#elif defined(__ANDROID__)
#include <signal.h>
#elif __APPLE__ || linux
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __ANDROID__
#include <SDL3/SDL.h>
#endif

#define BACKTRACE_MAX 100

void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Fatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);

#ifdef __ANDROID__
    /* Route fatal error to SDL_LogError so it's visible in Android logcat */
    {
        va_list args2;
        va_start(args2, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args2);
        va_end(args2);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FATAL: %s", buf);
    }
#endif

    fflush(stdout);
    fflush(stderr);

#if (__APPLE__ || linux) && !defined(__ANDROID__)
    void* buffer[BACKTRACE_MAX];

    int nptrs = backtrace(buffer, BACKTRACE_MAX);
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(buffer, nptrs, fileno(stderr));
#elif _WIN32
    void* buffer[BACKTRACE_MAX];

    fprintf(stderr, "Stack trace:\n");
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    int nptrs = CaptureStackBackTrace(0, BACKTRACE_MAX, buffer, NULL);
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + SYMBOL_NAME_MAX);

    if (!symbol) {
        fprintf(stderr, "Calloc failed when allocating SYMBOL_INFO, bailing!\n\n");
        SymCleanup(process);
        abort();
    }

    symbol->MaxNameLen = SYMBOL_NAME_MAX;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (int i = 0; i < nptrs; i++) {
        SymFromAddr(process, (DWORD64)buffer[i], 0, symbol);
        fprintf(stderr, "%i: %s - 0x%0llX\n", nptrs - i - 1, symbol->Name, symbol->Address);
    }

    free(symbol);
    SymCleanup(process);
    fflush(stderr);
#elif defined(__ANDROID__)
    fprintf(stderr, "Stack trace not supported on Android.\n");
#endif

    abort();
}

void not_implemented(const char* func) {
    fatal_error("Function not implemented: %s\n", func);
}

void debug_print(const char* fmt, ...) {
#if DEBUG
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
#endif
}

void stop_if(bool condition) {
#if DEBUG
    if (condition) {
#if _WIN32
        __debugbreak();
#elif !defined(__ANDROID__)
        raise(SIGSTOP);
#endif
    }
#endif
}
