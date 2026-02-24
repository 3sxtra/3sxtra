#ifndef PORT_RENDERER_H
#define PORT_RENDERER_H

#include "structs.h"
#include "types.h"

// Basic types for the renderer
// RendererVertex is defined in structs.h

typedef enum {
    RENDERER_BLEND_NORMAL,
    RENDERER_BLEND_ADD,
    // Add others as discovered (e.g., subtract, multiply)
} RendererBlendMode;

// Initialization and Lifecycle
void Renderer_Init(void);
void Renderer_BeginFrame(void);
void Renderer_EndFrame(void);

// State Management
void Renderer_SetBlendMode(RendererBlendMode mode);
void Renderer_SetTexture(int textureId);
void Renderer_SetCurrentTexture(Texture* tex);

// Drawing Commands

/**
 * @brief Draws a textured quad using the current texture.
 * Corresponds to legacy ppgWriteQuadWithST_B / njDrawTexture
 */
void Renderer_DrawTexturedQuad(const RendererVertex* vertices, int count);

/**
 * @brief Draws a sprite (2D textured quad).
 * Corresponds to legacy ppgWriteQuadWithST_B2 / njDrawSprite
 */
void Renderer_DrawSprite(const RendererVertex* vertices, int count);

/**
 * @brief Draws a solid colored quad (no texture).
 * Corresponds to legacy SDLGameRenderer_DrawSolidQuad
 */
void Renderer_DrawSolidQuad(const RendererVertex* vertices, int count);

/**
 * @brief Queues a 2D primitive with depth sorting.
 * Corresponds to legacy njdp2d_sort / njDrawPolygon2D
 * @param pos Array of floats. If type is 0: {x0,y0, x1,y1, x2,y2, x3,y3}. If type is 1: {bsy} (y-offset)
 * @param priority Z-depth/Priority
 * @param data Color (u32) if type is 0, or WORK* (uintptr_t) if type is 1
 * @param type 0 for normal, 1 for shadow/work
 */
void Renderer_Queue2DPrimitive(const f32* pos, f32 priority, uintptr_t data, int type);

/**
 * @brief Processes and draws all queued 2D primitives.
 * Corresponds to legacy njdp2d_draw
 */
void Renderer_Flush2DPrimitives(void);

// Texture Management
void Renderer_UpdateTexture(int textureId, const void* data, int x, int y, int width, int height);

#endif // PORT_RENDERER_H
