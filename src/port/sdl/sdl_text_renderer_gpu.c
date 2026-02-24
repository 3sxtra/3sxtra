/**
 * @file sdl_text_renderer_gpu.c
 * @brief SDL_GPU text rendering backend.
 *
 * Implements bitmap font text rendering using SDL3's GPU API with
 * batched vertex submission, background rectangles, and shader-based
 * glyph rendering. Alternative to the OpenGL text renderer.
 */
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "port/sdl/sdl_text_renderer.h"
#include "port/sdl/sdl_text_renderer_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/imgui_font_8x8.h"

// Font Atlas
typedef struct {
    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
    int width;
    int height;
} FontAtlasGPU;

static FontAtlasGPU s_font_atlas;
static SDL_GPUGraphicsPipeline* s_text_pipeline = NULL;
static SDL_GPUGraphicsPipeline* s_rect_pipeline = NULL;

// Buffering
#define MAX_TEXT_VERTICES 8192
typedef struct {
    float x, y;
    float u, v;
    float r, g, b, a;
} TextVertex;

typedef struct {
    float x, y;
    float r, g, b, a;
} RectVertex;

static TextVertex* s_text_verts = NULL;
static int s_text_vert_count = 0;
static RectVertex* s_rect_verts = NULL;
static int s_rect_vert_count = 0;

static SDL_GPUBuffer* s_vertex_buffer = NULL;           // Shared buffer
static SDL_GPUTransferBuffer* s_transfer_buffer = NULL; // Cycle=true

// State
static float s_text_y_offset = 8.0f;
static int s_bg_enabled = 1;
static float s_bg_color[4] = { 0.0f, 0.0f, 0.0f, 0.6f };
static float s_bg_padding = 2.0f;

static SDL_GPUDevice* device = NULL;

static SDL_GPUShader* CreateGPUShader(const char* filename, SDL_GPUShaderStage stage) {
    size_t size;
    void* code = SDL_LoadFile(filename, &size);
    if (!code) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Failed to load shader: %s", filename);
        return NULL;
    }

    SDL_ShaderCross_SPIRV_Info info;
    SDL_zero(info);
    info.bytecode = (const Uint8*)code;
    info.bytecode_size = size;
    info.entrypoint = "main";
    info.shader_stage = (SDL_ShaderCross_ShaderStage)stage;

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);

    if (!metadata) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to reflect SPIRV: %s", filename);
        SDL_free(code);
        return NULL;
    }

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &info, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(code);
    return shader;
}

/** @brief Initialize the SDL_GPU text renderer (shaders, pipelines, font atlas). */
void SDLTextRendererGPU_Init(const char* base_path, const char* font_path) {
    (void)font_path; // Unused, we use internal 8x8 font
    SDL_Log("Initializing SDL_GPU text renderer...");
    device = SDLApp_GetGPUDevice();
    if (!device)
        return;

    // Allocate CPU buffers
    s_text_verts = (TextVertex*)SDL_malloc(MAX_TEXT_VERTICES * sizeof(TextVertex));
    s_rect_verts = (RectVertex*)SDL_malloc(MAX_TEXT_VERTICES * sizeof(RectVertex)); // Reuse size

    if (!s_text_verts || !s_rect_verts) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to allocate text renderer buffers");
        return;
    }

    // Load Shaders
    char path[1024];
    snprintf(path, sizeof(path), "%sshaders/text.vert.spv", base_path);
    SDL_GPUShader* text_vert = CreateGPUShader(path, SDL_GPU_SHADERSTAGE_VERTEX);

    snprintf(path, sizeof(path), "%sshaders/text.frag.spv", base_path);
    SDL_GPUShader* text_frag = CreateGPUShader(path, SDL_GPU_SHADERSTAGE_FRAGMENT);

    snprintf(path, sizeof(path), "%sshaders/rect.vert.spv", base_path);
    SDL_GPUShader* rect_vert = CreateGPUShader(path, SDL_GPU_SHADERSTAGE_VERTEX);

    snprintf(path, sizeof(path), "%sshaders/rect.frag.spv", base_path);
    SDL_GPUShader* rect_frag = CreateGPUShader(path, SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (!text_vert || !text_frag || !rect_vert || !rect_frag) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create text/rect shaders.");
        return;
    }

    // Create Text Pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);
    pipeline_info.vertex_shader = text_vert;
    pipeline_info.fragment_shader = text_frag;

    SDL_GPUVertexAttribute text_attrs[3];
    // Pos
    text_attrs[0].location = 0;
    text_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    text_attrs[0].offset = 0;
    text_attrs[0].buffer_slot = 0;
    // UV
    text_attrs[1].location = 1;
    text_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    text_attrs[1].offset = 2 * sizeof(float);
    text_attrs[1].buffer_slot = 0;
    // Color
    text_attrs[2].location = 2;
    text_attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    text_attrs[2].offset = 4 * sizeof(float);
    text_attrs[2].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = text_attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 3;

    SDL_GPUVertexBufferDescription text_binding;
    SDL_zero(text_binding);
    text_binding.slot = 0;
    text_binding.pitch = sizeof(TextVertex);
    text_binding.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &text_binding;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;

    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // Blending
    SDL_GPUColorTargetDescription target_desc;
    SDL_zero(target_desc);
    target_desc.format = SDL_GetGPUSwapchainTextureFormat(device, SDLApp_GetWindow());
    target_desc.blend_state.enable_blend = true;
    target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    pipeline_info.target_info.color_target_descriptions = &target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    s_text_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);

    // Create Rect Pipeline
    SDL_zero(pipeline_info);
    pipeline_info.vertex_shader = rect_vert;
    pipeline_info.fragment_shader = rect_frag;

    SDL_GPUVertexAttribute rect_attrs[2];
    rect_attrs[0].location = 0;
    rect_attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    rect_attrs[0].offset = 0;
    rect_attrs[0].buffer_slot = 0;
    rect_attrs[1].location = 1;
    rect_attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    rect_attrs[1].offset = 2 * sizeof(float);
    rect_attrs[1].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = rect_attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 2;

    SDL_GPUVertexBufferDescription rect_binding;
    SDL_zero(rect_binding);
    rect_binding.slot = 0;
    rect_binding.pitch = sizeof(RectVertex);
    rect_binding.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &rect_binding;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.target_info.color_target_descriptions = &target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    s_rect_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);

    if (!s_text_pipeline || !s_rect_pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create text pipelines");
    }

    SDL_ReleaseGPUShader(device, text_vert);
    SDL_ReleaseGPUShader(device, text_frag);
    SDL_ReleaseGPUShader(device, rect_vert);
    SDL_ReleaseGPUShader(device, rect_frag);

    // Load Font
    s_font_atlas.width = 128;
    s_font_atlas.height = 64;
    unsigned char* bitmap = (unsigned char*)SDL_calloc(1, s_font_atlas.width * s_font_atlas.height);

    for (int ch = 0; ch < 128; ch++) {
        int cx = (ch % 16) * 8;
        int cy = (ch / 16) * 8;
        for (int row = 0; row < 8; row++) {
            uint8_t row_data = font8x8_basic[ch][row];
            for (int col = 0; col < 8; col++) {
                if (row_data & (1 << col)) {
                    bitmap[(cy + row) * s_font_atlas.width + (cx + col)] = 255;
                }
            }
        }
    }

    // Upload Texture
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = s_font_atlas.width;
    tex_info.height = s_font_atlas.height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;

    s_font_atlas.texture = SDL_CreateGPUTexture(device, &tex_info);

    SDL_GPUTransferBufferCreateInfo tb_info_font = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                     .size = s_font_atlas.width * s_font_atlas.height };
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tb_info_font);
    void* ptr = SDL_MapGPUTransferBuffer(device, tb, false);
    memcpy(ptr, bitmap, s_font_atlas.width * s_font_atlas.height);
    SDL_UnmapGPUTransferBuffer(device, tb);
    SDL_free(bitmap);

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureTransferInfo src = { .transfer_buffer = tb };
    SDL_GPUTextureRegion dst = {
        .texture = s_font_atlas.texture, .w = s_font_atlas.width, .h = s_font_atlas.height, .d = 1
    };
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cb);
    SDL_ReleaseGPUTransferBuffer(device, tb);

    // Sampler
    SDL_GPUSamplerCreateInfo samp_info;
    SDL_zero(samp_info);
    samp_info.min_filter = SDL_GPU_FILTER_NEAREST;
    samp_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    samp_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    s_font_atlas.sampler = SDL_CreateGPUSampler(device, &samp_info);

    // Vertex Buffer & Transfer Buffer
    SDL_GPUBufferCreateInfo b_info = { .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                       .size = MAX_TEXT_VERTICES * sizeof(TextVertex) +
                                               MAX_TEXT_VERTICES * sizeof(RectVertex) };
    s_vertex_buffer = SDL_CreateGPUBuffer(device, &b_info);

    SDL_GPUTransferBufferCreateInfo tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                .size = MAX_TEXT_VERTICES * sizeof(TextVertex) +
                                                        MAX_TEXT_VERTICES * sizeof(RectVertex) };
    s_transfer_buffer = SDL_CreateGPUTransferBuffer(device, &tb_info);
}

void SDLTextRendererGPU_Shutdown(void) {
    if (s_text_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, s_text_pipeline);
    if (s_rect_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, s_rect_pipeline);
    if (s_font_atlas.texture)
        SDL_ReleaseGPUTexture(device, s_font_atlas.texture);
    if (s_font_atlas.sampler)
        SDL_ReleaseGPUSampler(device, s_font_atlas.sampler);
    if (s_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, s_vertex_buffer);
    if (s_transfer_buffer)
        SDL_ReleaseGPUTransferBuffer(device, s_transfer_buffer);
    if (s_text_verts)
        SDL_free(s_text_verts);
    if (s_rect_verts)
        SDL_free(s_rect_verts);
}

/** @brief Queue text for rendering. */
void SDLTextRendererGPU_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                 float target_width, float target_height) {
    (void)target_width;
    (void)target_height;
    y += s_text_y_offset;

    float glyph_w = 8.0f;
    float glyph_h = 10.0f;
    float x_advance = 7.0f;

    // Background Rect Logic
    if (s_bg_enabled) {
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        const char* pp;
        float tx = 0, ty = 0; // Relative coordinates
        for (pp = text; *pp; pp++) {
            if (*pp < 32 || *pp >= 127 || *pp == ' ') {
                // Non-printable or unmapped character
                if (*pp == ' ')
                    tx += x_advance;
            } else {
                if (tx < minx)
                    minx = tx;
                if (ty < miny)
                    miny = ty;
                if (tx + glyph_w > maxx)
                    maxx = tx + glyph_w;
                if (ty + glyph_h > maxy)
                    maxy = ty + glyph_h;
                tx += x_advance;
            }
        }

        if (minx <= maxx && miny <= maxy) {
            float px = s_bg_padding;
            float x0 = x + (minx * scale) - px;
            float y0 = y + (miny * scale) - px;
            float x1 = x + (maxx * scale) + px;
            float y1 = y + (maxy * scale) + px;

            if (s_rect_vert_count + 6 <= MAX_TEXT_VERTICES) {
                RectVertex* v = &s_rect_verts[s_rect_vert_count];
                // Quad (2 triangles)
                // BL -> BR -> TR
                v[0] = (RectVertex) { x0, y1, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                v[1] = (RectVertex) { x1, y1, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                v[2] = (RectVertex) { x1, y0, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                // TR -> TL -> BL
                v[3] = (RectVertex) { x1, y0, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                v[4] = (RectVertex) { x0, y0, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                v[5] = (RectVertex) { x0, y1, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3] };
                s_rect_vert_count += 6;
            }
        }
    }

    const char* p;
    float current_rx = 0;
    float current_ry = 0;
    for (p = text; *p; p++) {
        unsigned char ch = *p;
        if (ch == ' ') {
            current_rx += x_advance;
            continue;
        }

        // Map to 128-char atlas (ASCII only)
        if (ch >= 128)
            ch = 127;

        float u0 = (ch % 16) * 8.0f / s_font_atlas.width;
        float v0 = (ch / 16) * 8.0f / s_font_atlas.height;
        float u1 = u0 + (8.0f / s_font_atlas.width);
        float v1 = v0 + (8.0f / s_font_atlas.height);

        if (s_text_vert_count + 6 <= MAX_TEXT_VERTICES) {
            TextVertex* v = &s_text_verts[s_text_vert_count];
            // Quad
            float qx0 = x + (current_rx * scale);
            float qx1 = x + ((current_rx + glyph_w) * scale);
            float qy0 = y + (current_ry * scale);
            float qy1 = y + ((current_ry + glyph_h) * scale);

            v[0] = (TextVertex) { qx0, qy1, u0, v1, r, g, b, 1.0f };
            v[1] = (TextVertex) { qx1, qy1, u1, v1, r, g, b, 1.0f };
            v[2] = (TextVertex) { qx1, qy0, u1, v0, r, g, b, 1.0f };
            v[3] = (TextVertex) { qx1, qy0, u1, v0, r, g, b, 1.0f };
            v[4] = (TextVertex) { qx0, qy0, u0, v0, r, g, b, 1.0f };
            v[5] = (TextVertex) { qx0, qy1, u0, v1, r, g, b, 1.0f };
            s_text_vert_count += 6;
        }

        current_rx += x_advance;
    }
}

/** @brief Flush queued text and background rects to the GPU. */
void SDLTextRendererGPU_Flush(void) {
    if (s_text_vert_count == 0 && s_rect_vert_count == 0)
        return;

    SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
    if (!cb)
        return;

    SDL_GPUTexture* swapchain_texture = SDLGameRendererGPU_GetSwapchainTexture();
    if (!swapchain_texture)
        return;

    // Upload vertices
    void* map = SDL_MapGPUTransferBuffer(device, s_transfer_buffer, true);
    if (!map)
        return;

    size_t rect_size = s_rect_vert_count * sizeof(RectVertex);
    size_t text_size = s_text_vert_count * sizeof(TextVertex);

    if (rect_size > 0)
        memcpy(map, s_rect_verts, rect_size);
    if (text_size > 0)
        memcpy((char*)map + rect_size, s_text_verts, text_size);

    SDL_UnmapGPUTransferBuffer(device, s_transfer_buffer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTransferBufferLocation src = { .transfer_buffer = s_transfer_buffer, .offset = 0 };
    SDL_GPUBufferRegion dst = { .buffer = s_vertex_buffer, .offset = 0, .size = rect_size + text_size };
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    // Render Pass (LOAD)
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = swapchain_texture;
    color_target.load_op = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &color_target, 1, NULL);
    if (pass) {
        int w, h;
        SDL_GetWindowSizeInPixels(SDLApp_GetWindow(), &w, &h);
        SDL_GPUViewport viewport = { 0, 0, (float)w, (float)h, 0.0f, 1.0f };
        SDL_SetGPUViewport(pass, &viewport);
        SDL_Rect scissor = { 0, 0, w, h };
        SDL_SetGPUScissor(pass, &scissor);

        // Projection Matrix
        float matrix[4][4] = { { 2.0f / w, 0.0f, 0.0f, 0.0f },
                               { 0.0f, -2.0f / h, 0.0f, 0.0f },
                               { 0.0f, 0.0f, -1.0f, 0.0f },
                               { -1.0f, 1.0f, 0.0f, 1.0f } };
        // Draw Rects
        if (s_rect_vert_count > 0) {
            SDL_BindGPUGraphicsPipeline(pass, s_rect_pipeline);
            SDL_PushGPUVertexUniformData(cb, 0, matrix, sizeof(matrix));
            SDL_GPUBufferBinding vb = { .buffer = s_vertex_buffer, .offset = 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            SDL_DrawGPUPrimitives(pass, s_rect_vert_count, 1, 0, 0);
        }

        // Draw Text
        if (s_text_vert_count > 0) {
            SDL_BindGPUGraphicsPipeline(pass, s_text_pipeline);
            SDL_PushGPUVertexUniformData(cb, 0, matrix, sizeof(matrix));
            SDL_GPUBufferBinding vb = { .buffer = s_vertex_buffer, .offset = rect_size };
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUTextureSamplerBinding tex_binding = { .texture = s_font_atlas.texture,
                                                         .sampler = s_font_atlas.sampler };
            SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

            SDL_DrawGPUPrimitives(pass, s_text_vert_count, 1, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    // Reset counts
    s_text_vert_count = 0;
    s_rect_vert_count = 0;
}

/** @brief Set vertical offset for text rendering. */
void SDLTextRendererGPU_SetYOffset(float y_offset) {
    s_text_y_offset = y_offset;
}
/** @brief Enable or disable the background rectangle behind text. */
void SDLTextRendererGPU_SetBackgroundEnabled(int enabled) {
    s_bg_enabled = enabled;
}
/** @brief Set the color of the text background rectangle. */
void SDLTextRendererGPU_SetBackgroundColor(float r, float g, float b, float a) {
    s_bg_color[0] = r;
    s_bg_color[1] = g;
    s_bg_color[2] = b;
    s_bg_color[3] = a;
}
/** @brief Set the padding of the text background rectangle. */
void SDLTextRendererGPU_SetBackgroundPadding(float px) {
    s_bg_padding = px;
}
