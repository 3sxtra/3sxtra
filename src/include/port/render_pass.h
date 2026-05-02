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

    // Custom pass execution callback (if NULL, backend default is used)
    void (*execute_callback)(int pass_index, void* user_data, int vp_x, int vp_y, int vp_w, int vp_h);
    void* user_data;

    // Phase 1 FrameGraph additions:
    bool has_geometry;
    bool force_execute; // e.g. for clears
    bool skip_this_frame;

    // FrameGraph Dependency Tracking:
    int transient_output;    // TransientTextureID to write to, or -1 for backbuffer
    int transient_inputs[4]; // TransientTextureIDs to read from, -1 for none

    // Backend fills in native handles:
    void* framebuffer; // GLuint FBO / SDL_GPUTexture* / NULL for backbuffer
} RenderPass;

typedef struct RenderPassTable {
    RenderPass passes[8]; // Max 8 passes
    int count;
} RenderPassTable;

extern RenderPassTable g_render_passes;

void RenderGraph_Compile(void);
void RenderGraph_Execute(int vp_x, int vp_y, int vp_w, int vp_h);

#endif // RENDER_PASS_H
