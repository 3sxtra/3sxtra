#include "port/renderer.h"
/**
 * @file renderer.c
 * @brief Legacy Ninja SDK renderer shim — 2D primitive queue.
 *
 * Translates original PS2 rendering calls (SetTexture, DrawTexturedQuad,
 * DrawSprite, etc.) into the modern SDLGameRenderer API. Maintains a
 * 2D primitive queue that batches draw calls within a single frame.
 */
#include "common.h"
#include "port/sdl/sdl_game_renderer.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "structs.h"
#include <stddef.h>
#include <string.h>

// Static state to track current texture/blend mode if needed
static int s_CurrentTextureId = 0;

// Internal 2D primitive queuing (migrated from dc_ghost.c)
typedef struct {
    Vec3 v[4];
    union {
        u32 color;
        WORK* work;
    } attr;
    u32 type;
    s32 next;
} Render2DPrim;

typedef struct {
    s16 ix1st;
    s16 total;
    Render2DPrim prim[100];
} Render2DQueue;

static Render2DQueue s_Render2DQueue;

static void Renderer_2DQueueInit(void) {
    s_Render2DQueue.ix1st = -1;
    s_Render2DQueue.total = 0;
}

void Renderer_Init(void) {
    Renderer_2DQueueInit();
}

void Renderer_BeginFrame(void) {
    // Handled by SDLGameRenderer
}

void Renderer_EndFrame(void) {
    // Handled by SDLGameRenderer
}

void Renderer_SetBlendMode(RendererBlendMode mode) {
    // TODO: Expose blend mode control in SDLGameRenderer
}

// Track current texture table for PPG texture index resolution

static Texture* s_CurrentTexture = NULL;

void Renderer_SetCurrentTexture(Texture* tex) {
    s_CurrentTexture = tex;
}

void Renderer_SetTexture(int textureId) {
    s_CurrentTextureId = textureId;

    u32 texCode;

    if (textureId >= 0x10000) {
        // Already a combined handle (e.g., from bg.c: tex | (pal << 16))
        texCode = (u32)textureId;
    } else if (textureId < 0) {
        // Negative index means use stored handles directly
        u16 palHandle = ppgGetCurrentPaletteHandle();
        texCode = (u32)(-textureId) | ((u32)palHandle << 16);
    } else if (s_CurrentTexture != NULL) {
        // PPG texture index - look up actual handle from current texture table
        s32 ix = textureId - s_CurrentTexture->ixNum1st;
        if (ix >= 0 && ix < s_CurrentTexture->total && s_CurrentTexture->handle != NULL) {
            u16 texHandle = s_CurrentTexture->handle[ix].b16[0];
            if (texHandle == 0) {
                // Texture not loaded
                return;
            }
            // Check if texture needs a palette (bit 14 set in flags)
            u16 palHandle = 0;
            if (s_CurrentTexture->handle[ix].b16[1] & 0x4000) {
                palHandle = ppgGetCurrentPaletteHandle();
            }
            texCode = texHandle | ((u32)palHandle << 16);
        } else {
            // Index out of bounds or no handle array - use texture index with palette
            u16 palHandle = ppgGetCurrentPaletteHandle();
            texCode = (u32)textureId | ((u32)palHandle << 16);
        }
    } else {
        // No current texture table - just combine with palette
        u16 palHandle = ppgGetCurrentPaletteHandle();
        texCode = (u32)textureId | ((u32)palHandle << 16);
    }

    if (texCode == 0) {
        // No texture - skip
        return;
    }

    SDLGameRenderer_SetTexture(texCode);
}

void Renderer_DrawTexturedQuad(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;

    Sprite sprite;
    for (int i = 0; i < 4; i++) {
        sprite.v[i].x = vertices[i].x;
        sprite.v[i].y = vertices[i].y;
        sprite.v[i].z = vertices[i].z;
        sprite.t[i].s = vertices[i].u;
        sprite.t[i].t = vertices[i].v;
    }

    SDLGameRenderer_DrawTexturedQuad(&sprite, vertices[0].color);
}

// Weak declaration for builds that don't link PPGFile.c (like test targets)
extern s32 ppgWriteQuadWithST_B2(Vertex* pos, u32 col, PPGDataList* tb, s32 tix, s32 cix) __attribute__((weak));

void Renderer_DrawSprite(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;

    // For PPG texture indices (small values 0-100), use ppgWriteQuadWithST_B2
    // which does proper texture handle lookup from the current data list
    if (s_CurrentTextureId >= 0 && s_CurrentTextureId < 100 && ppgWriteQuadWithST_B2 != NULL) {
        // Convert RendererVertex to Vertex for ppgWriteQuadWithST_B2
        // RendererVertex: {x,y,z,u,v,color} -> Vertex: {x,y,z,s,t}
        Vertex vtx[4];
        vtx[0].x = vertices[0].x;
        vtx[0].y = vertices[0].y;
        vtx[0].z = vertices[0].z;
        vtx[0].s = vertices[0].u;
        vtx[0].t = vertices[0].v;
        vtx[3].x = vertices[3].x;
        vtx[3].y = vertices[3].y;
        vtx[3].z = vertices[3].z;
        vtx[3].s = vertices[3].u;
        vtx[3].t = vertices[3].v;

        // Call PPG draw with texture index and palette index -1 (use current)
        ppgWriteQuadWithST_B2(vtx, vertices[0].color, NULL, s_CurrentTextureId, -1);
        return;
    }

    // For pre-combined handles (>= 0x10000) or direct handles, use SDL renderer
    Sprite sprite;
    for (int i = 0; i < 4; i++) {
        sprite.v[i].x = vertices[i].x;
        sprite.v[i].y = vertices[i].y;
        sprite.v[i].z = vertices[i].z;
        sprite.t[i].s = vertices[i].u;
        sprite.t[i].t = vertices[i].v;
    }

    SDLGameRenderer_DrawSprite(&sprite, vertices[0].color);
}

void Renderer_DrawSolidQuad(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;

    Quad quad;
    for (int i = 0; i < 4; i++) {
        quad.v[i].x = vertices[i].x;
        quad.v[i].y = vertices[i].y;
        quad.v[i].z = vertices[i].z;
    }

    SDLGameRenderer_DrawSolidQuad(&quad, vertices[0].color);
}

void Renderer_Queue2DPrimitive(const f32* pos, f32 priority, uintptr_t data, int type) {
    s32 i;
    s32 ix = s_Render2DQueue.total;
    s32 prev;

    if (ix >= 100) {
        flLogOut("Renderer: 2D primitive buffer overflow\n");
        return;
    }

    if (type == 0) {
        s_Render2DQueue.prim[ix].v[0].z = s_Render2DQueue.prim[ix].v[1].z = s_Render2DQueue.prim[ix].v[2].z =
            s_Render2DQueue.prim[ix].v[3].z = priority;
        s_Render2DQueue.prim[ix].v[0].x = pos[0];
        s_Render2DQueue.prim[ix].v[0].y = pos[1];
        s_Render2DQueue.prim[ix].v[1].x = pos[2];
        s_Render2DQueue.prim[ix].v[1].y = pos[3];
        s_Render2DQueue.prim[ix].v[2].x = pos[4];
        s_Render2DQueue.prim[ix].v[2].y = pos[5];
        s_Render2DQueue.prim[ix].v[3].x = pos[6];
        s_Render2DQueue.prim[ix].v[3].y = pos[7];
        s_Render2DQueue.prim[ix].type = 0;
        s_Render2DQueue.prim[ix].attr.color = (u32)data;
    } else if (type == 1) {
        s_Render2DQueue.prim[ix].v[0].z = priority;
        s_Render2DQueue.prim[ix].v[0].y = pos[0];
        s_Render2DQueue.prim[ix].type = 1;
        s_Render2DQueue.prim[ix].attr.work = (WORK*)data;
    }

    s_Render2DQueue.prim[ix].next = -1;

    if (s_Render2DQueue.ix1st == -1) {
        s_Render2DQueue.ix1st = ix;
    } else {
        i = s_Render2DQueue.ix1st;
        prev = -1;

        while (1) {
            if (priority > s_Render2DQueue.prim[i].v[0].z) {
                if (prev == -1) {
                    s_Render2DQueue.ix1st = ix;
                    s_Render2DQueue.prim[ix].next = i;
                } else {
                    s_Render2DQueue.prim[prev].next = ix;
                    s_Render2DQueue.prim[ix].next = i;
                }
                break;
            }

            if (s_Render2DQueue.prim[i].next == -1) {
                s_Render2DQueue.prim[i].next = ix;
                break;
            }

            prev = i;
            i = s_Render2DQueue.prim[i].next;
        }
    }

    s_Render2DQueue.total += 1;
}

void Renderer_Flush2DPrimitives(void) {
    Quad prm;
    s32 i, j;

    for (i = s_Render2DQueue.ix1st; i != -1; i = s_Render2DQueue.prim[i].next) {
        switch (s_Render2DQueue.prim[i].type) {
        case 0:
            for (j = 0; j < 4; j++) {
                prm.v[j] = s_Render2DQueue.prim[i].v[j];
            }
            SDLGameRenderer_DrawSolidQuad(&prm, s_Render2DQueue.prim[i].attr.color);
            break;

        case 1:
            shadow_drawing(s_Render2DQueue.prim[i].attr.work, (s16)s_Render2DQueue.prim[i].v[0].y);
            break;
        }
    }

    Renderer_2DQueueInit();
}

void Renderer_UpdateTexture(int textureId, const void* data, int x, int y, int width, int height) {
    // Mapping textureId to the logic expected by ppgRenewDotDataSeqs
    // Legay code used: ppgRenewDotDataSeqs(0, gix, (u32*)srcAdrs, ofs, size);
    // Here we wrap it similarly.
    ppgRenewDotDataSeqs(0, textureId, (u32*)data, x, y); // x=ofs, y=size in legacy context
}
