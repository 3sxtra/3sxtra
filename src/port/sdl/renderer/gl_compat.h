/**
 * @file gl_compat.h
 * @brief GL/GLES compatibility shim.
 *
 * Includes the correct GLAD loader header for the target platform and
 * provides macros to paper over minor API differences between desktop
 * OpenGL and OpenGL ES 3.0.
 */
#ifndef GL_COMPAT_H
#define GL_COMPAT_H

#ifdef __ANDROID__
    #include <glad/gles2.h>

    #define GL_COMPAT_ES 1

    /* GLES only has glClearDepthf; desktop GL has both glClearDepth (double)
     * and glClearDepthf (float, GL 4.1+). Map to the float variant. */
    #define glClearDepth glClearDepthf
#else
    #include <glad/gl.h>

    #define GL_COMPAT_ES 0
#endif

/* Provide stub definitions for desktop-only features conditionally used in GLES
 * paths (e.g., persistent mapping, where execution is guarded by a boolean) */
#if GL_COMPAT_ES
    #ifndef GL_MAP_PERSISTENT_BIT
        #define GL_MAP_PERSISTENT_BIT 0x0040
    #endif
    #ifndef GL_MAP_COHERENT_BIT
        #define GL_MAP_COHERENT_BIT   0x0080
    #endif
    #define glBufferStorage(target, size, data, flags) do { } while(0)

    #ifndef GL_CLAMP_TO_BORDER
        #define GL_CLAMP_TO_BORDER 0x812D
    #endif
    #ifndef GL_TEXTURE_BORDER_COLOR
        #define GL_TEXTURE_BORDER_COLOR 0x1004
    #endif
    #ifndef GL_FRAMEBUFFER_SRGB
        #define GL_FRAMEBUFFER_SRGB 0x8DB9
    #endif
    
    #ifndef GL_TEXTURE_WIDTH
        #define GL_TEXTURE_WIDTH 0x1000
    #endif
    #ifndef GL_TEXTURE_HEIGHT
        #define GL_TEXTURE_HEIGHT 0x1001
    #endif
    
    /* Dummy definition for queries not natively available in GLES 3.0 */
    #define glGetTexLevelParameteriv(target, level, pname, params) do { *(params) = 0; } while(0)
    
    #ifndef GL_TEXTURE_BUFFER
        #define GL_TEXTURE_BUFFER GL_TEXTURE_BUFFER_EXT
    #endif
    
    #ifndef glTexBuffer
        #define glTexBuffer glTexBufferEXT
    #endif
#endif

#endif /* GL_COMPAT_H */
