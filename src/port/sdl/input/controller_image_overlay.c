/**
 * @file controller_image_overlay.c
 * @brief ControllerImage button-glyph overlay — texture cache for Button Config menu.
 *
 * Generates GPU textures from ControllerImage SDL_Surfaces for each connected
 * controller's button slots. Uses a deferred draw queue: effect_10/effect_23
 * record overlay requests during sprite processing, then FlushGL() draws them
 * at full window resolution after the CPS3 canvas is upscaled.
 */
#include "port/sdl/input/controller_image_overlay.h"
#include "port/sdl/input/controller_image.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/sdl/app/sdl_app.h"
#include <SDL3/SDL.h>
#include <stddef.h>
#include <string.h>
#include <glad/gl.h>

#define OVERLAY_MAX_SLOTS 4
#define OVERLAY_GLYPH_SIZE 128 /* Source resolution for ControllerImage SVG glyphs */

/**
 * Mapping from button-config row index to SDL gamepad button/axis.
 *
 * Row layout (matching scrnAddTex1UV sprite order and the screenshot):
 *   0: □ / X / Y     → SDL_GAMEPAD_BUTTON_WEST
 *   1: △ / Y / X     → SDL_GAMEPAD_BUTTON_NORTH
 *   2: R1 / RB / R   → SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
 *   3: L1 / LB / L   → SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
 *   4: × / A / B     → SDL_GAMEPAD_BUTTON_SOUTH
 *   5: ○ / B / A     → SDL_GAMEPAD_BUTTON_EAST
 *   6: R2 / RT / ZR  → axis (SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
 *   7: L2 / LT / ZL  → axis (SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
 */
static const int s_row_is_axis[CONTROLLER_OVERLAY_BUTTON_COUNT] = { 0, 0, 0, 0, 0, 0, 1, 1 };

static const int s_row_to_button[CONTROLLER_OVERLAY_BUTTON_COUNT] = {
    SDL_GAMEPAD_BUTTON_WEST,           /* row 0: □ */
    SDL_GAMEPAD_BUTTON_NORTH,          /* row 1: △ */
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, /* row 2: R1 */
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  /* row 3: L1 */
    SDL_GAMEPAD_BUTTON_SOUTH,          /* row 4: × */
    SDL_GAMEPAD_BUTTON_EAST,           /* row 5: ○ */
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,    /* row 6: R2 (axis enum) */
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER,     /* row 7: L2 (axis enum) */
};

typedef struct {
    void* textures[CONTROLLER_OVERLAY_BUTTON_COUNT];
    bool valid;
} SlotCache;

static SlotCache s_slots[OVERLAY_MAX_SLOTS];
static bool s_initialized = false;

/* --- Deferred Draw Queue --- */
#define OVERLAY_DRAW_QUEUE_MAX 32

typedef struct {
    int slot;
    int button_row;
    float canvas_x, canvas_y, canvas_w, canvas_h;
} OverlayDrawCmd;

static OverlayDrawCmd s_draw_queue[OVERLAY_DRAW_QUEUE_MAX];
static int s_draw_queue_count = 0;

/* GL resources for post-composition rendering */
static GLuint s_overlay_vao = 0;
static GLuint s_overlay_vbo = 0;

static void generate_slot(int slot) {
    SlotCache* cache = &s_slots[slot];
    memset(cache, 0, sizeof(*cache));

    const char* device_type = ControllerImage_Module_GetDeviceType(slot);
    if (!device_type) {
        return;
    }

    SDL_Log("[ControllerImageOverlay] Generating glyphs for slot %d (type=%s)", slot, device_type);

    for (int row = 0; row < CONTROLLER_OVERLAY_BUTTON_COUNT; row++) {
        SDL_Surface* surface = NULL;

        if (s_row_is_axis[row]) {
            surface = ControllerImage_Module_CreateAxisSurface(
                slot, (SDL_GamepadAxis)s_row_to_button[row], OVERLAY_GLYPH_SIZE);
        } else {
            surface = ControllerImage_Module_CreateButtonSurface(
                slot, (SDL_GamepadButton)s_row_to_button[row], OVERLAY_GLYPH_SIZE);
        }

        if (surface) {
            cache->textures[row] = TextureUtil_LoadFromSurface(surface);
            SDL_DestroySurface(surface);
        }

        if (!cache->textures[row]) {
            SDL_LogDebug(
                SDL_LOG_CATEGORY_APPLICATION, "[ControllerImageOverlay] No glyph for slot %d row %d", slot, row);
        }
    }

    cache->valid = true;
}

void ControllerImageOverlay_Init(void) {
    if (s_initialized) {
        ControllerImageOverlay_Shutdown();
    }

    memset(s_slots, 0, sizeof(s_slots));

    for (int slot = 0; slot < OVERLAY_MAX_SLOTS; slot++) {
        generate_slot(slot);
    }

    s_draw_queue_count = 0;
    s_initialized = true;
    SDL_Log("[ControllerImageOverlay] Initialized (%d slots)", OVERLAY_MAX_SLOTS);
}

void ControllerImageOverlay_Shutdown(void) {
    if (!s_initialized) {
        return;
    }

    for (int slot = 0; slot < OVERLAY_MAX_SLOTS; slot++) {
        SlotCache* cache = &s_slots[slot];
        for (int row = 0; row < CONTROLLER_OVERLAY_BUTTON_COUNT; row++) {
            if (cache->textures[row]) {
                TextureUtil_Free(cache->textures[row]);
                cache->textures[row] = NULL;
            }
        }
        cache->valid = false;
    }

    if (s_overlay_vao) {
        glDeleteVertexArrays(1, &s_overlay_vao);
        s_overlay_vao = 0;
    }
    if (s_overlay_vbo) {
        glDeleteBuffers(1, &s_overlay_vbo);
        s_overlay_vbo = 0;
    }
    /* Don't delete shader — it's SDLApp's passthru or scene shader */

    s_draw_queue_count = 0;
    s_initialized = false;
    SDL_Log("[ControllerImageOverlay] Shut down");
}

void* ControllerImageOverlay_GetTexture(int slot, int button_row) {
    if (!s_initialized)
        return NULL;
    if (slot < 0 || slot >= OVERLAY_MAX_SLOTS)
        return NULL;
    if (button_row < 0 || button_row >= CONTROLLER_OVERLAY_BUTTON_COUNT)
        return NULL;
    if (!s_slots[slot].valid)
        return NULL;
    return s_slots[slot].textures[button_row];
}

bool ControllerImageOverlay_HasSlot(int slot) {
    if (!s_initialized || slot < 0 || slot >= OVERLAY_MAX_SLOTS)
        return false;
    return s_slots[slot].valid;
}

bool ControllerImageOverlay_DrawButton(int slot, int button_row, int px, int py, int sx, int sy, int pz) {
    (void)pz; /* z-depth not needed for deferred screen-space rendering */

    void* tex = ControllerImageOverlay_GetTexture(slot, button_row);
    if (!tex) {
        return false;
    }

    /* Enqueue a deferred draw command — will be flushed at window resolution
     * after the CPS3 canvas is upscaled to the screen. */
    if (s_draw_queue_count < OVERLAY_DRAW_QUEUE_MAX) {
        OverlayDrawCmd* cmd = &s_draw_queue[s_draw_queue_count++];
        cmd->slot = slot;
        cmd->button_row = button_row;
        cmd->canvas_x = (float)px;
        cmd->canvas_y = (float)py;
        cmd->canvas_w = (float)sx;
        cmd->canvas_h = (float)sy;
    }
    return true;
}

void ControllerImageOverlay_FlushGL(float vp_x, float vp_y, float vp_w, float vp_h, int win_w, int win_h) {
    if (!s_initialized || s_draw_queue_count == 0)
        return;

    /* Scale factors: CPS3 canvas (384×224) → viewport pixels */
    const float scale_x = vp_w / 384.0f;
    const float scale_y = vp_h / 224.0f;

    /* Use SDLApp's scene shader (position + color + texcoord, "Source" sampler) */
    GLuint shader = SDLApp_GetSceneShaderProgram();
    if (!shader) {
        s_draw_queue_count = 0;
        return;
    }

    /* Save GL state */
    GLint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    GLint prev_vp[4];
    glGetIntegerv(GL_VIEWPORT, prev_vp);

    /* Set up for screen-space rendering */
    glViewport(0, 0, win_w, win_h);
    glUseProgram(shader);

    /* Orthographic projection: 0,0 = top-left, win_w,win_h = bottom-right */
    float proj[16] = { 2.0f / (float)win_w,
                       0.0f,
                       0.0f,
                       0.0f,
                       0.0f,
                       -2.0f / (float)win_h,
                       0.0f,
                       0.0f,
                       0.0f,
                       0.0f,
                       -1.0f,
                       0.0f,
                       -1.0f,
                       1.0f,
                       0.0f,
                       1.0f };
    GLint loc_proj = glGetUniformLocation(shader, "projection");
    glUniformMatrix4fv(loc_proj, 1, GL_FALSE, proj);
    GLint loc_source = glGetUniformLocation(shader, "Source");
    glUniform1i(loc_source, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);

    /* Create VAO/VBO on first use */
    if (!s_overlay_vao) {
        glGenVertexArrays(1, &s_overlay_vao);
        glGenBuffers(1, &s_overlay_vbo);
    }
    glBindVertexArray(s_overlay_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_overlay_vbo);

    /* Draw each queued glyph */
    for (int i = 0; i < s_draw_queue_count; i++) {
        OverlayDrawCmd* cmd = &s_draw_queue[i];
        void* tex = ControllerImageOverlay_GetTexture(cmd->slot, cmd->button_row);
        if (!tex)
            continue;

        GLuint gl_tex = (GLuint)(intptr_t)tex;

        /* Convert CPS3 canvas coords → screen coords */
        float sx = vp_x + cmd->canvas_x * scale_x;
        float sy = vp_y + cmd->canvas_y * scale_y;
        float sw = cmd->canvas_w * scale_x;
        float sh = cmd->canvas_h * scale_y;

        /* SDL_Vertex layout: position(2f), color(4B RGBA), texcoord(2f) */
        SDL_Vertex verts[4];
        uint32_t white = 0xFFFFFFFF;

        verts[0].position.x = sx;
        verts[0].position.y = sy;
        verts[0].tex_coord.x = 0.0f;
        verts[0].tex_coord.y = 0.0f;
        memcpy(&verts[0].color, &white, sizeof(uint32_t));

        verts[1].position.x = sx + sw;
        verts[1].position.y = sy;
        verts[1].tex_coord.x = 1.0f;
        verts[1].tex_coord.y = 0.0f;
        memcpy(&verts[1].color, &white, sizeof(uint32_t));

        verts[2].position.x = sx;
        verts[2].position.y = sy + sh;
        verts[2].tex_coord.x = 0.0f;
        verts[2].tex_coord.y = 1.0f;
        memcpy(&verts[2].color, &white, sizeof(uint32_t));

        verts[3].position.x = sx + sw;
        verts[3].position.y = sy + sh;
        verts[3].tex_coord.x = 1.0f;
        verts[3].tex_coord.y = 1.0f;
        memcpy(&verts[3].color, &white, sizeof(uint32_t));

        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (void*)offsetof(SDL_Vertex, tex_coord));

        glBindTexture(GL_TEXTURE_2D, gl_tex);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /* Restore GL state */
    glUseProgram(prev_program);
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);

    s_draw_queue_count = 0;
}

void ControllerImageOverlay_ClearQueue(void) {
    s_draw_queue_count = 0;
}
