/**
 * @file sdl_game_renderer_gl_draw.c
 * @brief OpenGL renderer draw commands and frame lifecycle.
 *
 * Implements batched sprite/quad rendering, render task management,
 * frame begin/end, and the OpenGL draw pipeline including shader pass
 * application via librashader. Part of the GL rendering backend.
 */
#include "port/modded_stage.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_app_config.h"
#include "port/sdl/sdl_game_renderer_gl_internal.h"
#include "port/tracy_gpu.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include <stddef.h>
#include <string.h>

// --- Render Task Management ---

static void push_render_task(GLuint texture, const SDL_Vertex* vertices, float z, int array_layer, int pal_slot) {
    if (gl_state.render_task_count >= RENDER_TASK_MAX) {
        SDL_Log("Warning: render task buffer full, skipping task");
        return;
    }

    const int vertex_offset = gl_state.render_task_count * 4;
    memcpy(&gl_state.batch_vertices[vertex_offset], vertices, 4 * sizeof(SDL_Vertex));

    const float layer_f = (float)array_layer;
    gl_state.batch_layers[vertex_offset + 0] = layer_f;
    gl_state.batch_layers[vertex_offset + 1] = layer_f;
    gl_state.batch_layers[vertex_offset + 2] = layer_f;
    gl_state.batch_layers[vertex_offset + 3] = layer_f;

    const float pal_f = (float)pal_slot;
    gl_state.batch_pal_indices[vertex_offset + 0] = pal_f;
    gl_state.batch_pal_indices[vertex_offset + 1] = pal_f;
    gl_state.batch_pal_indices[vertex_offset + 2] = pal_f;
    gl_state.batch_pal_indices[vertex_offset + 3] = pal_f;

    RenderTask* task = &gl_state.render_tasks[gl_state.render_task_count];
    task->texture = texture;
    task->vertex_offset = vertex_offset;
    task->z = z;
    task->original_index = gl_state.render_task_count;
    task->index = gl_state.render_task_count;
    task->array_layer = array_layer;
    task->palette_slot = pal_slot;

    gl_state.render_task_count++;
}

static void clear_render_tasks(void) {
    gl_state.render_task_count = 0;
}

static void stable_sort_render_tasks(void) {
    const int n = gl_state.render_task_count;
    if (n <= 1)
        return;

    for (int width = 1; width < n; width *= 2) {
        for (int left = 0; left < n; left += 2 * width) {
            const int mid = left + width;
            int right = left + 2 * width;
            if (mid >= n)
                break;
            if (right > n)
                right = n;

            int i = left, j = mid, k = 0;
            while (i < mid && j < right) {
                if (gl_state.render_tasks[i].z <= gl_state.render_tasks[j].z) {
                    gl_state.merge_temp[k++] = gl_state.render_tasks[i++];
                } else {
                    gl_state.merge_temp[k++] = gl_state.render_tasks[j++];
                }
            }
            while (i < mid)
                gl_state.merge_temp[k++] = gl_state.render_tasks[i++];
            while (j < right)
                gl_state.merge_temp[k++] = gl_state.render_tasks[j++];

            memcpy(&gl_state.render_tasks[left], gl_state.merge_temp, (size_t)k * sizeof(RenderTask));
        }
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
    float a = (flPs2State.FrameClearColor >> 24) / 255.0f;

    if (ModdedStage_IsActiveForCurrentStage()) {
        a = 0.0f;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, gl_state.cps3_canvas_fbo);
    glViewport(0, 0, 384, 224);
    glClearColor(r, g, b, a);

    glClear(GL_COLOR_BUFFER_BIT);
    TRACE_ZONE_END();
}

void SDLGameRendererGL_RenderFrame(void) {
    TRACE_ZONE_N("RenderFrame");
    if (gl_state.render_task_count == 0) {
        TRACE_ZONE_END();
        return;
    }

    TRACE_SUB_BEGIN("GL:Sort");
    stable_sort_render_tasks();
    TRACE_SUB_END();

    static const float projection[4][4] = { { 2.0f / 384.0f, 0.0f, 0.0f, 0.0f },
                                            { 0.0f, -2.0f / 224.0f, 0.0f, 0.0f },
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

        for (int i = 0; i < gl_state.render_task_count; i++) {
            const int src = gl_state.render_tasks[i].original_index * 4;
            const int dst = i * 4;
            memcpy(&vbo_ptr[dst], &gl_state.batch_vertices[src], 4 * sizeof(SDL_Vertex));

            float lay = gl_state.batch_layers[src];
            layer_ptr[dst + 0] = lay;
            layer_ptr[dst + 1] = lay;
            layer_ptr[dst + 2] = lay;
            layer_ptr[dst + 3] = lay;

            float pal = gl_state.batch_pal_indices[src];
            pal_ptr[dst + 0] = pal;
            pal_ptr[dst + 1] = pal;
            pal_ptr[dst + 2] = pal;
            pal_ptr[dst + 3] = pal;
        }
    } else {
        current_buffer_idx = 0;
        static SDL_Vertex sorted_vertices[RENDER_TASK_MAX * 4];
        static float sorted_layers[RENDER_TASK_MAX * 4];
        static float sorted_pals[RENDER_TASK_MAX * 4];

        for (int i = 0; i < gl_state.render_task_count; i++) {
            const int src = gl_state.render_tasks[i].original_index * 4;
            const int dst = i * 4;
            memcpy(&sorted_vertices[dst], &gl_state.batch_vertices[src], 4 * sizeof(SDL_Vertex));

            float lay = gl_state.batch_layers[src];
            sorted_layers[dst + 0] = lay;
            sorted_layers[dst + 1] = lay;
            sorted_layers[dst + 2] = lay;
            sorted_layers[dst + 3] = lay;

            float pal = gl_state.batch_pal_indices[src];
            sorted_pals[dst + 0] = pal;
            sorted_pals[dst + 1] = pal;
            sorted_pals[dst + 2] = pal;
            sorted_pals[dst + 3] = pal;
        }

        glBindVertexArray(gl_state.persistent_vaos[0]);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(SDL_Vertex), sorted_vertices);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_layer_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(float), sorted_layers);

        glBindBuffer(GL_ARRAY_BUFFER, gl_state.persistent_pal_vbos[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gl_state.render_task_count * 4 * sizeof(float), sorted_pals);
    }

    glBindVertexArray(gl_state.persistent_vaos[current_buffer_idx]);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);

    TRACE_GPU_ZONE("RenderFrame");

    enum ShaderType { SHADER_NONE, SHADER_ARRAY, SHADER_LEGACY };
    enum ShaderType current_shader_type = SHADER_NONE;

    const GLuint arr_shader = SDLApp_GetSceneArrayShaderProgram();
    const GLuint leg_shader = SDLApp_GetSceneShaderProgram();

    TRACE_SUB_BEGIN("GL:BatchDraw");
    int i = 0;
    while (i < gl_state.render_task_count) {
        const bool is_array_task = (gl_state.render_tasks[i].array_layer >= 0);

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

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, gl_state.tex_array_id);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_BUFFER, gl_state.palette_tbo);
                glActiveTexture(GL_TEXTURE0);
            }

            int batch_count = 0;
            int start_index = i;
            while (i < gl_state.render_task_count && gl_state.render_tasks[i].array_layer >= 0) {
                batch_count++;
                i++;
            }

            const size_t offset_bytes = (size_t)start_index * 6 * sizeof(int);
            glDrawElements(GL_TRIANGLES, batch_count * 6, GL_UNSIGNED_INT, (void*)offset_bytes);

        } else {
            if (current_shader_type != SHADER_LEGACY) {
                glUseProgram(leg_shader);
                current_shader_type = SHADER_LEGACY;

                if (gl_state.loc_projection == -1)
                    gl_state.loc_projection = glGetUniformLocation(leg_shader, "projection");
                glUniformMatrix4fv(gl_state.loc_projection, 1, GL_FALSE, (const float*)projection);

                if (gl_state.loc_source == -1)
                    gl_state.loc_source = glGetUniformLocation(leg_shader, "Source");
                glUniform1i(gl_state.loc_source, 0);
            }

            while (i < gl_state.render_task_count && gl_state.render_tasks[i].array_layer < 0) {
                const GLuint current_texture = gl_state.render_tasks[i].texture;
                int batch_count = 0;
                int start_index = i;

                while (i < gl_state.render_task_count && gl_state.render_tasks[i].array_layer < 0 &&
                       gl_state.render_tasks[i].texture == current_texture) {
                    batch_count++;
                    i++;
                }

                glBindTexture(GL_TEXTURE_2D, current_texture);
                const size_t offset_bytes = (size_t)start_index * 6 * sizeof(int);
                glDrawElements(GL_TRIANGLES, batch_count * 6, GL_UNSIGNED_INT, (void*)offset_bytes);
            }
        }
    }
    TRACE_SUB_END();

    TRACE_GPU_ZONE_END();

    if (gl_state.use_persistent_mapping) {
        gl_state.fences[current_buffer_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

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

/// Lightweight reset for netplay sub-frames: clears the texture stack and
/// render tasks without unbinding the framebuffer or deleting textures.
void SDLGameRendererGL_ResetBatchState(void) {
    gl_state.texture_count = 0;
    gl_state.last_set_texture_th = 0;
    clear_render_tasks();
}

// --- Draw Quad Helpers ---

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    SDL_Vertex sdl_vertices[4];

    const Uint32 src = vertices[0].color;
    const Uint32 swizzled = ((src & 0xFF) << 16) | (src & 0xFF00FF00) | ((src >> 16) & 0xFF);

    if (textured) {
        for (int i = 0; i < 4; i++) {
            sdl_vertices[i].position.x = vertices[i].coord.x;
            sdl_vertices[i].position.y = vertices[i].coord.y;
            memcpy(&sdl_vertices[i].color, &swizzled, sizeof(Uint32));
            sdl_vertices[i].tex_coord.x = vertices[i].tex_coord.s;
            sdl_vertices[i].tex_coord.y = vertices[i].tex_coord.t;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            sdl_vertices[i].position.x = vertices[i].coord.x;
            sdl_vertices[i].position.y = vertices[i].coord.y;
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

        if (array_layer >= 0 && gl_state.texture_count > 0) {
            const float sx = gl_state.texture_uv_sx[gl_state.texture_count - 1];
            const float sy = gl_state.texture_uv_sy[gl_state.texture_count - 1];
            for (int i = 0; i < 4; i++) {
                sdl_vertices[i].tex_coord.x *= sx;
                sdl_vertices[i].tex_coord.y *= sy;
            }
        }
    } else {
        tex = gl_state.white_texture;
        array_layer = -1;
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
