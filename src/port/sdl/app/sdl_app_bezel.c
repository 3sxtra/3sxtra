/**
 * @file sdl_app_bezel.c
 * @brief Bezel overlay rendering — GL, GPU, and SDL2D backends.
 *
 * Extracted from sdl_app.c to isolate bezel vertex building, GL draw calls,
 * GPU pipeline setup/shutdown, and per-frame render dispatch from the main
 * application lifecycle.  All three renderer backends (OpenGL, SDL_GPU,
 * SDL2D) are handled here so sdl_app.c only calls high-level entry points.
 */
#include "port/sdl/app/sdl_app_bezel.h"

#include "port/rendering/sdl_bezel.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_scale.h"
#include "port/sdl/renderer/sdl_texture_util.h"

#include "sf33rd/Source/Game/engine/workuser.h"

// clang-format off
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
// clang-format on

#include <string.h>

#include "port/sdl/renderer/sdl_game_renderer_internal.h"

/* ── GL bezel state ─────────────────────────────────────────────────── */
static GLuint bezel_vao;
static GLuint bezel_vbo;

/* ── GPU bezel state ────────────────────────────────────────────────── */
static SDL_GPUGraphicsPipeline* s_bezel_pipeline = NULL;
static SDL_GPUSampler* s_bezel_sampler = NULL;
static SDL_GPUBuffer* s_bezel_vertex_buffer = NULL;
static SDL_GPUTransferBuffer* s_bezel_transfer_buffer = NULL;

/* ── Shared bezel state ─────────────────────────────────────────────── */
/** @brief Bezel VBO dirty flag — skip redundant vertex uploads. */
static bool bezel_vbo_dirty = true;
static int last_p1_char = -1;
static int last_p2_char = -1;

/* ── Cached uniform locations (GL passthrough shader) ──────────────── */
/* Set via the first SDLAppBezel_RenderGL call; stable because the
   passthru shader is created once at init. */
static GLint s_bezel_pt_loc_source = -1;
static GLint s_bezel_pt_loc_source_size = -1;
static GLint s_bezel_pt_loc_projection = -1;
static GLint s_bezel_pt_loc_filter_type = -1;

/* ===================================================================
 *   Helpers
 * =================================================================== */

/** @brief Build NDC vertices for a bezel quad from an SDL_FRect.
 *
 *  Pre-builds normalised-device-coordinate vertex data for a single
 *  bezel quad into a caller-provided array of 24 floats (6 vertices
 *  × 4 components: x, y, u, v). */
static void build_bezel_vertices(const SDL_FRect* rect, int win_w, int win_h, float* out) {
    if (rect->w <= 0 || rect->h <= 0) {
        memset(out, 0, 6 * 4 * sizeof(float));
        return;
    }

    /* Convert pixel rect to NDC */
    float x1 = (rect->x / win_w) * 2.0f - 1.0f;
    float y1 = 1.0f - (rect->y / win_h) * 2.0f;
    float x2 = ((rect->x + rect->w) / win_w) * 2.0f - 1.0f;
    float y2 = 1.0f - ((rect->y + rect->h) / win_h) * 2.0f;

    /* Triangle 1 */
    out[0] = x1;
    out[1] = y1;
    out[2] = 0.0f;
    out[3] = 0.0f;
    out[4] = x1;
    out[5] = y2;
    out[6] = 0.0f;
    out[7] = 1.0f;
    out[8] = x2;
    out[9] = y2;
    out[10] = 1.0f;
    out[11] = 1.0f;
    /* Triangle 2 */
    out[12] = x1;
    out[13] = y1;
    out[14] = 0.0f;
    out[15] = 0.0f;
    out[16] = x2;
    out[17] = y2;
    out[18] = 1.0f;
    out[19] = 1.0f;
    out[20] = x2;
    out[21] = y1;
    out[22] = 1.0f;
    out[23] = 0.0f;
}

/** @brief Draw a single bezel quad using the GL passthrough shader.
 *  @param texture       Opaque texture handle (cast to GLuint internally).
 *  @param vertex_offset First vertex index in the bezel VAO (0 = left, 6 = right). */
static void draw_bezel_quad_gl(void* texture, int vertex_offset) {
    GLuint tex = (GLuint)(intptr_t)texture;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(s_bezel_pt_loc_source, 0);

    int tw = 0, th = 0;
    TextureUtil_GetSize(texture, &tw, &th);
    glUniform4f(s_bezel_pt_loc_source_size,
                (float)tw,
                (float)th,
                tw > 0 ? 1.0f / (float)tw : 0.0f,
                th > 0 ? 1.0f / (float)th : 0.0f);

    glDrawArrays(GL_TRIANGLES, vertex_offset, 6);
}

/** @brief Compile a single SPIR-V shader stage via SDL_ShaderCross. */
static SDL_GPUShader* CreateBezelGPUShader(SDL_GPUDevice* dev, const char* filename, SDL_GPUShaderStage stage) {
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

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(dev, &info, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(code);
    return shader;
}

/** @brief Update character tracking and mark dirty if changed. */
static void update_character_tracking(void) {
    int p1 = My_char[0];
    int p2 = My_char[1];

    /* Only show character-specific bezels during gameplay */
    if (!(G_No[0] == 2 && G_No[1] >= 2)) {
        p1 = -1;
        p2 = -1;
    }

    if (p1 != last_p1_char || p2 != last_p2_char) {
        last_p1_char = p1;
        last_p2_char = p2;
        BezelSystem_SetCharacters(last_p1_char, last_p2_char);
        bezel_vbo_dirty = true;
    }
}

/* ===================================================================
 *   Public API — Initialisation / Shutdown
 * =================================================================== */

void SDLAppBezel_InitGL(void) {
    glGenVertexArrays(1, &bezel_vao);
    glGenBuffers(1, &bezel_vbo);

    glBindVertexArray(bezel_vao);
    glBindBuffer(GL_ARRAY_BUFFER, bezel_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void SDLAppBezel_InitGPU(const char* base_path) {
    SDL_GPUDevice* dev = SDLApp_GetGPUDevice();
    SDL_Window* win = SDLApp_GetWindow();
    if (!dev || !win)
        return;

    char vert_path[1024];
    char frag_path[1024];
    snprintf(vert_path, sizeof(vert_path), "%sshaders/blit.vert.spv", base_path);
    snprintf(frag_path, sizeof(frag_path), "%sshaders/blit.frag.spv", base_path);

    SDL_GPUShader* vert = CreateBezelGPUShader(dev, vert_path, SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* frag = CreateBezelGPUShader(dev, frag_path, SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (!vert || !frag)
        return;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);
    pipeline_info.vertex_shader = vert;
    pipeline_info.fragment_shader = frag;

    /* Attributes: Pos (vec2), UV (vec2) */
    SDL_GPUVertexAttribute attrs[2];
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = 0;
    attrs[0].buffer_slot = 0;

    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[1].offset = 2 * sizeof(float);
    attrs[1].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 2;

    SDL_GPUVertexBufferDescription bindings[1];
    bindings[0].slot = 0;
    bindings[0].pitch = 4 * sizeof(float);
    bindings[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = bindings;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    SDL_GPUColorTargetDescription target_desc;
    SDL_zero(target_desc);
    target_desc.format = SDL_GetGPUSwapchainTextureFormat(dev, win);
    target_desc.blend_state.enable_blend = true;
    target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    pipeline_info.target_info.color_target_descriptions = &target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    s_bezel_pipeline = SDL_CreateGPUGraphicsPipeline(dev, &pipeline_info);
    SDL_ReleaseGPUShader(dev, vert);
    SDL_ReleaseGPUShader(dev, frag);

    /* Sampler */
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    s_bezel_sampler = SDL_CreateGPUSampler(dev, &sampler_info);

    /* Buffers — 2 quads × 6 verts × 4 floats */
    size_t buf_size = 48 * sizeof(float);

    SDL_GPUBufferCreateInfo b_info = { .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = buf_size };
    s_bezel_vertex_buffer = SDL_CreateGPUBuffer(dev, &b_info);

    SDL_GPUTransferBufferCreateInfo tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = buf_size };
    s_bezel_transfer_buffer = SDL_CreateGPUTransferBuffer(dev, &tb_info);
}

void SDLAppBezel_Shutdown(void) {
    SDL_GPUDevice* dev = SDLApp_GetGPUDevice();

    /* GPU resources */
    if (dev) {
        if (s_bezel_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(dev, s_bezel_pipeline);
        if (s_bezel_sampler)
            SDL_ReleaseGPUSampler(dev, s_bezel_sampler);
        if (s_bezel_vertex_buffer)
            SDL_ReleaseGPUBuffer(dev, s_bezel_vertex_buffer);
        if (s_bezel_transfer_buffer)
            SDL_ReleaseGPUTransferBuffer(dev, s_bezel_transfer_buffer);
    }
    s_bezel_pipeline = NULL;
    s_bezel_sampler = NULL;
    s_bezel_vertex_buffer = NULL;
    s_bezel_transfer_buffer = NULL;

    /* GL resources — safe to call with 0 (no-op) */
    if (bezel_vao) {
        glDeleteVertexArrays(1, &bezel_vao);
        bezel_vao = 0;
    }
    if (bezel_vbo) {
        glDeleteBuffers(1, &bezel_vbo);
        bezel_vbo = 0;
    }
}

/* ===================================================================
 *   Public API — Per-frame Rendering
 * =================================================================== */

void SDLAppBezel_RenderGL(int win_w, int win_h, const SDL_FRect* viewport, unsigned int passthru,
                          const float* identity) {
    if (!BezelSystem_IsVisible())
        return;

    update_character_tracking();

    static SDL_FRect cached_left = { 0 }, cached_right = { 0 };
    static BezelTextures cached_bezels = { 0 };
    static float bezel_vertex_data[2 * 6 * 4] = { 0 };

    if (bezel_vbo_dirty) {
        BezelSystem_CalculateLayout(win_w, win_h, viewport, &cached_left, &cached_right);
        BezelSystem_GetTextures(&cached_bezels);

        build_bezel_vertices(&cached_left, win_w, win_h, &bezel_vertex_data[0]);
        build_bezel_vertices(&cached_right, win_w, win_h, &bezel_vertex_data[24]);

        glBindBuffer(GL_ARRAY_BUFFER, bezel_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(bezel_vertex_data), bezel_vertex_data, GL_DYNAMIC_DRAW);
        bezel_vbo_dirty = false;
    }

    if (cached_bezels.left || cached_bezels.right) {
        /* Reset viewport to full window for NDC-based bezel quads */
        glViewport(0, 0, win_w, win_h);

        glUseProgram(passthru);

        /* Resolve uniform locations once */
        if (s_bezel_pt_loc_projection == -1) {
            s_bezel_pt_loc_projection = glGetUniformLocation(passthru, "projection");
            s_bezel_pt_loc_source = glGetUniformLocation(passthru, "Source");
            s_bezel_pt_loc_source_size = glGetUniformLocation(passthru, "SourceSize");
            s_bezel_pt_loc_filter_type = glGetUniformLocation(passthru, "u_filter_type");
        }

        glUniformMatrix4fv(s_bezel_pt_loc_projection, 1, GL_FALSE, identity);
        glUniform1i(s_bezel_pt_loc_filter_type, 0); /* nearest */

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(bezel_vao);

        if (cached_bezels.left)
            draw_bezel_quad_gl(cached_bezels.left, 0);
        if (cached_bezels.right)
            draw_bezel_quad_gl(cached_bezels.right, 6);

        glDisable(GL_BLEND);
        glBindVertexArray(0);
    }
}

void SDLAppBezel_RenderGPU(int win_w, int win_h) {
    if (!BezelSystem_IsVisible())
        return;

    SDL_GPUDevice* dev = SDLApp_GetGPUDevice();
    if (!dev)
        return;

    update_character_tracking();

    static SDL_FRect cached_left = { 0 }, cached_right = { 0 };
    static BezelTextures cached_bezels = { 0 };
    static float bezel_vertex_data[2 * 6 * 4] = { 0 };

    if (bezel_vbo_dirty) {
        const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);
        BezelSystem_CalculateLayout(win_w, win_h, &viewport, &cached_left, &cached_right);
        BezelSystem_GetTextures(&cached_bezels);

        build_bezel_vertices(&cached_left, win_w, win_h, &bezel_vertex_data[0]);
        build_bezel_vertices(&cached_right, win_w, win_h, &bezel_vertex_data[24]);

        /* Upload to GPU */
        void* ptr = SDL_MapGPUTransferBuffer(dev, s_bezel_transfer_buffer, true);
        if (ptr) {
            memcpy(ptr, bezel_vertex_data, sizeof(bezel_vertex_data));
            SDL_UnmapGPUTransferBuffer(dev, s_bezel_transfer_buffer);

            SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
            if (cb) {
                SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);
                SDL_GPUTransferBufferLocation src = { .transfer_buffer = s_bezel_transfer_buffer, .offset = 0 };
                SDL_GPUBufferRegion dst = { .buffer = s_bezel_vertex_buffer,
                                            .offset = 0,
                                            .size = sizeof(bezel_vertex_data) };
                SDL_UploadToGPUBuffer(cp, &src, &dst, false);
                SDL_EndGPUCopyPass(cp);
            }
        }
        bezel_vbo_dirty = false;
    }

    SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
    if (cb && s_bezel_pipeline) {
        SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();
        if (swapchain) {
            SDL_GPUColorTargetInfo target;
            SDL_zero(target);
            target.texture = swapchain;
            target.load_op = SDL_GPU_LOADOP_LOAD;
            target.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &target, 1, NULL);
            if (pass) {
                SDL_BindGPUGraphicsPipeline(pass, s_bezel_pipeline);

                SDL_GPUBufferBinding vb = { .buffer = s_bezel_vertex_buffer, .offset = 0 };
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                /* Draw Left */
                if (cached_bezels.left) {
                    SDL_GPUTextureSamplerBinding tb = { .texture = (SDL_GPUTexture*)cached_bezels.left,
                                                        .sampler = s_bezel_sampler };
                    SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
                    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
                }
                /* Draw Right */
                if (cached_bezels.right) {
                    SDL_GPUTextureSamplerBinding tb = { .texture = (SDL_GPUTexture*)cached_bezels.right,
                                                        .sampler = s_bezel_sampler };
                    SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
                    SDL_DrawGPUPrimitives(pass, 6, 1, 6, 0);
                }
                SDL_EndGPURenderPass(pass);
            }
        }
    }
}

void SDLAppBezel_RenderSDL2D(SDL_Renderer* renderer, int win_w, int win_h, const SDL_FRect* dst_rect) {
    if (!BezelSystem_IsVisible())
        return;

    update_character_tracking();

    BezelTextures bezels;
    BezelSystem_GetTextures(&bezels);
    SDL_FRect left_dst, right_dst;
    BezelSystem_CalculateLayout(win_w, win_h, dst_rect, &left_dst, &right_dst);

    if (bezels.left)
        SDL_RenderTexture(renderer, (SDL_Texture*)bezels.left, NULL, &left_dst);
    if (bezels.right)
        SDL_RenderTexture(renderer, (SDL_Texture*)bezels.right, NULL, &right_dst);
}

void SDLAppBezel_MarkDirty(void) {
    bezel_vbo_dirty = true;
}
