#ifndef PS3_ENDIAN_MACROS_H
#define PS3_ENDIAN_MACROS_H

/**
 * @file endian_macros.h
 * @brief Standardized endian conversion macros for PS3 port
 * 
 * This header provides consistent byte-swapping macros across the entire PS3 port codebase.
 * All files should use these macros instead of platform-specific implementations.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platform detection for PS3/PowerPC
 * 
 * These defines help identify the target platform for conditional compilation.
 */
#if defined(__PPU__) || defined(__ppc__) || defined(__PS3__) || \
    defined(_BIG_ENDIAN) || defined(__BIG_ENDIAN__) || \
    (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define PLATFORM_PS3 1
    #define PLATFORM_BIG_ENDIAN 1
    
    // Natively Big-Endian: No byte swapping required for Big-Endian data
    #define REVERT_U16(x) ((uint16_t)(x))
    #define REVERT_U32(x) ((uint32_t)(x))
#else
    #define PLATFORM_LITTLE_ENDIAN 1

    /**
     * @brief Convert 16-bit value from little-endian to host byte order
     * @param x 16-bit value to convert
     * @return Converted 16-bit value
     */
    #define REVERT_U16(x) (uint16_t)((((uint16_t)(x) & 0xFF) << 8) | (((uint16_t)(x) & 0xFF00) >> 8))

    /**
     * @brief Convert 32-bit value from little-endian to host byte order
     * @param x 32-bit value to convert
     * @return Converted 32-bit value
     */
    #define REVERT_U32(x) ((((uint32_t)(x) & 0xFF) << 24) | (((uint32_t)(x) & 0xFF00) << 8) | \
                           (((uint32_t)(x) & 0xFF0000) >> 8) | (((uint32_t)(x) & 0xFF000000) >> 24))
#endif

#ifdef __cplusplus
}
#endif

#endif /* PS3_ENDIAN_MACROS_H */