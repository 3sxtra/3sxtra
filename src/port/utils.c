/**
 * @file utils.c
 * @brief Core utility functions: fatal error handling, stack traces, and debug printing.
 *
 * Provides `fatal_error()` with platform-specific stack trace output
 * (dbghelp on Windows, backtrace on Unix), a `not_implemented()` stub
 * reporter, and a conditional `debug_print()` for DEBUG builds.
 */
#include "common.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#define SYMBOL_NAME_MAX 256
#else
#include <execinfo.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define BACKTRACE_MAX 100

/** @brief Print a fatal error message with stack trace and abort. */
void fatal_error(const s8* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Fatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);

    fflush(stdout);
    fflush(stderr);

    void* buffer[BACKTRACE_MAX];
#if !defined(_WIN32)
    int nptrs = backtrace(buffer, BACKTRACE_MAX);
    fprintf(stderr, "Stack trace:\n");
    backtrace_symbols_fd(buffer, nptrs, fileno(stderr));
#else
    fprintf(stderr, "Stack trace:\n");
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    int nptrs = CaptureStackBackTrace(0, BACKTRACE_MAX, buffer, NULL);
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + SYMBOL_NAME_MAX);
    if (!symbol) {
        fprintf(stderr, "Calloc failed when allocating SYMBOL_INFO, bailing!\n\n");
        fflush(stderr);
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
#endif
    abort();
}

/** @brief Report that a function is not implemented and abort. */
void not_implemented(const s8* func) {
    fatal_error("Function not implemented: %s\n", func);
}

/** @brief Print a debug message to stdout (DEBUG builds only). */
void debug_print(const char* fmt, ...) {
#if defined(DEBUG)
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
#endif
}
