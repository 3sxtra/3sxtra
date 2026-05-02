/**
 * @file renderer_caps.h
 * @brief Runtime GPU/GL capability detection.
 *
 * Replaces compile-time #ifdef __ANDROID__ checks with runtime queries.
 * Populated once at renderer init by RendererCaps_Detect().
 */
#ifndef RENDERER_CAPS_H
#define RENDERER_CAPS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RendererCaps {
    /* ---- API type ---- */
    bool is_gles; /* Running on OpenGL ES (vs desktop GL) */

    /* ---- Feature flags ---- */
    bool has_get_tex_level_param; /* glGetTexLevelParameteriv available (GL 1.0+, not GLES) */
    bool has_persistent_mapping;  /* GL_ARB_buffer_storage / GL 4.4+ */
    bool has_texture_arrays;      /* GL_TEXTURE_2D_ARRAY (GL 3.0+ / GLES 3.0+) */
    bool has_compute_shaders;     /* GL 4.3+ / GLES 3.1+ */
    bool has_pbo;                 /* Pixel Buffer Objects (GL 2.1+ / GLES 3.0+) */

    /* ---- Limits ---- */
    uint32_t max_texture_size; /* GL_MAX_TEXTURE_SIZE */
    uint32_t max_array_layers; /* GL_MAX_ARRAY_TEXTURE_LAYERS */
} RendererCaps;

/** Global caps — valid after RendererCaps_Detect() is called. */
extern RendererCaps g_renderer_caps;

/**
 * @brief Detect GPU capabilities for the current backend.
 *
 * For GL/GLES: queries extensions and limits via glGetIntegerv/glGetString.
 * For GPU/SDL2D backends: sets safe defaults (most flags false).
 * Must be called after GL context or GPU device creation.
 */
void RendererCaps_Detect(void);

#ifdef __cplusplus
}
#endif

#endif /* RENDERER_CAPS_H */
