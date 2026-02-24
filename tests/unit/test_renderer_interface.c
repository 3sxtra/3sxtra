#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "cmocka.h"

#include "port/renderer.h"
#include "port/sdl/sdl_game_renderer.h"
#include "structs.h" // For WORK definition
#include "sf33rd/Source/Game/rendering/aboutspr.h" // For shadow_drawing signature
#include "sf33rd/Source/Common/PPGFile.h" // For ppgRenewDotDataSeqs signature

// --- Mocks ---

void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    check_expected(color);
    assert_non_null(sprite);
    // Optional: verify sprite content if needed
}

void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    check_expected(color);
}

void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color) {
    check_expected(color);
}

void SDLGameRenderer_SetTexture(unsigned int texture_handle) {
    // Mock implementation
}

// Mocking shadow_drawing from aboutspr.c
void shadow_drawing(WORK* wk, s16 bsy) {
    check_expected(wk);
    check_expected(bsy);
}

// Mocking ppgRenewDotDataSeqs from PPGFile.c
void ppgRenewDotDataSeqs(Texture* tch, u32 gix, u32* srcRam, u32 code, u32 size) {
    check_expected(gix);
    check_expected(code); // We mapped x -> ofs/code in renderer.c? No, x -> code, y -> size
    check_expected(size);
}

// Mocking ppgGetCurrentPaletteHandle from PPGFile.c
u16 ppgGetCurrentPaletteHandle(void) {
    return 0; // Return dummy handle 0
}

// Mocking flLogOut from foundaps2.c
s32 flLogOut(s8* format, ...) {
    // We can check expected format string if we want, or just ignore
    // For now, we don't expect it to be called in success paths
    // If it is called (e.g. overflow), we can mock it.
    // check_expected(format); 
    return 0;
}

// --- Tests ---

static void test_draw_textured_quad(void **state) {
    (void) state;
    
    RendererVertex v[4];
    // Setup a quad
    v[0].x = 10.0f; v[0].y = 10.0f; v[0].z = 0.0f; v[0].color = 0xFF0000FF;
    v[1].x = 20.0f; v[1].y = 10.0f; v[1].z = 0.0f; v[1].color = 0xFF0000FF;
    v[2].x = 20.0f; v[2].y = 20.0f; v[2].z = 0.0f; v[2].color = 0xFF0000FF;
    v[3].x = 10.0f; v[3].y = 20.0f; v[3].z = 0.0f; v[3].color = 0xFF0000FF;

    expect_value(SDLGameRenderer_DrawTexturedQuad, color, 0xFF0000FF);
    
    Renderer_DrawTexturedQuad(v, 4);
}

static void test_queue_and_flush_2d_primitives(void **state) {
    (void) state;

    Renderer_Init(); // Ensure queue is reset

    // 1. Queue a normal solid quad (Type 0)
    // pos: {x0,y0, x1,y1, x2,y2, x3,y3}
    f32 pos1[] = { 10,10, 20,10, 20,20, 10,20 };
    f32 prio1 = 1.0f;
    u32 color1 = 0xFF0000FF;
    Renderer_Queue2DPrimitive(pos1, prio1, (uintptr_t)color1, 0);

    // 2. Queue a shadow (Type 1)
    // pos: {y-offset} (stored in v[0].y)
    f32 pos2[] = { 5.0f }; 
    f32 prio2 = 2.0f;
    WORK mockWork; // Address will be used
    Renderer_Queue2DPrimitive(pos2, prio2, (uintptr_t)&mockWork, 1);

    // Expect calls in Flush
    // Order depends on priority. 
    // prio1 (1.0) < prio2 (2.0)
    // The sorting logic in renderer.c (from dc_ghost.c) is:
    // while (priority > s_Render2DQueue.prim[i].v[0].z) ...
    // So it inserts based on Z value.
    // If prio1=1.0 and prio2=2.0, let's see where they end up.
    // The list is singly linked.
    // It seems to sort ascending? Or descending?
    // "if (priority > ...)" implies if new priority is greater, keep searching?
    // So smaller priorities come first?
    // Let's assume order for now and verify.
    
    // Expect Type 0 DrawSolidQuad
    expect_value(SDLGameRenderer_DrawSolidQuad, color, color1);

    // Expect Type 1 shadow_drawing
    expect_value(shadow_drawing, wk, &mockWork);
    expect_value(shadow_drawing, bsy, 5);

    Renderer_Flush2DPrimitives();
}

static void test_update_texture(void **state) {
    (void) state;
    
    int textureId = 123;
    u32 dummyData = 0;
    int x = 10;
    int y = 20;
    
    // renderer.c: ppgRenewDotDataSeqs(0, textureId, (u32*)data, x, y);
    expect_value(ppgRenewDotDataSeqs, gix, textureId);
    expect_value(ppgRenewDotDataSeqs, code, x); 
    expect_value(ppgRenewDotDataSeqs, size, y);

    Renderer_UpdateTexture(textureId, &dummyData, x, y, 0, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_draw_textured_quad),
        cmocka_unit_test(test_queue_and_flush_2d_primitives),
        cmocka_unit_test(test_update_texture),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}