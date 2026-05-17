/**
 * @file sdl_text_renderer.c
 * @brief Text renderer backend dispatch via vtable.
 *
 * Each SDLTextRenderer_*() function is now a one-liner that
 * forwards to g_text_renderer->Xxx().  The if/else chain has been
 * replaced by three static const vtable instances (GL, GPU, SDL2D).
 */
#include "port/sdl/renderer/sdl_text_renderer.h"
#include "game_state.h"
#include "port/text_renderer_vtable.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_text_renderer_internal.h"
#include "port/system.h"
#include "types.h"
#include <assert.h>

/* ================================================================
 *  Static const vtable instances — one per backend group
 * ================================================================ */

static const TextRendererVtable s_vtable_gl = {
    .Init = SDLTextRendererGL_Init,
    .Shutdown = SDLTextRendererGL_Shutdown,
    .DrawText = SDLTextRendererGL_DrawText,
    .Flush = SDLTextRendererGL_Flush,
    .SetYOffset = SDLTextRendererGL_SetYOffset,
    .SetBackgroundEnabled = SDLTextRendererGL_SetBackgroundEnabled,
    .SetBackgroundColor = SDLTextRendererGL_SetBackgroundColor,
    .SetBackgroundPadding = SDLTextRendererGL_SetBackgroundPadding,
    .DrawDebugChars = SDLTextRendererGL_DrawDebugChars,
};

static const TextRendererVtable s_vtable_gpu = {
    .Init = SDLTextRendererGPU_Init,
    .Shutdown = SDLTextRendererGPU_Shutdown,
    .DrawText = SDLTextRendererGPU_DrawText,
    .Flush = SDLTextRendererGPU_Flush,
    .SetYOffset = SDLTextRendererGPU_SetYOffset,
    .SetBackgroundEnabled = SDLTextRendererGPU_SetBackgroundEnabled,
    .SetBackgroundColor = SDLTextRendererGPU_SetBackgroundColor,
    .SetBackgroundPadding = SDLTextRendererGPU_SetBackgroundPadding,
    .DrawDebugChars = NULL,
};

/* SDL2D and SDL2D Classic share the same text renderer */
static const TextRendererVtable s_vtable_sdl = {
    .Init = SDLTextRendererSDL_Init,
    .Shutdown = SDLTextRendererSDL_Shutdown,
    .DrawText = SDLTextRendererSDL_DrawText,
    .Flush = SDLTextRendererSDL_Flush,
    .SetYOffset = SDLTextRendererSDL_SetYOffset,
    .SetBackgroundEnabled = SDLTextRendererSDL_SetBackgroundEnabled,
    .SetBackgroundColor = SDLTextRendererSDL_SetBackgroundColor,
    .SetBackgroundPadding = SDLTextRendererSDL_SetBackgroundPadding,
    .DrawDebugChars = NULL,
};

/* ================================================================
 *  Global vtable pointer
 * ================================================================ */

const TextRendererVtable* g_text_renderer = NULL;

void TextRendererVtable_Init(void) {
    RendererBackend r = SDLApp_GetRenderer();
    switch (r) {
    case RENDERER_SDLGPU:
        g_text_renderer = &s_vtable_gpu;
        break;
    case RENDERER_SDL2D: /* FALLTHROUGH */
    case RENDERER_SDL2D_CLASSIC:
        g_text_renderer = &s_vtable_sdl;
        break;
    case RENDERER_OPENGL: /* FALLTHROUGH */
    default:
        g_text_renderer = &s_vtable_gl;
        break;
    }
    assert(g_text_renderer && "TextRendererVtable_Init: vtable not set");
}

/* ================================================================
 *  Public API — thin one-liner dispatch
 * ================================================================ */

void SDLTextRenderer_Init(const char* base_path, const char* font_path) {
    TextRendererVtable_Init();
    g_text_renderer->Init(base_path, font_path);
}

void SDLTextRenderer_Shutdown(void) {
    g_text_renderer->Shutdown();
}

void SDLTextRenderer_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                              float target_width, float target_height) {
    g_text_renderer->DrawText(text, x, y, scale, r, g, b, target_width, target_height);
}

void SDLTextRenderer_Flush(void) {
    g_text_renderer->Flush();
}

void SDLTextRenderer_SetYOffset(float y_offset) {
    g_text_renderer->SetYOffset(y_offset);
}

void SDLTextRenderer_SetBackgroundEnabled(int enabled) {
    g_text_renderer->SetBackgroundEnabled(enabled);
}

void SDLTextRenderer_SetBackgroundColor(float r, float g, float b, float a) {
    g_text_renderer->SetBackgroundColor(r, g, b, a);
}

void SDLTextRenderer_SetBackgroundPadding(float px) {
    g_text_renderer->SetBackgroundPadding(px);
}

void SDLTextRenderer_DrawDebugBuffer(float target_width, float target_height) {
#if DEBUG

    if (flDebugStrCtr == 0) {
        return;
    }

    void* buff_ptr = flPS2GetSystemBuffAdrs(flDebugStrHan);
    if (buff_ptr == NULL) {
        return;
    }

    /* ⚡ GL backend uses batched draw (2 glDrawArrays calls total).
     * Other backends still use the per-character approach. */
    if (g_text_renderer->DrawDebugChars) {
        float scale = target_height / 480.0f;
        g_text_renderer->DrawDebugChars(buff_ptr, (int)flDebugStrCtr, scale, target_width, target_height);
        flDebugStrCtr = 0;
        return;
    }

    /* Fallback for GPU/SDL2D: per-character rendering */
    typedef struct {
        u16 x;
        u16 y;
        u32 code;
        u32 col;
    } RenderBuffer;

    RenderBuffer* rb = (RenderBuffer*)buff_ptr;
    SDLTextRenderer_SetBackgroundEnabled(0);
    float scale = target_height / 480.0f;

    for (u32 i = 0; i < flDebugStrCtr; i++) {
        RenderBuffer* ch = &rb[i];
        if (ch->code < 0x20 || ch->code > 0x7F)
            continue;

        u8 cr = (ch->col >> 16) & 0xFF;
        u8 cg = (ch->col >> 8) & 0xFF;
        u8 cb = ch->col & 0xFF;
        cr = (cr < 128) ? cr * 2 : 255;
        cg = (cg < 128) ? cg * 2 : 255;
        cb = (cb < 128) ? cb * 2 : 255;

        float px = (float)ch->x * scale;
        float py = (float)ch->y * scale;
        char text[2] = { (char)ch->code, '\0' };

        SDLTextRenderer_DrawText(text, px + 1, py + 1, scale, 0.0f, 0.0f, 0.0f, target_width, target_height);
        SDLTextRenderer_DrawText(
            text, px, py, scale, cr / 255.0f, cg / 255.0f, cb / 255.0f, target_width, target_height);
    }

    SDLTextRenderer_SetBackgroundEnabled(1);
    flDebugStrCtr = 0;
#else
    (void)target_width;
    (void)target_height;
#endif
}
