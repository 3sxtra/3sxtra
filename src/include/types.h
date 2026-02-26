/**
 * @file types.h
 * @brief Fixed-width integer type aliases (s8/u8, s16/u16, s32/u32, etc.)
 *        and platform abstraction for PS2 vs. SDL3 targets.
 */
#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t strlen_t;

// SCE types — guarded because:
// - winsock2.h _bsd_types.h defines these differently on Windows
// - macOS <sys/types.h> already provides these as BSD types
#if !defined(_BSDTYPES_DEFINED) && !defined(__APPLE__)
typedef uint8_t u_char;
typedef uint16_t u_short;
typedef uint32_t u_int;
typedef uint64_t u_long;
#endif

typedef enum ModeType {
    MODE_ARCADE,
    MODE_VERSUS,
    MODE_NETWORK,
    MODE_NORMAL_TRAINING,
    MODE_PARRY_TRAINING,
    MODE_REPLAY,
    MODE_TRIALS,
} ModeType;

#endif
