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
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/input/control_mapping_bindings.h"
#include "port/input_definition.h"
#include "port/sdl/input/sdl_pad.h"
#include <SDL3/SDL.h>
#include <stddef.h>
#include <string.h>
#include "port/sdl/renderer/gl_compat.h"

#define OVERLAY_MAX_SLOTS 4
#define OVERLAY_GLYPH_SIZE 128 /* Source resolution for ControllerImage SVG glyphs */

static SDL_Scancode legacy_keyboard_reverse_map_button(SDL_GamepadButton btn) {
    switch (btn) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return SDL_SCANCODE_J;
    case SDL_GAMEPAD_BUTTON_EAST:
        return SDL_SCANCODE_K;
    case SDL_GAMEPAD_BUTTON_WEST:
        return SDL_SCANCODE_U;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return SDL_SCANCODE_I;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return SDL_SCANCODE_O;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return SDL_SCANCODE_P;
    case SDL_GAMEPAD_BUTTON_BACK:
        return SDL_SCANCODE_BACKSPACE;
    case SDL_GAMEPAD_BUTTON_START:
        return SDL_SCANCODE_RETURN;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return SDL_SCANCODE_9;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return SDL_SCANCODE_0;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return SDL_SCANCODE_W;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return SDL_SCANCODE_S;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return SDL_SCANCODE_A;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return SDL_SCANCODE_D;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static SDL_Scancode legacy_keyboard_reverse_map_axis(SDL_GamepadAxis axis) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        return SDL_SCANCODE_SEMICOLON;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        return SDL_SCANCODE_L;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

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

static const char* s_row_to_action[CONTROLLER_OVERLAY_BUTTON_COUNT] = {
    "Light Punch",  /* 0 */
    "Medium Punch", /* 1 */
    "Hard Punch",   /* 2 */
    NULL,           /* 3 - Unmapped by default, CPS3 shows L1 */
    "Light Kick",   /* 4 */
    "Medium Kick",  /* 5 */
    "Hard Kick",    /* 6 */
    NULL            /* 7 - Unmapped by default, CPS3 shows L2 */
};

static bool input_to_gamepad_button(InputID id, SDL_GamepadButton* out_btn) {
    switch (id) {
    case INPUT_ID_BUTTON_SOUTH:
        *out_btn = SDL_GAMEPAD_BUTTON_SOUTH;
        return true;
    case INPUT_ID_BUTTON_EAST:
        *out_btn = SDL_GAMEPAD_BUTTON_EAST;
        return true;
    case INPUT_ID_BUTTON_WEST:
        *out_btn = SDL_GAMEPAD_BUTTON_WEST;
        return true;
    case INPUT_ID_BUTTON_NORTH:
        *out_btn = SDL_GAMEPAD_BUTTON_NORTH;
        return true;
    case INPUT_ID_LEFT_SHOULDER:
        *out_btn = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        return true;
    case INPUT_ID_RIGHT_SHOULDER:
        *out_btn = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        return true;
    case INPUT_ID_DPAD_UP:
        *out_btn = SDL_GAMEPAD_BUTTON_DPAD_UP;
        return true;
    case INPUT_ID_DPAD_DOWN:
        *out_btn = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
        return true;
    case INPUT_ID_DPAD_LEFT:
        *out_btn = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
        return true;
    case INPUT_ID_DPAD_RIGHT:
        *out_btn = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        return true;
    case INPUT_ID_START:
        *out_btn = SDL_GAMEPAD_BUTTON_START;
        return true;
    case INPUT_ID_BACK:
        *out_btn = SDL_GAMEPAD_BUTTON_BACK;
        return true;
    case INPUT_ID_LEFT_STICK:
        *out_btn = SDL_GAMEPAD_BUTTON_LEFT_STICK;
        return true;
    case INPUT_ID_RIGHT_STICK:
        *out_btn = SDL_GAMEPAD_BUTTON_RIGHT_STICK;
        return true;
    default:
        return false;
    }
}

static bool input_to_gamepad_axis(InputID id, SDL_GamepadAxis* out_axis) {
    switch (id) {
    case INPUT_ID_LEFT_TRIGGER:
        *out_axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        return true;
    case INPUT_ID_RIGHT_TRIGGER:
        *out_axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return true;
    case INPUT_ID_LEFT_STICK_X_PLUS:
    case INPUT_ID_LEFT_STICK_X_MINUS:
        *out_axis = SDL_GAMEPAD_AXIS_LEFTX;
        return true;
    case INPUT_ID_LEFT_STICK_Y_PLUS:
    case INPUT_ID_LEFT_STICK_Y_MINUS:
        *out_axis = SDL_GAMEPAD_AXIS_LEFTY;
        return true;
    case INPUT_ID_RIGHT_STICK_X_PLUS:
    case INPUT_ID_RIGHT_STICK_X_MINUS:
        *out_axis = SDL_GAMEPAD_AXIS_RIGHTX;
        return true;
    case INPUT_ID_RIGHT_STICK_Y_PLUS:
    case INPUT_ID_RIGHT_STICK_Y_MINUS:
        *out_axis = SDL_GAMEPAD_AXIS_RIGHTY;
        return true;
    default:
        return false;
    }
}

static void generate_slot(int player_idx) {
    SlotCache* cache = &s_slots[player_idx];
    memset(cache, 0, sizeof(*cache));

    int player_num = player_idx + 1; // 1 for P1, 2 for P2
    int device_id = ControlMapping_GetPlayerDeviceID(player_num);
    bool use_custom_mapping = true;
    bool is_keyboard = false;

    if (device_id == -1) {
        // Fall back to legacy physical slot assignment (P1 -> slot 0, P2 -> slot 1)
        device_id = player_idx;
        use_custom_mapping = false;
        is_keyboard = SDLPad_IsKeyboard(device_id);

        // Ensure there actually is a trackable device in this slot
        if (!is_keyboard && !ControllerImage_Module_GetDeviceType(device_id)) {
            return;
        }
    } else {
        is_keyboard = SDLPad_IsKeyboard(device_id);
    }

    SDL_Log("[ControllerImageOverlay] Generating glyphs for player %d (device_id=%d, keyboard=%d, custom=%d)",
            player_num,
            device_id,
            is_keyboard,
            use_custom_mapping);

    for (int row = 0; row < CONTROLLER_OVERLAY_BUTTON_COUNT; row++) {
        SDL_Surface* surface = NULL;
        const char* action = s_row_to_action[row];

        if (use_custom_mapping) {
            InputID mapped_id = INPUT_ID_UNKNOWN;
            if (action != NULL) {
                mapped_id = ControlMapping_GetPlayerMapping(player_num, action);
            }

            if (mapped_id != INPUT_ID_UNKNOWN) {
                if (is_keyboard_input(mapped_id)) {
                    SDL_Scancode scancode = (SDL_Scancode)(mapped_id - INPUT_ID_KEY_BASE);
                    surface = ControllerImage_Module_CreateScancodeSurface(scancode, OVERLAY_GLYPH_SIZE);
                } else {
                    SDL_GamepadButton out_btn;
                    SDL_GamepadAxis out_axis;
                    if (input_to_gamepad_button(mapped_id, &out_btn)) {
                        surface = ControllerImage_Module_CreateButtonSurface(device_id, out_btn, OVERLAY_GLYPH_SIZE);
                    } else if (input_to_gamepad_axis(mapped_id, &out_axis)) {
                        surface = ControllerImage_Module_CreateAxisSurface(device_id, out_axis, OVERLAY_GLYPH_SIZE);
                    }
                }
            }
        }

        // --- FALLBACK ---
        // If the mapped action didn't yield an image (or was unmapped), fall back
        // to the physical default layout if it's NOT a keyboard, OR use the reverse
        // mapped default keyboard layout if it IS a keyboard. This prevents the
        // pink CPS3 sprites from bleeding thru.
        if (!surface) {
            if (is_keyboard) {
                SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
                if (s_row_is_axis[row]) {
                    scancode = legacy_keyboard_reverse_map_axis((SDL_GamepadAxis)s_row_to_button[row]);
                } else {
                    scancode = legacy_keyboard_reverse_map_button((SDL_GamepadButton)s_row_to_button[row]);
                }
                if (scancode != SDL_SCANCODE_UNKNOWN) {
                    surface = ControllerImage_Module_CreateScancodeSurface(scancode, OVERLAY_GLYPH_SIZE);
                }
            } else if (ControllerImage_Module_GetDeviceType(device_id) || !use_custom_mapping) {
                // If it's a known gamepad, OR if we fell back to physical slot 0/1
                if (s_row_is_axis[row]) {
                    surface = ControllerImage_Module_CreateAxisSurface(
                        device_id, (SDL_GamepadAxis)s_row_to_button[row], OVERLAY_GLYPH_SIZE);
                } else {
                    surface = ControllerImage_Module_CreateButtonSurface(
                        device_id, (SDL_GamepadButton)s_row_to_button[row], OVERLAY_GLYPH_SIZE);
                }
            }
        }

        if (surface) {
            cache->textures[row] = TextureUtil_LoadFromSurface(surface);
            SDL_DestroySurface(surface);
        }

        if (!cache->textures[row]) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                         "[ControllerImageOverlay] No glyph for player %d row %d fallback=%d",
                         player_num,
                         row,
                         s_row_to_button[row]);
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
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
