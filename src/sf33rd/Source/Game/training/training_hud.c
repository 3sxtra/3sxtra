#include "training_hud.h"
#include "game_state.h"
#include "rendering/game_renderer.h"
#include "port/training_menu.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/trials.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "training_state.h"
#include "port/I_System.h"
#include <stdio.h>

void training_hud_init() {
    // Basic setup if required
}

static void draw_box(s16 left, s16 right, s16 top, s16 bottom, u32 color) {
    Quad q;

    // Use the actual main screen center, left edge = center - 192
    s16 cam_x = get_center_position() - 192;
    s16 cam_y = get_height_position();

    f32 ground_offset = 24.0f; // character Y=0 floor baseline offset from bottom of screen

    f32 sx_l = (f32)(left - cam_x) * g_state.scr_sc;
    f32 sx_r = (f32)(right - cam_x) * g_state.scr_sc;
    f32 sy_t = 224.0f - (f32)(top - cam_y) * g_state.scr_sc - ground_offset;
    f32 sy_b = 224.0f - (f32)(bottom - cam_y) * g_state.scr_sc - ground_offset;

    for (int i = 0; i < 4; i++) {
        q.v[i].z = -1.0f;
    }

    q.v[0].x = sx_l;
    q.v[0].y = sy_t;
    q.v[1].x = sx_r;
    q.v[1].y = sy_t;
    q.v[2].x = sx_l;
    q.v[2].y = sy_b;
    q.v[3].x = sx_r;
    q.v[3].y = sy_b;

    Renderer_DrawSolidQuad(&q, color);
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
    if (g_training_menu_settings.show_pushboxes && player->wu.pushbox) {
        calc_and_draw_box(pos_x, pos_y, flip, player->wu.pushbox->hos_box, 0x8000FF00, 0);
    }

    // Hurtboxes (Blue)
    if (g_training_menu_settings.show_hurtboxes && player->wu.body_hurtbox) {
        for (int i = 0; i < 4; i++) {
            if (player->wu.body_hurtbox->body_dm[i][1] != 0)
                calc_and_draw_box(pos_x, pos_y, flip, player->wu.body_hurtbox->body_dm[i], 0x400000FF, 0);
        }
    }

    // Hitboxes (Red)
    if (g_training_menu_settings.show_attackboxes && player->wu.attack_hitbox) {
        for (int i = 0; i < 4; i++) {
            if (player->wu.attack_hitbox->att_box[i][1] != 0)
                calc_and_draw_box(pos_x, pos_y, flip, player->wu.attack_hitbox->att_box[i], 0xC0FF0000, 0);
        }
    }

    // Throwable box (Pink) — clamped to minimum visible size
    if (g_training_menu_settings.show_throwboxes && player->wu.caught_box) {
        if (!is_empty_box(player->wu.caught_box->cau_box))
            calc_and_draw_box(pos_x, pos_y, flip, player->wu.caught_box->cau_box, 0x60FF80FF, 1);
    }

    // Throw hitbox (Yellow) — clamped to minimum visible size
    if (g_training_menu_settings.show_throwboxes && player->wu.catch_box) {
        if (!is_empty_box(player->wu.catch_box->cat_box))
            calc_and_draw_box(pos_x, pos_y, flip, player->wu.catch_box->cat_box, 0x80FFFF00, 1);
    }
}

void training_hud_draw() {
    // Called each frame — hitboxes are still rendered via C (GPU quads).
    // Stun/life/meter text is now handled by the RmlUI HUD overlay.
    if ((g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_TRIALS) && !show_training_menu) {
        if (g_training_menu_settings.show_hitboxes || g_training_menu_settings.show_pushboxes ||
            g_training_menu_settings.show_hurtboxes || g_training_menu_settings.show_attackboxes ||
            g_training_menu_settings.show_throwboxes) {
            training_hud_draw_hitboxes(&g_state.plw[0]);
            training_hud_draw_hitboxes(&g_state.plw[1]);
        }
    }

    trials_draw();
}
