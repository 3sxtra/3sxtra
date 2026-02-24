/**
 * @file modded_stage.c
 * @brief HD multi-layer parallax stage background replacement.
 *
 * Loads PNG layers from assets/stages/stage_XX/ and renders them at native
 * screen resolution into the default framebuffer (backbuffer). The game's
 * 384×224 canvas FBO is then composited on top with blending, so sprites
 * appear over the HD background without any downscaling.
 *
 * All scroll/positioning data is read directly from the live bg_w engine
 * struct — this system owns zero gameplay state and is purely cosmetic.
 */
#include "port/modded_stage.h"
#include "port/paths.h"
#include "port/sdl/sdl_texture_util.h"
#include "port/stage_config.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Total number of stages the engine supports */
#define MODDED_STAGE_COUNT 22

/* Number of BGW entries in the engine's BG struct (bgw[7]) */
#define BGW_ARRAY_SIZE 7

typedef struct {
    void* texture; /* GL texture ID */
    int width;
    int height;
} ModdedLayerResources;

/* Module state */
static bool s_enabled = false;
static bool s_rendering_disabled = false;
static bool s_animations_disabled = false;
static int s_loaded_stage = -1;
static ModdedLayerResources s_layer_res[MAX_STAGE_LAYERS];
static int s_layer_res_count = 0;

/* Simple passthru shader for textured quad rendering */
static GLuint s_shader_program = 0;
static GLuint s_quad_vao = 0;
static GLuint s_quad_vbo = 0;
static GLint s_loc_projection = -1;
static GLint s_loc_texture = -1;

/* ---------- Shader Setup ---------- */

static const char* s_vert_src = "#version 330 core\n"
                                "layout(location = 0) in vec2 aPos;\n"
                                "layout(location = 1) in vec2 aUV;\n"
                                "uniform mat4 projection;\n"
                                "out vec2 vUV;\n"
                                "void main() {\n"
                                "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
                                "    vUV = aUV;\n"
                                "}\n";

static const char* s_frag_src = "#version 330 core\n"
                                "in vec2 vUV;\n"
                                "uniform sampler2D tex;\n"
                                "out vec4 FragColor;\n"
                                "void main() {\n"
                                "    FragColor = texture(tex, vUV);\n"
                                "}\n";

static void init_shader(void) {
    if (s_shader_program != 0)
        return;

    GLint success;
    char info_log[512];

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &s_vert_src, NULL);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert, sizeof(info_log), NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "ModdedStage: vertex shader compile failed: %s", info_log);
        glDeleteShader(vert);
        return;
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &s_frag_src, NULL);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, sizeof(info_log), NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "ModdedStage: fragment shader compile failed: %s", info_log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return;
    }

    s_shader_program = glCreateProgram();
    glAttachShader(s_shader_program, vert);
    glAttachShader(s_shader_program, frag);
    glLinkProgram(s_shader_program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    glGetProgramiv(s_shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(s_shader_program, sizeof(info_log), NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "ModdedStage: shader link failed: %s", info_log);
        glDeleteProgram(s_shader_program);
        s_shader_program = 0;
        return;
    }

    s_loc_projection = glGetUniformLocation(s_shader_program, "projection");
    s_loc_texture = glGetUniformLocation(s_shader_program, "tex");

    /* Create reusable VAO/VBO for quad rendering */
    glGenVertexArrays(1, &s_quad_vao);
    glGenBuffers(1, &s_quad_vbo);

    glBindVertexArray(s_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
    /* Allocate space for 4 vertices × 4 floats (pos.x, pos.y, uv.u, uv.v) */
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

/* ---------- Lifecycle ---------- */

void ModdedStage_Init(void) {
    s_enabled = false;
    s_rendering_disabled = false;
    s_animations_disabled = false;
    s_loaded_stage = -1;
    s_layer_res_count = 0;
    memset(s_layer_res, 0, sizeof(s_layer_res));
    StageConfig_Init();
}

void ModdedStage_Shutdown(void) {
    ModdedStage_Unload();
    if (s_shader_program) {
        glDeleteProgram(s_shader_program);
        s_shader_program = 0;
    }
    if (s_quad_vao) {
        glDeleteVertexArrays(1, &s_quad_vao);
        s_quad_vao = 0;
    }
    if (s_quad_vbo) {
        glDeleteBuffers(1, &s_quad_vbo);
        s_quad_vbo = 0;
    }
}

void ModdedStage_SetEnabled(bool enabled) {
    s_enabled = enabled;
}

bool ModdedStage_IsEnabled(void) {
    return s_enabled;
}

void ModdedStage_SetDisableRendering(bool disabled) {
    s_rendering_disabled = disabled;
}

bool ModdedStage_IsRenderingDisabled(void) {
    return s_rendering_disabled;
}

void ModdedStage_SetAnimationsDisabled(bool disabled) {
    s_animations_disabled = disabled;
    if (disabled) {
        effect_work_quick_init();
    }
}

bool ModdedStage_IsAnimationsDisabled(void) {
    return s_animations_disabled;
}

/* ---------- Asset Loading ---------- */

void ModdedStage_LoadForStage(int stage_index) {
    /* Don't reload if already loaded for this stage */
    if (s_loaded_stage == stage_index && s_layer_res_count > 0) {
        return;
    }

    ModdedStage_Unload();

    if (stage_index < 0 || stage_index >= MODDED_STAGE_COUNT) {
        return;
    }

    /* Load Configuration */
    StageConfig_Load(stage_index);

    const char* base = Paths_GetBasePath();
    if (!base)
        base = "";

    char path[512];
    int loaded = 0;

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        StageLayerConfig* cfg = &g_stage_config.layers[i];
        if (!cfg->enabled)
            continue;

        snprintf(path, sizeof(path), "%sassets/stages/stage_%02d/%s", base, stage_index, cfg->filename);

        void* tex = TextureUtil_Load(path);
        if (tex == NULL) {
            /* If it's a critical layer (0), maybe warn? But config might specify empty layers. */
            SDL_LogDebug(SDL_LOG_CATEGORY_RENDER, "ModdedStage: Failed to load %s", path);
            continue;
        }

        s_layer_res[i].texture = tex;
        TextureUtil_GetSize(tex, &s_layer_res[i].width, &s_layer_res[i].height);
        loaded++;

        if (i >= s_layer_res_count)
            s_layer_res_count = i + 1;
    }

    if (loaded > 0) {
        s_loaded_stage = stage_index;
        SDL_Log("ModdedStage: Stage %d loaded with %d active layers", stage_index, loaded);
    }
}

void ModdedStage_Unload(void) {
    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        if (s_layer_res[i].texture) {
            TextureUtil_Free(s_layer_res[i].texture);
            s_layer_res[i].texture = NULL;
        }
    }
    s_layer_res_count = 0;
    s_loaded_stage = -1;
}

/* ---------- Query ---------- */

bool ModdedStage_IsActiveForCurrentStage(void) {
    return s_enabled && s_layer_res_count > 0 && s_loaded_stage == bg_w.stage;
}

int ModdedStage_GetLayerCount(void) {
    return s_layer_res_count; // Approximation
}

int ModdedStage_GetLoadedStageIndex(void) {
    return s_loaded_stage;
}

/* ---------- Rendering ---------- */

#if 0
static float fmod_wrap(float val, float max) {
    val = fmodf(val, max);
    if (val < 0.0f) val += max;
    return val;
}
#endif

static void draw_layer(int layer_index, const BackgroundParameters* bg_prm) {
    if (layer_index < 0 || layer_index >= MAX_STAGE_LAYERS)
        return;

    StageLayerConfig* cfg = &g_stage_config.layers[layer_index];
    if (!cfg->enabled)
        return;

    ModdedLayerResources* res = &s_layer_res[layer_index];
    if (!res->texture)
        return;

    float tex_w = (float)res->width;
    float tex_h = (float)res->height;

    /* Calculate scale */
    float effective_w, effective_h;

    if (cfg->scale_mode == SCALE_MODE_MANUAL) {
        effective_w = tex_w * cfg->scale_factor_x;
        effective_h = tex_h * cfg->scale_factor_y;
    } else if (cfg->scale_mode == SCALE_MODE_NATIVE) {
        effective_w = tex_w;
        effective_h = tex_h;
    } else if (cfg->scale_mode == SCALE_MODE_STRETCH) {
        // Stretch to fill viewport (simplistic default)
        effective_w = 384.0f;
        effective_h = 224.0f;
    } else {
        // FIT_HEIGHT (Default)
        float scale = tex_h / 512.0f;
        if (scale < 0.001f)
            scale = 1.0f;
        effective_w = tex_w / scale;
        effective_h = 512.0f;
    }

    /* Viewport size in game pixels */
    const float vp_w = 384.0f;
    const float vp_h = 224.0f;

    /* Get native scroll position (absolute camera pos) */
    // Note: 'bg->bgw[0].wxy' isn't always reliable directly for all layers?
    // Using bg_prm shift values gives us the "engine computed" shift for a standard layer.
    // To support CUSTOM parallax, we need the *delta* or the *absolute camera*.
    // bg_prm[0] usually tracks the main camera.

    int bg_idx = cfg->original_bg_index;
    float base_x = 0.0f;
    float base_y = 0.0f;

    // Look up referencing original layer speed
    if (bg_idx >= 0 && bg_idx < 8) {
        base_x = (float)(s16)bg_prm[bg_idx].bg_h_shift;
        base_y = (float)(s16)bg_prm[bg_idx].bg_v_shift;
    } else {
        // Fallback: If no original layer assigned, assume static (0.0) base
        // or potentially specific behavior. For now, 0.0 makes sense for strict manual control.
        // If user wants legacy behavior (track layer 0), they should set index 0.
        // Defaults are handled in StageConfig_Load.
        base_x = 0.0f;
        base_y = 0.0f;
    }

    // Config parallax application
    // If standard mode, base_x is 1.0x native speed.
    // We want parallax_x * base_x.

    float scroll_x = base_x * cfg->parallax_x + cfg->offset_x;
    float scroll_y = base_y * cfg->parallax_y + cfg->offset_y;

    /*
     * Wrap scroll position if looping is enabled.
     * The texture coordinate logic essentially handles this,
     * but we might want to clamp 'scroll_x' if loop_x is false.
     *
     * However, the simplest way is to just let UVs march on.
     * CLAMP_TO_EDGE on the texture + shader sampling does the work for non-looping?
     * No, if UV > 1.0 and we clamp, it smears edges. We want the quad to be bounded?
     *
     * For now, implementing standard infinite scroll loop via UVs.
     */

    // If loop is forced off in config, we might need a different UV calc,
    // but typically stage backgrounds loop.

    float u0 = scroll_x / effective_w;
    float v0 = scroll_y / effective_h;
    float u1 = (scroll_x + vp_w) / effective_w;
    float v1 = (scroll_y + vp_h) / effective_h;

    // Support non-looping layers by clamping UVs?
    // Or just rely on GL_CLAMP_TO_EDGE if user intended it.
    // The previous implementation used CLAMP_TO_EDGE universally.

    // If user explicitly wants NO loop, we should probably set GL_CLAMP_TO_EDGE.
    // If they WANT loop, we need GL_REPEAT.
    // But the old code used CLAMP_TO_EDGE and handled looping via logic?
    // Wait, ModdedStage_Render sets CLAMP_TO_EDGE.
    // If we want wrapping, we need GL_REPEAT.
    // Let's set the wrap mode based on config.

    float verts[] = {
        0.0f, 0.0f, u0, v0, 1.0f, 0.0f, u1, v0, 1.0f, 1.0f, u1, v1, 0.0f, 1.0f, u0, v1,
    };

    glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    /* Bind layer texture */
    GLuint tex_id = (GLuint)(intptr_t)res->texture;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint wrap_s = cfg->loop_x ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    GLint wrap_t = cfg->loop_y ? GL_REPEAT : GL_CLAMP_TO_EDGE;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

    if (layer_index > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (layer_index > 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void ModdedStage_Render(const BG* bg) {
    if (!bg || s_layer_res_count == 0)
        return;

    /* Lazy-init the shader on first render */
    init_shader();

    /* Save current GL state */
    GLint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    GLint prev_vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);

    static const float projection[4][4] = { { 2.0f, 0.0f, 0.0f, 0.0f },
                                            { 0.0f, -2.0f, 0.0f, 0.0f },
                                            { 0.0f, 0.0f, -1.0f, 0.0f },
                                            { -1.0f, 1.0f, 0.0f, 1.0f } };

    glUseProgram(s_shader_program);
    glUniformMatrix4fv(s_loc_projection, 1, GL_FALSE, (const float*)projection);
    glUniform1i(s_loc_texture, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(s_quad_vao);

    GLboolean prev_blend = glIsEnabled(GL_BLEND);
    GLint prev_src, prev_dst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_dst);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Sort layers by Z-Index from Config */
    // We can create a lightweight index array
    int order[MAX_STAGE_LAYERS];
    int count = 0;

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        if (g_stage_config.layers[i].enabled && s_layer_res[i].texture) {
            order[count++] = i;
        }
    }

    // Create a simple mapping to help insertion sort
    struct SortNode {
        int index;
        int z;
    } sort_buf[MAX_STAGE_LAYERS];

    for (int i = 0; i < count; i++) {
        sort_buf[i].index = order[i];
        sort_buf[i].z = g_stage_config.layers[order[i]].z_index;
    }

    // Insertion sort by z_index
    for (int i = 1; i < count; i++) {
        struct SortNode key = sort_buf[i];
        int j = i - 1;
        while (j >= 0 && sort_buf[j].z > key.z) {
            sort_buf[j + 1] = sort_buf[j];
            j--;
        }
        sort_buf[j + 1] = key;
    }

    for (int i = 0; i < count; i++) {
        draw_layer(sort_buf[i].index, bg_prm); // Pass global bg_prm array
    }

    /* Restore previous GL state */
    if (!prev_blend)
        glDisable(GL_BLEND);
    glBlendFunc(prev_src, prev_dst);
    glBindVertexArray(prev_vao);
    glUseProgram(prev_program);
}
