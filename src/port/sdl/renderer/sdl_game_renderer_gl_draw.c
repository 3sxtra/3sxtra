/**
 * @file sdl_game_renderer_gl_draw.c
 * @brief OpenGL renderer draw commands and frame lifecycle.
 *
 * Implements batched sprite/quad rendering, render task management,
 * frame begin/end, and the OpenGL draw pipeline including shader pass
 * application via librashader. Part of the GL rendering backend.
 */
#include "port/mods/modded_stage.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/renderer/sdl_game_renderer_gl_internal.h"
#include "port/tracy_gpu.h"
#include "port/tracy_zones.h"
#include "port/render_pass.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include <stddef.h>
#include <string.h>

#include "radix_sort.h"

// --- Render Task Management ---

static void push_render_task(GLuint texture, const SDL_Vertex* vertices, float z, int array_layer, int pal_slot) {
    if (gl_state.render_task_count >= RENDER_TASK_MAX) {
        SDL_Log("Warning: render task buffer full, skipping task");
        return;
    }

    const int vertex_offset = gl_state.render_task_count * 4;
    memcpy(&gl_state.batch_vertices[vertex_offset], vertices, 4 * sizeof(SDL_Vertex));

    // ⚡ Bolt: SIMD broadcast — write same float to 4 consecutive slots
    simde_mm_storeu_ps(&gl_state.batch_layers[vertex_offset], simde_mm_set1_ps((float)array_layer));
    simde_mm_storeu_ps(&gl_state.batch_pal_indices[vertex_offset], simde_mm_set1_ps((float)pal_slot));
    simde_mm_storeu_ps(&gl_state.batch_z[vertex_offset], simde_mm_set1_ps(z));

    RenderTask* task = &gl_state.render_tasks[gl_state.render_task_count];
    task->texture = texture;
    task->vertex_offset = vertex_offset;
    task->z = z;
    task->original_index = gl_state.render_task_count;
    task->index = gl_state.render_task_count;
    task->array_layer = array_layer;
    task->palette_slot = pal_slot;
    task->blend_mode = gl_state.current_blend_mode;

    if (z < gl_state.last_submitted_z) {
        gl_state.sort_inversions++;
    }
    gl_state.last_submitted_z = z;

    gl_state.render_task_count++;
}

static void clear_render_tasks(void) {
    gl_state.render_task_count = 0;
    gl_state.sort_inversions = 0;
    gl_state.last_submitted_z = -1e30f;
}

// ⚡ Radix sort scratch buffers (used when sort_inversions > INSERTION_SORT_THRESHOLD)
static uint32_t radix_keys[RENDER_TASK_MAX];
static int radix_scratch[RENDER_TASK_MAX];
static int render_task_order[RENDER_TASK_MAX];
static float task_z[RENDER_TASK_MAX];

#define INSERTION_SORT_THRESHOLD 16

static void insertion_sort_render_tasks(void) {
    const int count = gl_state.render_task_count;
    RenderTask* tasks = gl_state.render_tasks;
    for (int i = 1; i < count; i++) {
        const RenderTask key = tasks[i];
        int j = i - 1;
        while (j >= 0) {
            if (tasks[j].z <= key.z)
                break;
            tasks[j + 1] = tasks[j];
            j--;
        }
        tasks[j + 1] = key;
    }
}

// turbo
// What: Convert O(N log N) render task merge sort into an O(N) adaptive radix/insertion sort.
// Target: CPU Algorithmic Complexity / Branch Prediction.
// Expected Impact: Eliminates the O(N log N) merge sort overhead and associated branch mispredictions. Gameplay
// typically features near-sorted z-depths where insertion sort achieves O(N).
static void stable_sort_render_tasks(void) {
    const int count = gl_state.render_task_count;
    if (count <= 1)
        return;

    if (gl_state.sort_inversions <= INSERTION_SORT_THRESHOLD) {
        insertion_sort_render_tasks();
    } else {
        // Prepare arrays for radix sort
        for (int i = 0; i < count; i++) {
            task_z[i] = gl_state.render_tasks[i].z;
            render_task_order[i] = i;
        }

        radix_sort_render_task_indices(render_task_order, task_z, count, radix_keys, radix_scratch);

        // Reorder render_tasks using merge_temp as scratch space
        for (int i = 0; i < count; i++) {
            gl_state.merge_temp[i] = gl_state.render_tasks[render_task_order[i]];
        }
        memcpy(gl_state.render_tasks, gl_state.merge_temp, (size_t)count * sizeof(RenderTask));
    }
}

// --- Frame ---

void SDLGameRendererGL_BeginFrame(void) {
    TRACE_ZONE_N("BeginFrame");
    gl_state.last_set_texture_th = 0;

    for (int i = 0; i < gl_state.dirty_texture_count; i++) {
        const int idx = gl_state.dirty_texture_indices[i];
        SDLGameRendererGL_DestroyTexture(idx + 1);
        SDLGameRendererGL_CreateTexture(idx + 1);
        gl_state.texture_dirty_flags[idx] = false;
    }
    gl_state.dirty_texture_count = 0;

    for (int i = 0; i < gl_state.dirty_palette_count; i++) {
        const int idx = gl_state.dirty_palette_indices[i];
        SDLGameRendererGL_DestroyPalette(idx + 1);
        SDLGameRendererGL_CreatePalette((idx + 1) << 16);
        gl_state.palette_dirty_flags[idx] = false;
    }
    gl_state.dirty_palette_count = 0;

    const float r = ((flPs2State.FrameClearColor >> 16) & 0xFF) / 255.0f;
    const float g = ((flPs2State.FrameClearColor >> 8) & 0xFF) / 255.0f;
    const float b = (flPs2State.FrameClearColor & 0xFF) / 255.0f;
    // Use transparent alpha when HD stage is active so the background shows through.
    // Otherwise opaque alpha for correct RmlUi pre-multiplied text blending.
    float a = ModdedStage_IsActiveForCurrentStage() ? 0.0f : 1.0f;

    glBindFramebuffer(GL_FRAMEBUFFER, gl_state.cps3_canvas_fbo);
    glViewport(0, 0, 384 * g_resolution_scale, 224 * g_resolution_scale);
    glClearColor(r, g, b, a);
    glClearDepth(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    TRACE_ZONE_END();
}

void SDLGameRendererGL_RenderFrame(void) {
    TRACE_ZONE_N("RenderFrame");
    if (gl_state.render_task_count == 0) {
        TRACE_ZONE_END();
        return;
    }

    stable_sort_render_tasks();

    const float sw = 384.0f * g_resolution_scale;
    const float sh = 224.0f * g_resolution_scale;
    const float projection[4][4] = { { 2.0f / sw, 0.0f, 0.0f, 0.0f },
                                     { 0.0f, -2.0f / sh, 0.0f, 0.0f },
                                     { 0.0f, 0.0f, -1.0f, 0.0f },
                                     { -1.0f, 1.0f, 0.0f, 1.0f } };

    int current_buffer_idx = 0;

    if (gl_state.use_persistent_mapping) {
        gl_state.buffer_index = (gl_state.buffer_index + 1) % OFFSET_BUFFER_COUNT;
        current_buffer_idx = gl_state.buffer_index;

        if (gl_state.fences[current_buffer_idx]) {
            GLenum waitStatus =
                glClientWaitSync(gl_state.fences[current_buffer_idx], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
            if (waitStatus == GL_TIMEOUT_EXPIRED || waitStatus == GL_WAIT_FAILED) {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Fence sync timeout or failure!");
            }
            glDeleteSync(gl_state.fences[current_buffer_idx]);
            gl_state.fences[current_buffer_idx] = 0;
        }

        SDL_Vertex* vbo_ptr = gl_state.persistent_vbo_ptr[current_buffer_idx];
        float* layer_ptr = gl_state.persistent_layer_ptr[current_buffer_idx];
        float* pal_ptr = gl_state.persistent_pal_ptr[current_buffer_idx];
        float* z_ptr = gl_state.persistent_z_ptr[current_buffer_idx];

        // ⚡ Bolt: SIMD broadcast — copy sorted layer/palette with single stores
        for (int i = 0; i < gl_state.render_task_count; i++) {
            const int src = gl_state.render_tasks[i].original_index * 4;
            const int dst = i * 4;
            memcpy(&vbo_ptr[dst], &gl_state.batch_vertices[src], 4 * sizeof(SDL_Vertex));

            simde_mm_storeu_ps(&layer_ptr[dst], simde_mm_set1_ps(gl_state.batch_layers[src]));
            simde_mm_storeu_ps(&pal_ptr[dst], simde_mm_set1_ps(gl_state.batch_pal_indices[src]));
            simde_mm_storeu_ps(&z_ptr[dst], simde_mm_set1_ps(gl_state.batch_z[src]));
        }
    } else {
        current_buffer_idx = 0;
        static SDL_Vertex sorted_vertices[RENDER_TASK_MAX * 4];
        static float sorted_layers[RENDER_TASK_MAX * 4];
        static float sorted_pals[RENDER_TASK_MAX * 4];
        static float sorted_z[RENDER_TASK_MAX * 4];

        // ⚡ Bolt: SIMD broadcast — copy sorted layer/palette with single stores
        for (int i = 0; i < gl_state.render_task_count; i++) {
            const int src = gl_state.render_tasks[i].original_index * 4;
            const int dst = i * 4;
            memcpy(&sorted_vertices[dst], &gl_state.batch_vertices[src], 4 * sizeof(SDL_Vertex));

            simde_mm_storeu_ps(&sorted_layers[dst], simde_mm_set1_ps(gl_state.batch_layers[src]));
            simde_mm_storeu_ps(&sorted_pals[dst], simde_mm_set1_ps(gl_state.batch_pal_indices[src]));
            simde_mm_storeu_ps(&sorted_z[dst], simde_mm_set1_ps(gl_state.batch_z[src]));
        }

        glBindVertexArray(gl_state.persistent_vaos[0]);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(SDL_Vertex), sorted_vertices);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_layer_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(float), sorted_layers);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_pal_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(float), sorted_pals);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_z_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(float), sorted_z);
    }

    glBindVertexArray(gl_state.persistent_vaos[current_buffer_idx]);
    // Common state setup
    glEnable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);

    // ⚡ Bolt: Enable depth testing for HD composition pass occlusion
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS); // Painter's algorithm guarantees last-drawn is front-most

    RendererBlendMode current_applied_blend_mode = -1;

    TRACE_GPU_ZONE("RenderFrame");

    enum ShaderType { SHADER_NONE, SHADER_ARRAY, SHADER_LEGACY };
    enum ShaderType current_shader_type = SHADER_NONE;

    const GLuint arr_shader = SDLApp_GetSceneArrayShaderProgram();
    const GLuint leg_shader = SDLApp_GetSceneShaderProgram();

    int i = 0;
    int stat_array_sprites = 0;
    int stat_legacy_sprites = 0;
    int stat_draw_calls = 0;
    TRACE_SUB_BEGIN("GL:BatchDraw");
    while (i < gl_state.render_task_count) {
        // array_layer >= 0  → R8UI indexed array
        // array_layer <= -2 → RGBA8 direct-color array
        // array_layer == -1 → legacy individual texture (fallback)
        const bool is_array_task = (gl_state.render_tasks[i].array_layer != -1);
        const RendererBlendMode task_blend = gl_state.render_tasks[i].blend_mode;

        if (task_blend != current_applied_blend_mode) {
            if (task_blend == RENDERER_BLEND_ADD) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            } else if (task_blend == RENDERER_BLEND_MULTIPLY) {
                glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            current_applied_blend_mode = task_blend;
        }

        if (is_array_task) {
            if (current_shader_type != SHADER_ARRAY) {
                glUseProgram(arr_shader);
                current_shader_type = SHADER_ARRAY;

                if (gl_state.arr_loc_projection == -1)
                    gl_state.arr_loc_projection = glGetUniformLocation(arr_shader, "projection");
                glUniformMatrix4fv(gl_state.arr_loc_projection, 1, GL_FALSE, (const float*)projection);

                if (gl_state.arr_loc_source == -1)
                    gl_state.arr_loc_source = glGetUniformLocation(arr_shader, "Source");
                glUniform1i(gl_state.arr_loc_source, 0);

                if (gl_state.arr_loc_palette == -1)
                    gl_state.arr_loc_palette = glGetUniformLocation(arr_shader, "PaletteBuffer");
                glUniform1i(gl_state.arr_loc_palette, 1);

                if (gl_state.arr_loc_source_rgba == -1)
                    gl_state.arr_loc_source_rgba = glGetUniformLocation(arr_shader, "SourceRGBA");
                glUniform1i(gl_state.arr_loc_source_rgba, 2);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_id);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, gl_state.palette_tbo);

                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_rgba_id);

                glActiveTexture(GL_TEXTURE0);
            }

            int batch_count = 0;
            int start_index = i;
            while (i < gl_state.render_task_count && gl_state.render_tasks[i].array_layer != -1 &&
                   gl_state.render_tasks[i].blend_mode == current_applied_blend_mode) {
                batch_count++;
                i++;
            }

            stat_array_sprites += batch_count;
            stat_draw_calls++;
            const size_t offset_bytes = (size_t)start_index * 6 * sizeof(int);
            glDrawElements(GL_TRIANGLES, batch_count * 6, GL_UNSIGNED_INT, (void*)offset_bytes);

        } else {
            // ⚡ Bolt: HD Sprites and legacy direct mode skips the native pass.
            // They are deferred to SDLGameRendererGL_RenderHDPass().
            i++;
        }
    }
    TRACE_SUB_END();
    TRACE_PLOT_INT("ArraySprites", stat_array_sprites);
    TRACE_PLOT_INT("LegacySprites", stat_legacy_sprites);
    TRACE_PLOT_INT("DrawCalls", stat_draw_calls);
    TRACE_PLOT_INT("R8UIFree", gl_state.tex_array_free_count);
    TRACE_PLOT_INT("RGBAFree", gl_state.tex_array_rgba_free_count);

    TRACE_GPU_ZONE_END();
    glDisable(GL_DEPTH_TEST);

    if (gl_state.use_persistent_mapping) {
        gl_state.fences[current_buffer_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    TRACE_ZONE_END();
}

void SDLGameRendererGL_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h) {
    if (gl_state.render_task_count == 0)
        return;

    if (pass_index < 0 || pass_index >= g_render_passes.count || g_render_passes.passes[pass_index].skip_this_frame)
        return;

    TRACE_ZONE_N("GL:ExecutePass");

    const float sw = 384.0f * g_resolution_scale;
    const float sh = 224.0f * g_resolution_scale;
    const float projection[4][4] = { { 2.0f / sw, 0.0f, 0.0f, 0.0f },
                                     { 0.0f, -2.0f / sh, 0.0f, 0.0f },
                                     { 0.0f, 0.0f, -1.0f, 0.0f },
                                     { -1.0f, 1.0f, 0.0f, 1.0f } };

    int current_buffer_idx = gl_state.use_persistent_mapping ? gl_state.buffer_index : 0;
    glBindVertexArray(gl_state.persistent_vaos[current_buffer_idx]);

    glEnable(GL_BLEND);

    GLuint hd_shader;

    bool backgrounds_only = (pass_index == 1);

    if (g_render_passes.passes[pass_index].framebuffer) {
        // Assume framebuffer is a GLTransientRenderTarget*
        struct { GLuint fbo; GLuint tex; }* rt = g_render_passes.passes[pass_index].framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        if (g_render_passes.passes[pass_index].clear_color) {
            uint32_t cv = g_render_passes.passes[pass_index].clear_color_value;
            glClearColor(((cv >> 24) & 0xFF)/255.0f, ((cv >> 16) & 0xFF)/255.0f, ((cv >> 8) & 0xFF)/255.0f, (cv & 0xFF)/255.0f);
            glClear(GL_COLOR_BUFFER_BIT | (g_render_passes.passes[pass_index].clear_depth ? GL_DEPTH_BUFFER_BIT : 0));
        }
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, gl_state.cps3_canvas_fbo);
    }

    if (!backgrounds_only) {
        // Bind Native depth map to texture unit 1 to enable fragment exclusion
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cps3_canvas_depth_texture);
        glActiveTexture(GL_TEXTURE0);

        hd_shader = SDLApp_GetHDSceneShaderProgram(); // Utilizes depth discard
    } else {
        glActiveTexture(GL_TEXTURE0);
        hd_shader = SDLApp_GetSceneShaderProgram(); // Standard texturing
    }

    glUseProgram(hd_shader);

    GLint loc_proj = glGetUniformLocation(hd_shader, "projection");
    glUniformMatrix4fv(loc_proj, 1, GL_FALSE, (const float*)projection);

    GLint loc_src = glGetUniformLocation(hd_shader, "Source");
    glUniform1i(loc_src, 0);

    if (!backgrounds_only) {
        GLint loc_depth = glGetUniformLocation(hd_shader, "NativeDepthTex");
        if (loc_depth != -1) {
            glUniform1i(loc_depth, 1);
        }

        GLint loc_res = glGetUniformLocation(hd_shader, "Resolution");
        if (loc_res != -1) {
            glUniform2f(loc_res, (float)viewport_w, (float)viewport_h);
        }
    }

    GLint loc_res = glGetUniformLocation(hd_shader, "Resolution");
    glUniform2f(loc_res, (float)viewport_w, (float)viewport_h);

    RendererBlendMode current_applied_blend_mode = -1;
    int i = 0;

    while (i < gl_state.render_task_count) {
        if (gl_state.render_tasks[i].array_layer != -1) {
            i++;
            continue;
        }

        if (backgrounds_only && gl_state.render_tasks[i].z >= 0.1f) {
            i++;
            continue;
        }
        if (!backgrounds_only && gl_state.render_tasks[i].z < 0.1f) {
            i++;
            continue;
        }

        const RendererBlendMode task_blend = gl_state.render_tasks[i].blend_mode;
        if (task_blend != current_applied_blend_mode) {
            if (task_blend == RENDERER_BLEND_ADD) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glBlendEquation(GL_FUNC_ADD);
            } else if (task_blend == RENDERER_BLEND_MULTIPLY) {
                glBlendFunc(GL_DST_COLOR, GL_ZERO);
                glBlendEquation(GL_FUNC_ADD);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBlendEquation(GL_FUNC_ADD);
            }
            current_applied_blend_mode = task_blend;
        }

        const GLuint current_texture = gl_state.render_tasks[i].texture;
        int batch_count = 0;
        int start_index = i;

        while (i < gl_state.render_task_count && gl_state.render_tasks[i].array_layer == -1 &&
               gl_state.render_tasks[i].texture == current_texture &&
               gl_state.render_tasks[i].blend_mode == current_applied_blend_mode) {

            // Re-validate against splitting planes within the internal batch loop
            if (backgrounds_only && gl_state.render_tasks[i].z >= 0.1f)
                break;
            if (!backgrounds_only && gl_state.render_tasks[i].z < 0.1f)
                break;

            batch_count++;
            i++;
        }

        glBindTexture(GL_TEXTURE_2D, current_texture);
        const size_t offset_bytes = (size_t)start_index * 6 * sizeof(int);
        glDrawElements(GL_TRIANGLES, batch_count * 6, GL_UNSIGNED_INT, (void*)offset_bytes);
    }

    // Unbind Depth
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    TRACE_ZONE_END();
}

void SDLGameRendererGL_EndFrame(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // destroy_textures(); // Handled in BeginFrame now, or explicit call?
    // Original code called destroy_textures() in EndFrame.
    // The "Resources" file manages the textures list, but destroy_textures() is internal there.
    // However, dirty cleanup is in BeginFrame.
    // Wait, original EndFrame called destroy_textures() which processes glDeleteTextures.
    // If I moved destroy logic to BeginFrame in my split, I should call it there.
    // In my Resources split, I didn't expose destroy_textures().
    // But I exposed push_texture_to_destroy.
    // Actually, in SDLGameRendererGL_BeginFrame above, I implemented the loops to process dirty flags.
    // BUT, destroy_textures() in the original code also processed the "textures_to_destroy" list.
    // I need to process that list here or in BeginFrame.

    // In the resources file, I should probably expose a "ProcessDestructionQueue" function.
    // Or just do it in BeginFrame like I did for dirty flags?
    // Let's add the processing logic here directly since I have access to gl_state.

    gl_state.texture_count = 0;

    if (gl_state.textures_to_destroy_count > 0) {
        glDeleteTextures(gl_state.textures_to_destroy_count, gl_state.textures_to_destroy);
        gl_state.textures_to_destroy_count = 0;
    }

    clear_render_tasks();
}

// --- Draw Quad Helpers ---

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    SDL_Vertex sdl_vertices[4];

    const Uint32 src = vertices[0].color;
    const Uint32 swizzled = ((src & 0xFF) << 16) | (src & 0xFF00FF00) | ((src >> 16) & 0xFF);

    const float scale = (float)g_resolution_scale;

    if (textured) {
        for (int i = 0; i < 4; i++) {
            sdl_vertices[i].position.x = vertices[i].coord.x * scale;
            sdl_vertices[i].position.y = vertices[i].coord.y * scale;
            memcpy(&sdl_vertices[i].color, &swizzled, sizeof(Uint32));
            sdl_vertices[i].tex_coord.x = vertices[i].tex_coord.s;
            sdl_vertices[i].tex_coord.y = vertices[i].tex_coord.t;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            sdl_vertices[i].position.x = vertices[i].coord.x * scale;
            sdl_vertices[i].position.y = vertices[i].coord.y * scale;
            memcpy(&sdl_vertices[i].color, &swizzled, sizeof(Uint32));
            sdl_vertices[i].tex_coord.x = 0.0f;
            sdl_vertices[i].tex_coord.y = 0.0f;
        }
    }

    GLuint tex;
    int array_layer;
    int pal_slot;

    if (textured) {
        if (gl_state.texture_count == 0)
            fatal_error("No textures to get");
        tex = gl_state.textures[gl_state.texture_count - 1];

        // Manual "get_texture_layer" and "get_texture_pal_slot"
        if (gl_state.texture_count > 0) {
            array_layer = gl_state.texture_layers[gl_state.texture_count - 1];
            pal_slot = gl_state.texture_pal_slots[gl_state.texture_count - 1];
        } else {
            array_layer = -1;
            pal_slot = 0;
        }

        if (array_layer != -1 && gl_state.texture_count > 0) {
            const float sx = gl_state.texture_uv_sx[gl_state.texture_count - 1];
            const float sy = gl_state.texture_uv_sy[gl_state.texture_count - 1];
            for (int i = 0; i < 4; i++) {
                sdl_vertices[i].tex_coord.x *= sx;
                sdl_vertices[i].tex_coord.y *= sy;
            }
        }
    } else {
        tex = 0;
        array_layer = -(gl_state.white_array_layer + 2); // RGBA array encoding
        pal_slot = 0;
    }
    push_render_task(tex, sdl_vertices, flPS2ConvScreenFZ(vertices[0].coord.z), array_layer, pal_slot);
}

void SDLGameRendererGL_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    for (int i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].color = color;
        vertices[i].tex_coord = sprite->t[i];
    }
    draw_quad(vertices, true);
}

void SDLGameRendererGL_DrawSolidQuad(const Quad* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    for (s32 i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].color = color;
    }
    draw_quad(vertices, false);
}

void SDLGameRendererGL_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = sprite->v[0].z;
        vertices[i].color = color;
    }
    vertices[0].coord.x = sprite->v[0].x;
    vertices[0].coord.y = sprite->v[0].y;
    vertices[3].coord.x = sprite->v[3].x;
    vertices[3].coord.y = sprite->v[3].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite->t[0];
    vertices[3].tex_coord = sprite->t[3];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}

void SDLGameRendererGL_DrawSprite2(const Sprite2* sprite2) {
    SDLGameRenderer_Vertex vertices[4];
    const float z = sprite2->v[0].z;
    const unsigned int color = sprite2->vertex_color;

    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = z;
        vertices[i].color = color;
    }
    vertices[0].coord.x = sprite2->v[0].x;
    vertices[0].coord.y = sprite2->v[0].y;
    vertices[3].coord.x = sprite2->v[1].x;
    vertices[3].coord.y = sprite2->v[1].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite2->t[0];
    vertices[3].tex_coord = sprite2->t[1];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}

/**
 * @brief ⚡ Inlined GL batch sprite flush — writes directly to batch buffers.
 *
 * Eliminates the 3-deep call chain (DrawSprite2 → draw_quad → push_render_task)
 * and 2 intermediate vertex arrays per sprite. Caches texture/layer/palette state
 * on tex_code change and performs color swizzle + UV scale + Z conversion inline.
 *
 * GL backend preserves submission order for correct Z via stable_sort_render_tasks.
 */
void SDLGameRendererGL_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    // ⚡ Bolt: Batch stale-promotion — single O(pending + live) pass before draw
    SDLGameRendererGL_FlushPendingUnlocks();

    unsigned int last_tex_code = 0;

    // Cached texture state — refreshed on tex_code change
    int cur_layer = -1;
    int cur_pal = 0;
    float cur_uv_sx = 1.0f;
    float cur_uv_sy = 1.0f;
    GLuint cur_texture = 0;

    for (int i = 0; i < count; i++) {
        if (!active_layers[chips[i].id])
            continue;

        if (gl_state.render_task_count >= RENDER_TASK_MAX)
            break;

        const Sprite2* spr = &chips[i];

        // --- tex_code change: call SetTexture, then cache the result ---
        const unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            flSetRenderState(FLRENDER_TEXSTAGE0, tc);

            // Cache the texture stack top for all subsequent sprites with same tex_code
            if (gl_state.texture_count > 0) {
                const int top = gl_state.texture_count - 1;
                cur_texture = gl_state.textures[top];
                cur_layer = gl_state.texture_layers[top];
                cur_pal = gl_state.texture_pal_slots[top];
                cur_uv_sx = gl_state.texture_uv_sx[top];
                cur_uv_sy = gl_state.texture_uv_sy[top];
            }
        }
        // Same tex_code: reuse cached cur_texture/cur_layer/cur_pal/cur_uv — no stack push needed

        // --- Inline vertex construction (was: DrawSprite2 → draw_quad → push_render_task) ---
        const int task_idx = gl_state.render_task_count;
        const int vo = task_idx * 4; // vertex offset

        // Color swizzle: Sprite2 stores BGRA, GL shader expects RGBA byte order
        const Uint32 src_color = spr->vertex_color;
        const Uint32 color = ((src_color & 0xFF) << 16) | (src_color & 0xFF00FF00u) | ((src_color >> 16) & 0xFF);

        // Positions: v[0] = top-left, v[1] = bottom-right; expand to 4 corners
        const float scale = (float)g_resolution_scale;
        const float x0 = spr->v[0].x * scale;
        const float y0 = spr->v[0].y * scale;
        const float x1 = spr->v[1].x * scale;
        const float y1 = spr->v[1].y * scale;

        // UVs: t[0] = top-left, t[1] = bottom-right; expand to 4 corners
        float s0 = spr->t[0].s;
        float t0 = spr->t[0].t;
        float s1 = spr->t[1].s;
        float t1 = spr->t[1].t;

        // Apply array-texture UV scale (both indexed >= 0 and RGBA <= -2)
        if (cur_layer != -1) {
            s0 *= cur_uv_sx;
            t0 *= cur_uv_sy;
            s1 *= cur_uv_sx;
            t1 *= cur_uv_sy;
        }

        // Z depth conversion
        const float z = flPS2ConvScreenFZ(spr->v[0].z);

        // Write 4 vertices directly to batch buffer
        SDL_Vertex* v = &gl_state.batch_vertices[vo];

        v[0].position.x = x0;
        v[0].position.y = y0;
        v[0].tex_coord.x = s0;
        v[0].tex_coord.y = t0;
        memcpy(&v[0].color, &color, sizeof(Uint32));

        v[1].position.x = x1;
        v[1].position.y = y0;
        v[1].tex_coord.x = s1;
        v[1].tex_coord.y = t0;
        memcpy(&v[1].color, &color, sizeof(Uint32));

        v[2].position.x = x0;
        v[2].position.y = y1;
        v[2].tex_coord.x = s0;
        v[2].tex_coord.y = t1;
        memcpy(&v[2].color, &color, sizeof(Uint32));

        v[3].position.x = x1;
        v[3].position.y = y1;
        v[3].tex_coord.x = s1;
        v[3].tex_coord.y = t1;
        memcpy(&v[3].color, &color, sizeof(Uint32));

        // Write layer + palette + Z with SIMD broadcast
        simde_mm_storeu_ps(&gl_state.batch_layers[vo], simde_mm_set1_ps((float)cur_layer));
        simde_mm_storeu_ps(&gl_state.batch_pal_indices[vo], simde_mm_set1_ps((float)cur_pal));
        simde_mm_storeu_ps(&gl_state.batch_z[vo], simde_mm_set1_ps(z));

        // Fill render task metadata
        RenderTask* task = &gl_state.render_tasks[task_idx];
        task->texture = cur_texture;
        task->vertex_offset = vo;
        task->z = z;
        task->original_index = task_idx;
        task->index = task_idx;
        task->array_layer = cur_layer;
        task->palette_slot = cur_pal;
        task->blend_mode = gl_state.current_blend_mode;

        if (z < gl_state.last_submitted_z) {
            gl_state.sort_inversions++;
        }
        gl_state.last_submitted_z = z;

        gl_state.render_task_count++;
    }
}

/**
 * @brief Push a raw GL RGBA texture into the batch renderer as a legacy sprite.
 *
 * Called by TextureUtil_DrawQuad to draw ControllerImage glyph textures.
 * The texture participates in the normal z-sorted batch render (array_layer=-1
 * legacy path) so it integrates correctly with CPS3 sprites.
 *
 * @param gl_texture_id  Raw GL texture ID (from glGenTextures / TextureUtil).
 * @param x, y           Top-left position in CPS3 canvas pixels (384×224).
 * @param w, h           Width and height in CPS3 canvas pixels.
 * @param z              Z-depth from flPS2ConvScreenFZ() or equivalent.
 */
void SDLGameRendererGL_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z) {
    const GLuint gl_texture_id = (GLuint)(uintptr_t)texture;
    const Uint32 white = 0xFFFFFFFF;
    SDL_Vertex sdl_vertices[4];
    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    /* Top-left */
    sdl_vertices[0].position.x = sx;
    sdl_vertices[0].position.y = sy;
    sdl_vertices[0].tex_coord.x = 0.0f;
    sdl_vertices[0].tex_coord.y = 0.0f;
    memcpy(&sdl_vertices[0].color, &white, sizeof(Uint32));

    /* Top-right */
    sdl_vertices[1].position.x = sx + sw;
    sdl_vertices[1].position.y = sy;
    sdl_vertices[1].tex_coord.x = 1.0f;
    sdl_vertices[1].tex_coord.y = 0.0f;
    memcpy(&sdl_vertices[1].color, &white, sizeof(Uint32));

    /* Bottom-left */
    sdl_vertices[2].position.x = sx;
    sdl_vertices[2].position.y = sy + sh;
    sdl_vertices[2].tex_coord.x = 0.0f;
    sdl_vertices[2].tex_coord.y = 1.0f;
    memcpy(&sdl_vertices[2].color, &white, sizeof(Uint32));

    /* Bottom-right */
    sdl_vertices[3].position.x = sx + sw;
    sdl_vertices[3].position.y = sy + sh;
    sdl_vertices[3].tex_coord.x = 1.0f;
    sdl_vertices[3].tex_coord.y = 1.0f;
    memcpy(&sdl_vertices[3].color, &white, sizeof(Uint32));

    /* Push as a legacy texture (array_layer = -1, pal_slot = 0) */
    push_render_task(gl_texture_id, sdl_vertices, z, -1, 0);
}

void SDLGameRendererGL_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                         int flip_y) {
    const GLuint gl_texture_id = (GLuint)(uintptr_t)texture;
    const Uint32 white = 0xFFFFFFFF;
    SDL_Vertex sdl_vertices[4];
    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    float min_u = flip_x ? 1.0f : 0.0f;
    float max_u = flip_x ? 0.0f : 1.0f;
    float min_v = flip_y ? 1.0f : 0.0f;
    float max_v = flip_y ? 0.0f : 1.0f;

    /* Top-left */
    sdl_vertices[0].position.x = sx;
    sdl_vertices[0].position.y = sy;
    sdl_vertices[0].tex_coord.x = min_u;
    sdl_vertices[0].tex_coord.y = min_v;
    memcpy(&sdl_vertices[0].color, &white, sizeof(Uint32));

    /* Top-right */
    sdl_vertices[1].position.x = sx + sw;
    sdl_vertices[1].position.y = sy;
    sdl_vertices[1].tex_coord.x = max_u;
    sdl_vertices[1].tex_coord.y = min_v;
    memcpy(&sdl_vertices[1].color, &white, sizeof(Uint32));

    /* Bottom-left */
    sdl_vertices[2].position.x = sx;
    sdl_vertices[2].position.y = sy + sh;
    sdl_vertices[2].tex_coord.x = min_u;
    sdl_vertices[2].tex_coord.y = max_v;
    memcpy(&sdl_vertices[2].color, &white, sizeof(Uint32));

    /* Bottom-right */
    sdl_vertices[3].position.x = sx + sw;
    sdl_vertices[3].position.y = sy + sh;
    sdl_vertices[3].tex_coord.x = max_u;
    sdl_vertices[3].tex_coord.y = max_v;
    memcpy(&sdl_vertices[3].color, &white, sizeof(Uint32));

    /* Push as a legacy texture (array_layer = -1, pal_slot = 0) */
    push_render_task(gl_texture_id, sdl_vertices, z, -1, 0);
}

void SDLGameRendererGL_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0, float v0,
                                            float u1, float v1, float z) {
    const GLuint gl_texture_id = (GLuint)(uintptr_t)texture;
    const Uint32 white = 0xFFFFFFFF;
    SDL_Vertex sdl_vertices[4];
    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    /* Top-left */
    sdl_vertices[0].position.x = sx;
    sdl_vertices[0].position.y = sy;
    sdl_vertices[0].tex_coord.x = u0;
    sdl_vertices[0].tex_coord.y = v0;
    memcpy(&sdl_vertices[0].color, &white, sizeof(Uint32));

    /* Top-right */
    sdl_vertices[1].position.x = sx + sw;
    sdl_vertices[1].position.y = sy;
    sdl_vertices[1].tex_coord.x = u1;
    sdl_vertices[1].tex_coord.y = v0;
    memcpy(&sdl_vertices[1].color, &white, sizeof(Uint32));

    /* Bottom-left */
    sdl_vertices[2].position.x = sx;
    sdl_vertices[2].position.y = sy + sh;
    sdl_vertices[2].tex_coord.x = u0;
    sdl_vertices[2].tex_coord.y = v1;
    memcpy(&sdl_vertices[2].color, &white, sizeof(Uint32));

    /* Bottom-right */
    sdl_vertices[3].position.x = sx + sw;
    sdl_vertices[3].position.y = sy + sh;
    sdl_vertices[3].tex_coord.x = u1;
    sdl_vertices[3].tex_coord.y = v1;
    memcpy(&sdl_vertices[3].color, &white, sizeof(Uint32));

    /* Push as a legacy texture (array_layer = -1, pal_slot = 0) */
    push_render_task(gl_texture_id, sdl_vertices, z, -1, 0);
}

