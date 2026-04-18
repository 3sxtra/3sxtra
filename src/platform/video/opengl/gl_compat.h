/**
 * @file gl_compat.h
 * @brief GL/GLES compatibility shim for the official OpenGL renderer.
 *
 * On desktop: includes SDL_opengl.h with GL_GLEXT_PROTOTYPES.
 * On Android: includes GLES 3.0 headers and provides compatibility
 * macros for GL_TEXTURE_1D (which doesn't exist in GLES).
 */
#ifndef GL_COMPAT_H
#define GL_COMPAT_H

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

/* GLES 3.0 has no 1D textures. The official renderer uses GL_TEXTURE_1D
 * for palette lookup textures (up to 256 entries). We emulate this by
 * using a GL_TEXTURE_2D with height=1. The shaders also need adaptation
 * (sampler1D → sampler2D, texelFetch gets ivec2 instead of int). */
#define GL_TEXTURE_1D GL_TEXTURE_2D

/* Map glTexImage1D to glTexImage2D with height=1 */
#define glTexImage1D(target, level, internalformat, width, border, format, type, data) \
    glTexImage2D(GL_TEXTURE_2D, (level), (internalformat), (width), 1, (border), (format), (type), (data))

/* GLES has no glClearDepth (double), only glClearDepthf (float) */
#ifndef glClearDepth
#define glClearDepth glClearDepthf
#endif

/* GLES 3.0 does not support GL_BGRA as a format parameter.
 * Map to GL_RGBA — callers must handle byte-order conversion if needed. */
#ifndef GL_BGRA
#define GL_BGRA GL_RGBA
#endif

/* GLES 3.0 does not support GL_UNSIGNED_SHORT_1_5_5_5_REV.
 * Map to GL_UNSIGNED_SHORT_5_5_5_1 — the bit layout differs, but for
 * palette lookup textures this is acceptable (colors are looked up by index). */
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV GL_UNSIGNED_SHORT_5_5_5_1
#endif

#else
/* Desktop OpenGL */
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>
#endif

#endif /* GL_COMPAT_H */

