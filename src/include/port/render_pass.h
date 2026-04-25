#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct RenderPass {
    const char* name;       // "CPS3 Canvas", "Screen Upscale", "HD Overlay"
    uint16_t x, y;          // Viewport origin offset (for letterboxing)
    uint16_t width, height; // Render target dimensions (0 = backbuffer size)
    bool clear_color;
    bool clear_depth;
    uint32_t clear_color_value; // RGBA
    float clear_depth_value;
    // Phase 1 FrameGraph additions:
    bool has_geometry;
    bool force_execute; // e.g. for clears
    bool skip_this_frame;

    // Backend fills in native handles:
    void* framebuffer; // GLuint FBO / SDL_GPUTexture* / NULL for backbuffer
} RenderPass;

typedef struct RenderPassTable {
    RenderPass passes[8]; // Max 8 passes
    int count;
} RenderPassTable;

extern RenderPassTable g_render_passes;

void RenderGraph_Compile(void);

#endif // RENDER_PASS_H
