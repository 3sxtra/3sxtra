#ifndef PS3_TEX_HANDLE_H
#define PS3_TEX_HANDLE_H

/**
 * @file ps3_tex_handle.h
 * @brief Formalized texture/palette handle packing convention.
 *
 * P10c Audit Fix: Documents the packed texture handle encoding used
 * throughout the GCM renderer. Previously existed only as scattered
 * comments using LO_16_BITS/HI_16_BITS macros.
 *
 * Packed handle layout (uint32_t):
 *   Low  16 bits = texture index + 1 (0 = invalid / no texture)
 *   High 16 bits = palette index + 1 (0 = no palette)
 *
 * Example: texture 5 with palette 3 = 0x0004_0006
 */

#include <stdint.h>

typedef uint32_t PackedTexHandle;

/* Extract the low/high 16-bit fields */
#define TEX_HANDLE_LO(th) ((th) & 0xFFFF)
#define TEX_HANDLE_HI(th) (((th) >> 16) & 0xFFFF)

/* Pack two 1-based indices into a handle */
#define TEX_HANDLE_PACK(tex_1based, pal_1based) ((uint32_t)(pal_1based) << 16 | (uint32_t)(tex_1based))

/* Convert 1-based handle fields to 0-based array indices */
#define TEX_IDX(th) (TEX_HANDLE_LO(th) - 1)
#define PAL_IDX(th) (TEX_HANDLE_HI(th) - 1)

/* Check validity */
#define TEX_HANDLE_HAS_TEX(th) (TEX_HANDLE_LO(th) != 0)
#define TEX_HANDLE_HAS_PAL(th) (TEX_HANDLE_HI(th) != 0)

#endif /* PS3_TEX_HANDLE_H */
