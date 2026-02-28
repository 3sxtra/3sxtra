#include "training_hud.h"
#include "game_state.h"
#include "port/renderer.h"
#include "port/sdl/training_menu.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/trials.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "training_state.h"
#include <SDL3/SDL.h>
#include <stdio.h>

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui_phase3_toggles.h"
extern bool use_rmlui;

void training_hud_init() {
    // Basic setup if required
}

void training_hud_draw_stun(PLW* player, TrainingPlayerState* state) {
    if (!player || !state)
        return;

    // Check if the player is currently stunned using the external piyori_type array
    s16 p_index = player->wu.id; // Get the player index (0 or 1)

    // Check if the player is in damage state (e.g. 0x5D is damage) or stun value is high
    if (player->wu.char_state.body.fields.cg_type >= 0x40 || piyori_type[p_index].now.timer > 0) {
        // Use the native combo stun tracker we added to state, which doesn't decay
        if (state->combo_stun > 0 || piyori_type[p_index].now.timer > 0) {
            // Draw numerical text showing accumulated stun for this combo
            char stun_str[32];
            snprintf(stun_str, sizeof(stun_str), "STUN: %d", state->combo_stun);

            s16 hud_x = (p_index == 0) ? 10 : 250;
            s16 hud_y = 60;
            if (!use_rmlui || !rmlui_hud_training_stun)
                SSPutStr_Bigger(hud_x, hud_y, 5, stun_str, 1.0f, 0, 1.0f);
        }
    }
}

static void draw_box(s16 left, s16 right, s16 top, s16 bottom, u32 color) {
    RendererVertex v[4];

    // Use the actual main screen center, left edge = center - 192
    s16 cam_x = get_center_position() - 192;
    s16 cam_y = get_height_position();

    f32 ground_offset = 24.0f; // character Y=0 floor baseline offset from bottom of screen

    f32 sx_l = (f32)(left - cam_x) * scr_sc;
    f32 sx_r = (f32)(right - cam_x) * scr_sc;
    f32 sy_t = 224.0f - (f32)(top - cam_y) * scr_sc - ground_offset;
    f32 sy_b = 224.0f - (f32)(bottom - cam_y) * scr_sc - ground_offset;

    // (Debug logs removed, coordinates confirmed correct)

    for (int i = 0; i < 4; i++) {
        v[i].z = -1.0f;
        v[i].color = color;
    }

    v[0].x = sx_l;
    v[0].y = sy_t;
    v[1].x = sx_r;
    v[1].y = sy_t;
    v[2].x = sx_l;
    v[2].y = sy_b;
    v[3].x = sx_r;
    v[3].y = sy_b;

    Renderer_DrawSolidQuad(v, 4);
}

/**
 * Compute a bounding box from a 4-element array [x_off, width, y_off, height]
 * relative to the player position and facing direction, then draw it.
 *
 * When clamp_min_size is true, zero-dimension boxes are expanded to a minimum
 * visible size (used for throw-range boxes that may be 1-D checks).
 */
static void calc_and_draw_box(s16 pos_x, s16 pos_y, s8 flip, const s16 box[4], u32 color, int clamp_min_size) {
    s16 l, r, t, b;
    if (flip == 1) {
        l = pos_x + box[0];
        r = l + box[1];
    } else {
        l = pos_x - box[0] - box[1];
        r = pos_x - box[0];
    }
    b = pos_y + box[2];
    t = b + box[3];

    if (clamp_min_size) {
        if (l == r) {
            r += 2;
            l -= 2;
        }
        if (t == b) {
            t += 100;
            b -= 10;
        }
    }

    draw_box(l, r, t, b, color);
}

/** Returns true if all four elements of a box array are zero. */
static int is_empty_box(const s16 box[4]) {
    return box[0] == 0 && box[1] == 0 && box[2] == 0 && box[3] == 0;
}

void training_hud_draw_hitboxes(PLW* player) {
    if (!player)
        return;

    s16 pos_x = player->wu.xyz[0].disp.pos;
    s16 pos_y = player->wu.xyz[1].disp.pos;
    s8 flip = player->wu.rl_flag ? -1 : 1;

    // Pushbox (Green)
    if (g_training_menu_settings.show_pushboxes && player->wu.h_hos) {
        calc_and_draw_box(pos_x, pos_y, flip, player->wu.h_hos->hos_box, 0x8000FF00, 0);
    }

    // Hurtboxes (Blue)
    if (g_training_menu_settings.show_hurtboxes && player->wu.h_bod) {
        for (int i = 0; i < 4; i++) {
            if (player->wu.h_bod->body_dm[i][1] != 0)
                calc_and_draw_box(pos_x, pos_y, flip, player->wu.h_bod->body_dm[i], 0x400000FF, 0);
        }
    }

    // Hitboxes (Red)
    if (g_training_menu_settings.show_attackboxes && player->wu.h_att) {
        for (int i = 0; i < 4; i++) {
            if (player->wu.h_att->att_box[i][1] != 0)
                calc_and_draw_box(pos_x, pos_y, flip, player->wu.h_att->att_box[i], 0xC0FF0000, 0);
        }
    }

    // Throwable box (Pink) — clamped to minimum visible size
    if (g_training_menu_settings.show_throwboxes && player->wu.h_cau) {
        if (!is_empty_box(player->wu.h_cau->cau_box))
            calc_and_draw_box(pos_x, pos_y, flip, player->wu.h_cau->cau_box, 0x60FF80FF, 1);
    }

    // Throw hitbox (Yellow) — clamped to minimum visible size
    if (g_training_menu_settings.show_throwboxes && player->wu.h_cat) {
        if (!is_empty_box(player->wu.h_cat->cat_box))
            calc_and_draw_box(pos_x, pos_y, flip, player->wu.h_cat->cat_box, 0x80FFFF00, 1);
    }
}

void training_hud_draw() {
    // Called each frame from menu_draw.c or equivalent to render our custom Training HUD
    if ((Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS) && !show_training_menu) {
        if (g_training_menu_settings.show_stun) {
            training_hud_draw_stun(&plw[0], &g_training_state.p1);
            training_hud_draw_stun(&plw[1], &g_training_state.p2);
        }

        if (g_training_menu_settings.show_hitboxes || g_training_menu_settings.show_pushboxes ||
            g_training_menu_settings.show_hurtboxes || g_training_menu_settings.show_attackboxes ||
            g_training_menu_settings.show_throwboxes) {
            training_hud_draw_hitboxes(&plw[0]);
            training_hud_draw_hitboxes(&plw[1]);
        }
    }

    trials_draw();
}
