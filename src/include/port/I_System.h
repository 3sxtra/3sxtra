#ifndef I_SYSTEM_H
#define I_SYSTEM_H

#include "types.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Zeroes out a block of memory.
 */
void I_MemSet(void* dst, int val, size_t size);

/**
 * @brief Copies a block of memory.
 */
void I_MemCopy(void* dst, const void* src, size_t size);

/**
 * @brief Helper macro to zero out a struct or array.
 */
#define I_ZeroStruct(x) I_MemSet(&(x), 0, sizeof(x))
#define I_ZeroArray(x) I_MemSet((x), 0, sizeof(x))
#define I_ZeroPointer(x) I_MemSet((x), 0, sizeof(*(x)))

/**
 * @brief Logs an informational message.
 */
void I_Log(const char* fmt, ...);

/**
 * @brief Logs a debug message.
 */
void I_LogDebug(const char* fmt, ...);

/**
 * @brief Logs an error message.
 */
void I_Error(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // I_SYSTEM_H
