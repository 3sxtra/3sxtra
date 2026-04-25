/**
 * @file text_renderer_vtable.h
 * @brief Function-pointer vtable for text renderer backend dispatch.
 *
 * Replaces the if/else chain in sdl_text_renderer.c with a single
 * const pointer that is set once at init time.  Each backend
 * (GL, GPU, SDL2D) provides a static const instance.
 */
#ifndef TEXT_RENDERER_VTABLE_H
#define TEXT_RENDERER_VTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextRendererVtable {
    /* ---- Lifecycle (2) ---- */
    void (*Init)(const char* base_path, const char* font_path);
    void (*Shutdown)(void);

    /* ---- Drawing (2) ---- */
    void (*DrawText)(const char* text, float x, float y, float scale,
                     float r, float g, float b,
                     float target_width, float target_height);
    void (*Flush)(void);

    /* ---- Configuration (4) ---- */
    void (*SetYOffset)(float y_offset);
    void (*SetBackgroundEnabled)(int enabled);
    void (*SetBackgroundColor)(float r, float g, float b, float a);
    void (*SetBackgroundPadding)(float px);

    /* ---- Debug (1) — GL-only, NULL for other backends ---- */
    void (*DrawDebugChars)(const void* buffer, int count, float scale,
                           float target_width, float target_height);
} TextRendererVtable;

/** Active text-renderer vtable — set once by TextRendererVtable_Init(). */
extern const TextRendererVtable* g_text_renderer;

/** Pick the vtable matching the current RendererBackend. */
void TextRendererVtable_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_RENDERER_VTABLE_H */
