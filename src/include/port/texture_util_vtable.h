/**
 * @file texture_util_vtable.h
 * @brief Vtable for TextureUtil backend dispatch.
 *
 * Each backend implements Load, Free, GetSize, and Shutdown.
 * The vtable is selected once at init and all TextureUtil_*()
 * functions become one-liner forwards.
 */
#ifndef TEXTURE_UTIL_VTABLE_H
#define TEXTURE_UTIL_VTABLE_H

#include <stdbool.h>

struct SDL_Surface;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextureUtilVtable {
    void* (*LoadFromSurface)(struct SDL_Surface* surface);
    void (*Free)(void* texture_id);
    void (*GetSize)(void* texture_id, int* w, int* h);
    void (*Shutdown)(void);
} TextureUtilVtable;

/** Global vtable pointer — valid after TextureUtilVtable_Init() */
extern const TextureUtilVtable* g_texture_util;

/** Select the correct vtable based on the active renderer backend. */
void TextureUtilVtable_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURE_UTIL_VTABLE_H */
