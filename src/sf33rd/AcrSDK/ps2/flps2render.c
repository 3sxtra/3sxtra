/**
 * @file flps2render.c
 * @brief Render state management and texture register setup implementation.
 *
 * Dispatches render state changes (texture stages, clear color),
 * builds GS texture register packets, and provides Z-buffer depth
 * conversion for the SDL rendering path.
 *
 * Part of the AcrSDK ps2 module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "common.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include "port/sdl/sdl_game_renderer.h"

static void flPS2SetClearColor(u32 col);
static s32 flPS2SendTextureRegister(u32 th);

/** @brief Dispatch a render state change by function type and value. */
s32 flSetRenderState(enum _FLSETRENDERSTATE func, u32 value) {
    u32 th;

    switch (func) {
    case FLRENDER_TEXSTAGE0:
    case FLRENDER_TEXSTAGE1:
    case FLRENDER_TEXSTAGE2:
    case FLRENDER_TEXSTAGE3:
        th = value;

        if (func == FLRENDER_TEXSTAGE0) {
            flPS2SendTextureRegister(th);
        }

        break;

    case FLRENDER_BACKCOLOR:
        flPS2SetClearColor(value);
        break;

    default:
        break;
    }

    return 1;
}

/** @brief Set the frame clear (background) color. */
static void flPS2SetClearColor(u32 col) {
    flPs2State.FrameClearColor = col;
}

/** @brief Build and send the GS texture register packet for a texture handle. */
static s32 flPS2SendTextureRegister(u32 th) {
    static u64 psTexture_data[16] = {
        0x0000000070000007, 0x0000000000000000, 0x1000000000008006, 0x000000000000000E,
        0x0000000000000000, 0x000000000000003B, 0x0000000000000000, 0x0000000000000014,
        0x0000000000000000, 0x0000000000000006, 0x0000000000000000, 0x0000000000000008,
        0x0000000000000000, 0x0000000000000034, 0x0000000000000000, 0x0000000000000036,
    };

    if (!flPS2SetTextureRegister(th,
                                 &psTexture_data[4],
                                 &psTexture_data[6],
                                 &psTexture_data[8],
                                 &psTexture_data[10],
                                 &psTexture_data[12],
                                 &psTexture_data[14],
                                 flSystemRenderOperation)) {
        return 0;
    }

    return 1;
}

/** @brief Set the GS texture registers and activate a texture on the SDL renderer. */
s32 flPS2SetTextureRegister(u32 th, u64* texA, u64* tex1, u64* tex0, u64* clamp, u64* miptbp1, u64* miptbp2,
                            u32 render_ope) {
    (void)texA;
    (void)tex1;
    (void)tex0;
    (void)clamp;
    (void)miptbp1;
    (void)miptbp2;
    (void)render_ope;

    // Each backend (GPU/GL/SDL2D) validates handle bounds and null surfaces.
    SDLGameRenderer_SetTexture(th);

    return 1;
}

/** @brief Convert a normalised Z depth to the frame buffer Z range. */
f32 flPS2ConvScreenFZ(f32 z) {
    // ⚡ Bolt: Pre-compute multiply-add constants — ZBuffMax is set once to 65535.0f
    // at init and never changes. Original: (z-1) * -0.5 * ZBuffMax = z*factor + offset.
    // Reduces 3 FP ops + 1 struct load per call → 1 multiply + 1 add with static consts.
    // Called 100-300× per frame from draw_quad.
    static f32 s_factor = 0.0f;
    static f32 s_offset = 0.0f;
    static int s_init = 0;
    if (!s_init) {
        s_factor = -0.5f * flPs2State.ZBuffMax;
        s_offset = 0.5f * flPs2State.ZBuffMax;
        s_init = 1;
    }
    return z * s_factor + s_offset;
}
