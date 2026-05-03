/**
 * @file bg_sub.c
 * Stage Subroutines
 */

#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "game_state.h"
#include "sf33rd/Source/Game/stage/bg_constants.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"
#include "structs.h"

// ⚡ Bolt: const — place dispatch table in .rodata (read-only memory)
void (*const scr_x_mv_jp[35])() = { scr_10_20,   scr_10_21,   scr_10_22,   scr_x_dummy, scr_x_dummy, scr_x_dummy,
                                    scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy,
                                    scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_11_20,   scr_11_21,
                                    scr_11_22,   scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy,
                                    scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy, scr_x_dummy,
                                    scr_x_dummy, scr_x_dummy, scr_12_20,   scr_12_21,   scr_12_22 };

// Forward decls
static s16 remake_x_mvstep(s16 mvstep);
static s32 suzi_offset_set_sub(State_Other* ewk);

/** @brief Handle CGA zoom logic for the current stage. */
void check_cg_zoom() {
    s16 i;
    s16 zoom_wk;
    u16 p1zoom;
    u16 p2zoom;
    u16 zmlv;
    u16 lookp1;
    u16 lookp2;

    p1zoom = g_state.plw[0].wu.cg_zoom;
    p2zoom = g_state.plw[1].wu.cg_zoom;

    if (g_state.bg_stop != 0 && !((p1zoom | p2zoom) & ZOOM_DISABLE)) {
        zmlv = p1zoom & ZOOM_LEVEL_MASK;

        if (zmlv < (p2zoom & ZOOM_LEVEL_MASK)) {
            zmlv = p2zoom & ZOOM_LEVEL_MASK;
        }

        lookp1 = p1zoom >> ZOOM_LOOK_SHIFT & ZOOM_LOOK_FIELD;
        lookp2 = p2zoom >> ZOOM_LOOK_SHIFT & ZOOM_LOOK_FIELD;
        p1zoom = zmlv | (lookp2 << 12 | lookp1 << ZOOM_LOOK_SHIFT);
        p2zoom = zmlv | (lookp1 << 12 | lookp2 << ZOOM_LOOK_SHIFT);
    }

    g_state.zoom_req_flag_old = g_state.zoom_request_flag;
    g_state.zoom_request_flag = 0;

    for (i = 0; i < 2; i++) {
        if (g_state.plw[i].scr_pos_set_flag) {
            g_state.plw[i].wu.scr_mv_x = g_state.plw[i].wu.xyz[0].disp.pos;
            g_state.plw[i].wu.scr_mv_y = g_state.plw[i].wu.xyz[1].disp.pos;
        } else if (g_state.plw[i].is_being_thrown) {
            g_state.plw[i].wu.scr_mv_x = g_state.plw[(i + 1) & 1].wu.xyz[0].disp.pos;
            g_state.plw[i].wu.scr_mv_y = g_state.plw[(i + 1) & 1].wu.xyz[1].disp.pos;
        }
    }

    zoom_wk = p2zoom & ZOOM_X_MASK;

    switch (p1zoom & ZOOM_X_MASK) {
    case ZOOM_DISABLE:
        break;

    case ZOOM_LOOK_LEFT:
        switch (zoom_wk) {
        case ZOOM_DISABLE:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[0].wu.xyz[0].disp.pos;
            break;

        case ZOOM_LOOK_LEFT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = (g_state.plw[0].wu.xyz[0].disp.pos + g_state.plw[1].wu.xyz[0].disp.pos) >> 1;
            break;

        case ZOOM_LOOK_RIGHT:
        case 0x0:
        case ZOOM_LOOK_BOTH:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[1].wu.xyz[0].disp.pos;
            break;
        }
        break;

    case ZOOM_LOOK_RIGHT:
        switch (zoom_wk) {
        case ZOOM_LOOK_LEFT:
        case 0x0:
        case ZOOM_DISABLE:
        case ZOOM_LOOK_BOTH:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[0].wu.xyz[0].disp.pos;
            break;

        case ZOOM_LOOK_RIGHT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = (g_state.plw[0].wu.xyz[0].disp.pos + g_state.plw[1].wu.xyz[0].disp.pos) >> 1;
            break;
        }
        break;

    case 0x0:
        switch (zoom_wk) {
        case ZOOM_LOOK_BOTH:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = (g_state.plw[0].wu.xyz[0].disp.pos + g_state.plw[1].wu.xyz[0].disp.pos) >> 1;
            break;

        case ZOOM_LOOK_LEFT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[0].wu.xyz[0].disp.pos;
            break;

        case ZOOM_LOOK_RIGHT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[1].wu.xyz[0].disp.pos;
            break;

        case ZOOM_DISABLE:
        case 0x0:
            break;
        }
        break;

    case ZOOM_LOOK_BOTH:
        switch (zoom_wk) {
        case 0x0:
        case ZOOM_LOOK_BOTH:
        case ZOOM_DISABLE:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = (g_state.plw[0].wu.xyz[0].disp.pos + g_state.plw[1].wu.xyz[0].disp.pos) >> 1;
            break;
        case ZOOM_LOOK_LEFT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[0].wu.xyz[0].disp.pos;
            break;
        case ZOOM_LOOK_RIGHT:
            g_state.zoom_request_flag = ZOOM_REQ_X;
            g_state.scr_req_x = g_state.plw[1].wu.xyz[0].disp.pos;
            break;

            break;
        }
        break;
    }

    zoom_wk = p2zoom & ZOOM_Y_MASK;

    switch (p1zoom & ZOOM_Y_MASK) {
    case ZOOM_DISABLE:
        g_state.zoom_request_flag |= ZOOM_REQ_Y;
        g_state.scr_req_y = 0;
        break;

    case ZOOM_Y_LOOK_UP:
        switch (zoom_wk) {
        case ZOOM_Y_LOOK_UP:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = (g_state.plw[0].wu.xyz[1].disp.pos + g_state.plw[1].wu.xyz[1].disp.pos) >> 1;
            break;

        case ZOOM_Y_LOOK_DOWN:
        case 0x0:
        case ZOOM_Y_LOOK_BOTH:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[1].wu.xyz[1].disp.pos;
            break;

        case ZOOM_DISABLE:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = 0;
            break;
        }
        break;

    case ZOOM_Y_LOOK_DOWN:
        switch (zoom_wk) {
        case ZOOM_Y_LOOK_UP:
        case 0x0:
        case ZOOM_Y_LOOK_BOTH:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[0].wu.xyz[1].disp.pos;
            break;

        case ZOOM_Y_LOOK_DOWN:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = (g_state.plw[0].wu.xyz[1].disp.pos + g_state.plw[1].wu.xyz[1].disp.pos) >> 1;
            break;

        case ZOOM_DISABLE:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = 0;
            break;
        }
        break;

    case 0x0:
        switch (zoom_wk) {
        case ZOOM_Y_LOOK_UP:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[0].wu.xyz[1].disp.pos;
            break;

        case ZOOM_Y_LOOK_DOWN:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[1].wu.xyz[1].disp.pos;
            break;

        case ZOOM_Y_LOOK_BOTH:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = (g_state.plw[0].wu.xyz[1].disp.pos + g_state.plw[1].wu.xyz[1].disp.pos) >> 1;
            break;

        case 0x0:
            break;

        case ZOOM_DISABLE:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = 0;
            break;
        }
        break;

    case ZOOM_Y_LOOK_BOTH:
        switch (zoom_wk) {
        case ZOOM_Y_LOOK_UP:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[0].wu.xyz[1].disp.pos;
            break;

        case ZOOM_Y_LOOK_DOWN:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = g_state.plw[1].wu.xyz[1].disp.pos;
            break;

        case ZOOM_Y_LOOK_BOTH:
        case 0x0:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = (g_state.plw[0].wu.xyz[1].disp.pos + g_state.plw[1].wu.xyz[1].disp.pos) >> 1;
            break;

        case ZOOM_DISABLE:
            g_state.zoom_request_flag |= ZOOM_REQ_Y;
            g_state.scr_req_y = 0;
            break;
        }
        break;
    }

    g_state.zoom_request_level = p1zoom & ZOOM_LEVEL_MASK;

    if (g_state.zoom_request_level < (p2zoom & ZOOM_LEVEL_MASK)) {
        g_state.zoom_request_level = p2zoom & ZOOM_LEVEL_MASK;
    }

    if (g_state.zoom_request_level) {
        g_state.zoom_request_flag |= ZOOM_REQ_ACTIVE;
    }
}

/** @brief Process camera chase movement each frame. */
void bg_chase_move() {
    if (!g_state.Bonus_Game_Flag) {
        chase_start_check();

        if (g_state.bg_w.chase_flag) {
            chase_xy_move();
        }
    }
}

/** @brief Check conditions to start camera chase mode. */
void chase_start_check() {
    s16 work;
    s16 work2;

    if (g_state.zoom_request_flag & ZOOM_REQ_X_FIELD) {
        if (g_state.chase_x != g_state.scr_req_x) {
            g_state.chase_x = g_state.scr_req_x;

            if (bgw_ptr->zuubun) {
                bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->abs_x + g_state.bg_w.pos_offset;
            } else {
                bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->position_x + g_state.bg_w.pos_offset;
            }

            g_state.chase_time_x = 6;
            cal_bg_speed_data_x(bgw_ptr->fam_no, g_state.chase_time_x, g_state.chase_x);
            g_state.bg_w.chase_flag |= CHASE_X_ACTIVE;
            g_state.bg_w.chase_flag &= ~CHASE_X_RETURN;
            g_state.bg_w.old_chase_flag = CHASE_X_ACTIVE;
        }
    } else {
        work = g_state.zoom_req_flag_old & ZOOM_REQ_X_FIELD;
        work2 = ~(g_state.zoom_request_flag & ZOOM_REQ_X_FIELD);
        work &= work2;

        if (work) {
            g_state.bg_w.chase_flag |= CHASE_X_RETURN;
            g_state.bg_w.chase_flag &= ~CHASE_X_ACTIVE;
            g_state.chase_x = bgw_ptr->wxy[0].disp.pos;
            g_state.chase_time_x = 6;
            cal_bg_speed_data_x(bgw_ptr->fam_no, g_state.chase_time_x, g_state.chase_x);
        }
    }

    if (g_state.zoom_request_flag & ZOOM_REQ_Y_FIELD) {
        g_state.bg_w.chase_flag |= CHASE_Y_ACTIVE;
        g_state.bg_w.chase_flag &= ~CHASE_Y_RETURN;
        g_state.bg_w.old_chase_flag |= CHASE_Y_ACTIVE;
        g_state.bg_w.old_chase_flag &= ~CHASE_Y_RETURN;

        if (g_state.chase_y != g_state.scr_req_y) {
            g_state.chase_y = g_state.scr_req_y;

            if (bgw_ptr->abs_y < 0) {
                bgw_ptr->chase_xy[1].disp.pos = 0;
            } else {
                bgw_ptr->chase_xy[1].disp.pos = bgw_ptr->abs_y;
            }

            g_state.chase_time_y = 6;
            cal_bg_speed_data_y(bgw_ptr->fam_no, g_state.chase_time_y, g_state.chase_y);
        }
    } else {
        work = g_state.zoom_req_flag_old & ZOOM_REQ_Y_FIELD;
        work2 = ~(g_state.zoom_request_flag & ZOOM_REQ_Y_FIELD);
        work &= work2;

        if (work) {
            g_state.bg_w.chase_flag |= CHASE_Y_RETURN;
            g_state.bg_w.chase_flag &= ~CHASE_Y_ACTIVE;
            g_state.chase_y = bgw_ptr->xy[1].disp.pos;
            g_state.chase_time_y = 6;
            cal_bg_speed_data_y(bgw_ptr->fam_no, g_state.chase_time_y, g_state.chase_y);
        }
    }
}

/** @brief Move the camera chase target position. */
void chase_xy_move() {
    if (g_state.bg_w.chase_flag & CHASE_X_MASK) {
        g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;

        if (g_state.bg_w.chase_flag & CHASE_X_ACTIVE) {
            g_state.chase_time_x -= 1;

            if (g_state.chase_time_x > 0) {
                g_state.bg_mvxy.a[0].sp += g_state.bg_mvxy.d[0].sp;
                bgw_ptr->chase_xy[0].cal += g_state.bg_mvxy.a[0].sp;
            }
        }

        if (g_state.bg_w.chase_flag & CHASE_X_RETURN) {
            g_state.chase_time_x -= 1;

            if (g_state.chase_time_x > 0) {
                g_state.bg_mvxy.a[0].sp += g_state.bg_mvxy.d[0].sp;
                bgw_ptr->chase_xy[0].cal += g_state.bg_mvxy.a[0].sp;
            } else {
                g_state.bg_w.chase_flag &= ~CHASE_X_MASK;
                g_state.bg_w.old_chase_flag &= ~CHASE_X_MASK;
                bgw_ptr->chase_xy[0].disp.pos = g_state.chase_x;
            }
        }

        if (bgw_ptr->chase_xy[0].disp.pos > bgw_ptr->r_limit2) {
            bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->r_limit2;
            bgw_ptr->chase_xy[0].disp.low = 0;
        }

        if (bgw_ptr->chase_xy[0].disp.pos < bgw_ptr->l_limit2) {
            bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->l_limit2;
            bgw_ptr->chase_xy[0].disp.low = 0;
        }

        g_state.bg_w.bg2_sp_x = g_state.bg_w.bg2_sp_x2 = bgw_ptr->chase_xy[0].disp.pos - bgw_ptr->pos_x_work;
    }

    if (g_state.bg_w.chase_flag & CHASE_Y_MASK) {
        if (g_state.bg_w.chase_flag & CHASE_Y_ACTIVE) {
            g_state.chase_time_y -= 1;

            if (g_state.chase_time_y > 0) {
                g_state.bg_mvxy.a[1].sp += g_state.bg_mvxy.d[1].sp;
                bgw_ptr->chase_xy[1].cal += g_state.bg_mvxy.a[1].sp;
            }
        }

        if (g_state.bg_w.chase_flag & CHASE_Y_RETURN) {
            g_state.chase_time_y -= 1;

            if (g_state.chase_time_y > 0) {
                g_state.bg_mvxy.a[1].sp += g_state.bg_mvxy.d[1].sp;
                bgw_ptr->chase_xy[1].cal += g_state.bg_mvxy.a[1].sp;
            } else {
                g_state.bg_w.chase_flag &= CHASE_X_MASK;
                g_state.bg_w.old_chase_flag &= CHASE_X_MASK;
                bgw_ptr->chase_xy[1].disp.pos = g_state.chase_y;
            }
        }

        if (bgw_ptr->chase_xy[1].disp.pos > bgw_ptr->y_limit2) {
            bgw_ptr->chase_xy[1].disp.pos = bgw_ptr->y_limit2;
            bgw_ptr->chase_xy[1].disp.low = 0;
        }

        g_state.bg_w.bg2_sp_y = bgw_ptr->chase_xy[1].disp.pos - bgw_ptr->pos_y_work;
    }
}

/** @brief Apply movement tweaks to x and y scrolling. */
void Bg_mv_tw(s32 value_x, s32 value_y) {
    bgw_ptr->xy[0].cal += value_x;
    bgw_ptr->xy[1].cal += value_y;
    bgw_ptr->wxy[0].cal += value_x;
    bgw_ptr->wxy[1].cal += value_y;
}

/** @brief Clamp scroll position against the right boundary. */
void x_right_check(s16 d1) {
    s32 speed_w;

    g_state.bg_w.scr_stop &= ~3;
    speed_w = bgw_ptr->speed_x * d1;
    g_state.ideal_w.iw[0].cal += speed_w;
}

/** @brief Clamp scroll position against the left boundary. */
void x_left_check(s16 d0) {
    s32 speed_w;

    g_state.bg_w.scr_stop &= ~3;
    speed_w = bgw_ptr->speed_x * d0;
    g_state.ideal_w.iw[0].cal += speed_w;
}

/** @brief No-op scroll function placeholder. */
void scr_x_dummy() {}

/** @brief Scroll handler for stage 10, screen layer 0. */
void scr_10_20() {}

/** @brief Scroll handler for stage 10, screen layer 1. */
void scr_10_21() {
    s16 meri;
    meri = g_state.plw[1].wu.scr_mv_x - satse[g_state.plw[1].player_number];
    meri = meri - (g_state.ideal_w.iw[0].disp.pos - g_state.bg_w.pos_offset + SCR_ZONE_LEFT);

    x_left_check(meri);
}

/** @brief Scroll handler for stage 10, screen layer 2. */
void scr_10_22() {
    s16 meri;
    meri = g_state.plw[1].wu.scr_mv_x + satse[g_state.plw[1].player_number];
    meri = meri - (g_state.ideal_w.iw[0].disp.pos + g_state.bg_w.pos_offset - SCR_ZONE_LEFT_ADJ);

    x_right_check(meri);
}

/** @brief Scroll handler for stage 11, screen layer 0. */
void scr_11_20() {
    s16 meri;
    meri = g_state.plw[0].wu.scr_mv_x - satse[g_state.plw[0].player_number];
    meri = meri - (g_state.ideal_w.iw[0].disp.pos - g_state.bg_w.pos_offset + SCR_ZONE_LEFT);

    x_left_check(meri);
}

/** @brief Scroll handler for stage 11, screen layer 1. */
void scr_11_21() {
    s16 meri;

    if (g_state.plw[0].wu.scr_mv_x < g_state.plw[1].wu.scr_mv_x) {
        meri = g_state.plw[0].wu.scr_mv_x - satse[g_state.plw[0].player_number];
    } else {
        meri = g_state.plw[1].wu.scr_mv_x - satse[g_state.plw[1].player_number];
    }

    meri = meri - (g_state.ideal_w.iw[0].disp.pos - g_state.bg_w.pos_offset + SCR_ZONE_LEFT);

    x_left_check(meri);
}

/** @brief Scroll handler for stage 11, screen layer 2. */
void scr_11_22() {
    s16 meri;
    s16 meri2;

    meri = (satse[g_state.plw[1].player_number] - satse[g_state.plw[0].player_number]);
    meri >>= 1;
    meri2 = g_state.plw[0].wu.scr_mv_x + g_state.plw[1].wu.scr_mv_x;
    meri2 >>= 1;
    meri2 += meri;
    meri2 -= g_state.ideal_w.iw[0].disp.pos;

    if (meri2 < 0) {
        if (g_state.plw[1].close_proximity_flag != 1) {
            x_left_check(meri2);
        }
    } else if (g_state.plw[0].close_proximity_flag != 2) {
        x_right_check(meri2);
    }
}

/** @brief Scroll handler for stage 12, screen layer 0. */
void scr_12_20() {
    s16 meri;
    meri = g_state.plw[0].wu.scr_mv_x + satse[g_state.plw[0].player_number];
    meri = meri - (g_state.ideal_w.iw[0].disp.pos + g_state.bg_w.pos_offset - SCR_ZONE_LEFT_ADJ);

    x_right_check(meri);
}

/** @brief Scroll handler for stage 12, screen layer 1. */
void scr_12_21() {
    s16 meri;
    s16 meri2;

    meri = (satse[g_state.plw[0].player_number] - satse[g_state.plw[1].player_number]);
    meri >>= 1;
    meri2 = g_state.plw[0].wu.scr_mv_x + g_state.plw[1].wu.scr_mv_x;
    meri2 >>= 1;
    meri2 += meri;
    meri2 -= g_state.ideal_w.iw[0].disp.pos;

    if (meri2 < 0) {
        if (g_state.plw[0].close_proximity_flag != 1) {
            x_left_check(meri2);
        }
    } else if (g_state.plw[1].close_proximity_flag != 2) {
        x_right_check(meri2);
    }
}

/** @brief Scroll handler for stage 12, screen layer 2. */
void scr_12_22() {
    s16 meri;

    if (g_state.plw[0].wu.scr_mv_x > g_state.plw[1].wu.scr_mv_x) {
        meri = g_state.plw[0].wu.scr_mv_x + satse[g_state.plw[0].player_number];
    } else {
        meri = g_state.plw[1].wu.scr_mv_x + satse[g_state.plw[1].player_number];
    }

    meri = meri - (g_state.ideal_w.iw[0].disp.pos + g_state.bg_w.pos_offset - SCR_ZONE_LEFT_ADJ);

    x_right_check(meri);
}

/** @brief Sub-routine for base horizontal scroll movement. */
void bg_base_x_move_sub() {
    s16 work[2];
    s8 st[2];
    s8 i;
    s16 calculated_bg_pos;

    calculated_bg_pos = g_state.ideal_w.iw[0].disp.pos - g_state.bg_w.pos_offset;

    if (g_state.plw[0].wu.scr_mv_x < g_state.ideal_w.iw[0].disp.pos) {
        work[0] = g_state.plw[0].wu.scr_mv_x - *&satse[g_state.plw[0].player_number];
    } else {
        work[0] = g_state.plw[0].wu.scr_mv_x + *&satse[g_state.plw[0].player_number];
    }

    work[0] -= calculated_bg_pos;

    if (g_state.plw[1].wu.scr_mv_x < g_state.ideal_w.iw[0].disp.pos) {
        work[1] = g_state.plw[1].wu.scr_mv_x - *&satse[g_state.plw[1].player_number];
    } else {
        work[1] = g_state.plw[1].wu.scr_mv_x + *&satse[g_state.plw[1].player_number];
    }

    work[1] -= calculated_bg_pos;

    if (work[0] < 0) {
        work[0] = 0;
    } else if (work[0] > SCR_ZONE_MAX) {
        work[0] = SCR_ZONE_MAX;
    }

    if (work[1] < 0) {
        work[1] = 0;
    } else if (work[1] > SCR_ZONE_MAX) {
        work[1] = SCR_ZONE_MAX;
    }

    for (i = 0; i < 2; i++) {
        if (0 <= work[i] && work[i] < SCR_ZONE_LEFT) {
            st[i] = 1;
        } else if (work[i] >= SCR_ZONE_RIGHT_START && work[i] < SCR_ZONE_RIGHT_END) {
            st[i] = 2;
        } else {
            st[i] = 0;
        }
    }

    scr_x_mv_jp[(st[0] << 4) + st[1]]();
}

/** @brief Check and apply base horizontal scroll boundaries. */
void bg_base_x_move_check() {
    s16 mvstep, old_work;

    g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;

    if (!g_state.bg_stop && !g_state.bg_app_stop) {
        if (g_state.bg_w.chase_flag & CHASE_X_MASK) {
            bgw_ptr->old_pos_x = bgw_ptr->chase_xy[0].disp.pos;
        } else {
            bgw_ptr->old_pos_x = bgw_ptr->wxy[0].disp.pos;
        }

        old_work = bgw_ptr->wxy[0].disp.pos;
        g_state.ideal_w.iw[0].cal = bgw_ptr->wxy[0].cal;
        bg_base_x_move_sub();
        mvstep = g_state.ideal_w.iw[0].disp.pos;
        mvstep -= old_work;
        g_state.ideal_w.iw[0].cal = 0;
        g_state.ideal_w.iw[0].disp.pos = mvstep;

        if (mvstep) {
            if (mvstep < 0) {
                if (mvstep < -g_state.bg_w.max_x) {
                    mvstep = -g_state.bg_w.max_x;
                }
                mvstep = -remake_x_mvstep(-mvstep);
            } else {
                if (mvstep > g_state.bg_w.max_x) {
                    mvstep = g_state.bg_w.max_x;
                }
                mvstep = remake_x_mvstep(mvstep);
            }
        }

        g_state.ideal_w.iw[0].disp.pos = mvstep;
        Bg_mv_tw(g_state.ideal_w.iw[0].cal, 0);

        if (bgw_ptr->wxy[0].disp.pos < bgw_ptr->l_limit2) {
            bgw_ptr->wxy[0].disp.pos = bgw_ptr->l_limit2;
            bgw_ptr->wxy[0].disp.low = 0;
            bgw_ptr->xy[0].disp.pos = bgw_ptr->l_limit2;
            bgw_ptr->xy[0].disp.low = 0;
        }

        if (bgw_ptr->wxy[0].disp.pos > bgw_ptr->r_limit2) {
            bgw_ptr->wxy[0].disp.pos = bgw_ptr->r_limit2;
            bgw_ptr->wxy[0].disp.low = 0;
            bgw_ptr->xy[0].disp.pos = bgw_ptr->r_limit2;
            bgw_ptr->xy[0].disp.low = 0;
        }
    }

    g_state.bg_w.bg2_sp_x = bgw_ptr->xy[0].disp.pos - bgw_ptr->pos_x_work;
    g_state.bg_w.bg2_sp_x2 = bgw_ptr->wxy[0].disp.pos - bgw_ptr->pos_x_work;

    if (!(g_state.bg_w.chase_flag & CHASE_X_MASK)) {
        bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->wxy[0].disp.pos;
    }
}

/** @brief Recalculate horizontal movement step size. */
static s16 remake_x_mvstep(s16 mvstep) {
    return mvstep * SCR_SPEED_NUMERATOR / SCR_SPEED_DENOMINATOR;
}

/** @brief Set or release a fixed vertical position for the camera. */
void Bg_Y_Sitei(u8 on_off, s16 pos) {
    g_state.y_fixed_flag = on_off;

    if (on_off == 0) {
        g_state.y_fixed_pos = 0;
        return;
    }

    g_state.y_fixed_pos = pos;
}

/** @brief Check and apply base vertical scroll boundaries. */
void bg_base_y_move_check() {
    s32 pos_w, kake;
    s16 hi_pos;

    if (g_state.y_fixed_flag == 1) {
        hi_pos = g_state.y_fixed_pos;
    } else {
        if (g_state.bg_stop)
            goto end;
        if (g_state.bg_app_stop)
            goto end;

        if (g_state.plw[0].wu.scr_mv_y > g_state.plw[1].wu.scr_mv_y) {
            hi_pos = g_state.plw[0].wu.scr_mv_y;
        } else {
            hi_pos = g_state.plw[1].wu.scr_mv_y;
        }
    }

    hi_pos -= BG_Y_VERTICAL_OFFSET;

    if (hi_pos <= 0) {
        bgw_ptr->wxy[1].cal = 0;
        bgw_ptr->xy[1].cal = 0;
    } else {
        kake = BG_Y_SCALE_FACTOR;
        pos_w = kake * hi_pos;

        bgw_ptr->xy[1].cal = 0;
        bgw_ptr->wxy[1].cal = 0;

        bgw_ptr->xy[1].cal += pos_w;
        bgw_ptr->wxy[1].cal += pos_w;

        if (bgw_ptr->xy[1].disp.pos > bgw_ptr->y_limit2) {
            bgw_ptr->xy[1].disp.pos = bgw_ptr->y_limit2;
            bgw_ptr->xy[1].disp.low = 0;
            bgw_ptr->wxy[1].disp.pos = bgw_ptr->y_limit2;
            bgw_ptr->wxy[1].disp.low = 0;
            g_state.bg_w.scr_stop &= 0x7FFF;
        }
    }
end:
    g_state.bg_w.bg2_sp_y = bgw_ptr->xy[1].disp.pos - bgw_ptr->pos_y_work;
}

/** @brief Check and apply horizontal scroll for a foreground layer. */
void bg_x_move_check() {
    if (g_state.bg_w.chase_flag & CHASE_X_MASK) {
        bgw_ptr->old_pos_x = bgw_ptr->chase_xy[0].disp.pos;
    } else {
        bgw_ptr->old_pos_x = bgw_ptr->wxy[0].disp.pos;
    }

    if (g_state.bg_w.chase_flag & CHASE_X_MASK) {
        bgw_ptr->chase_xy[0].cal = bgw_ptr->speed_x * g_state.bg_w.bg2_sp_x2;
        bgw_ptr->chase_xy[0].disp.pos += bgw_ptr->pos_x_work;
    } else {
        bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal = bgw_ptr->speed_x * g_state.bg_w.bg2_sp_x2;
        bgw_ptr->xy[0].disp.pos += bgw_ptr->pos_x_work;
        bgw_ptr->wxy[0].disp.pos = bgw_ptr->xy[0].disp.pos;
    }

    if (!(g_state.bg_w.chase_flag & CHASE_X_MASK)) {
        bgw_ptr->chase_xy[0].disp.pos = bgw_ptr->wxy[0].disp.pos;
    }
}

/** @brief Check and apply vertical scroll for a foreground layer. */
void bg_y_move_check() {
    if (g_state.bg_w.chase_flag & CHASE_Y_MASK) {
        bgw_ptr->chase_xy[1].cal = bgw_ptr->speed_y * g_state.bg_w.bg2_sp_y;

        if (bgw_ptr->y_limit2 < bgw_ptr->chase_xy[1].disp.pos) {
            bgw_ptr->chase_xy[1].disp.pos = bgw_ptr->y_limit2;
            bgw_ptr->chase_xy[1].disp.low = 0;
        }

        bgw_ptr->chase_xy[1].disp.pos += bgw_ptr->pos_y_work;

        return;
    }

    bgw_ptr->xy[1].cal = bgw_ptr->speed_y * g_state.bg_w.bg2_sp_y;

    if (bgw_ptr->y_limit2 < bgw_ptr->xy[1].disp.pos) {
        bgw_ptr->xy[1].disp.pos = bgw_ptr->y_limit2;
        bgw_ptr->xy[1].disp.low = 0;
    }

    bgw_ptr->xy[1].disp.pos += bgw_ptr->pos_y_work;
    bgw_ptr->wxy[1].cal = bgw_ptr->xy[1].cal;
}

/** @brief Update zoom and vertical offset for the stage frame. */
void zoom_ud_check() {
    s16 work;
    s16 work2;
    s16 pos_w;

    if (g_state.bg_app) {
        return;
    }

    if (g_state.Bonus_Game_Flag) {
        return;
    }

    if (g_state.bg_app_stop && g_state.bg_w.bg_f_x == ZOOM_FRAME_DEFAULT) {
        return;
    }

    work2 = g_state.zoom_request_flag & ZOOM_LEVEL_MASK;
    g_state.bg_w.frame_deff = ZOOM_FRAME_DEFAULT - g_state.zoom_request_level;
    work = (~(g_state.zoom_req_flag_old) & (g_state.zoom_request_flag) & ZOOM_LEVEL_MASK);

    if (work && !g_state.bg_w.frame_flag) {
        g_state.bg_w.frame_flag = 1;
        g_state.bg_w.old_frame_flag = 1;
        g_state.bg_w.center_y = BG_SCREEN_HEIGHT - g_state.scr_req_y;

        if (g_state.bg_w.center_y < 0) {
            g_state.bg_w.center_y = 0;
        }

        if (g_state.scr_req_x < g_state.bg_w.bgw[1].l_limit2) {
            if (g_state.bg_w.bgw[1].zuubun != 0) {
                g_state.bg_w.center_x = g_state.scr_req_x + BG_WRAP_WIDTH;
                pos_w = g_state.bg_w.bgw[1].wxy[0].disp.pos + BG_WRAP_WIDTH;
            } else {
                g_state.bg_w.center_x = g_state.scr_req_x;
                pos_w = g_state.bg_w.bgw[1].wxy[0].disp.pos;
            }

            pos_w -= g_state.bg_w.pos_offset;
            g_state.bg_w.center_x -= pos_w;
        } else if (g_state.bg_w.bgw[1].r_limit2 < g_state.scr_req_x) {
            if (g_state.bg_w.bgw[1].zuubun != 0) {
                g_state.bg_w.center_x = g_state.scr_req_x + BG_WRAP_WIDTH;
                pos_w = g_state.bg_w.bgw[1].wxy[0].disp.pos + BG_WRAP_WIDTH;
            } else {
                g_state.bg_w.center_x = g_state.scr_req_x;
                pos_w = g_state.bg_w.bgw[1].wxy[0].disp.pos;
            }

            pos_w -= g_state.bg_w.pos_offset;
            g_state.bg_w.center_x -= pos_w;
        } else {
            g_state.bg_w.center_x = BG_POS_OFFSET_DEFAULT;
            g_state.bg_w.center_y = BG_SCREEN_HEIGHT;
        }
    }

    if (work2) {
        if (g_state.bg_w.bg_f_x > g_state.bg_w.frame_deff) {
            Frame_Up((u16)g_state.bg_w.center_x, (u16)g_state.bg_w.center_y, 1);
            g_state.bg_w.bg_f_x--;
            g_state.bg_w.bg_f_y--;
            return;
        }

        if (g_state.bg_w.bg_f_x < g_state.bg_w.frame_deff) {
            Frame_Down((u16)g_state.bg_w.center_x, (u16)g_state.bg_w.center_y, 1);
            g_state.bg_w.bg_f_x++;
            g_state.bg_w.bg_f_y++;

            if (g_state.bg_w.bg_f_x == ZOOM_FRAME_DEFAULT) {
                g_state.bg_w.frame_flag = 0;
                Zoomf_Init();
            }
        }
    } else if (g_state.bg_w.bg_f_x < ZOOM_FRAME_DEFAULT) {
        Frame_Down((u16)g_state.bg_w.center_x, (u16)g_state.bg_w.center_y, 1);
        g_state.bg_w.bg_f_x++;
        g_state.bg_w.bg_f_y++;

        if (g_state.bg_w.bg_f_x == ZOOM_FRAME_DEFAULT) {
            g_state.bg_w.frame_flag = 0;
            Zoomf_Init();
        }
    }
}

/** @brief Set suzi (tile strip) offset for a background object. */
void suzi_offset_set(State_Other* ewk) {
    if (ewk->wu.sync_bg_strip == 1) {
        suzi_offset_set_sub(ewk);
    }
}

/** @brief Calculate suzi offset sub-value for a background object. */
static s32 suzi_offset_set_sub(State_Other* ewk) {
    s16 work, work2;

    work = ewk->wu.xyz[1].disp.pos & 0x300;
    work = 0x300 - work;

    work2 = ewk->wu.xyz[1].disp.pos & 0xFF;
    work2 = 0x100 - work2;

    work += work2;

    return 0;
}

/** @brief Synchronize object position with suzi tile strip data. */
void sync_bg_strip_position(State_Other* ewk) {
    s16 work;

    if (ewk->wu.sync_bg_strip) {
        if (ewk->wu.sync_bg_strip == 2) {
            suzi_offset_set_sub(ewk);
        }

        work = *ewk->wu.bg_strip_offset;
        work -= 0x200;
    } else {
        work = 0;
    }

    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos - work & 0xFFFF;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
}

/** @brief Update parallax family position for a single layer with Y offset. */
static void bg_family_set_layer(s32 index, s16 y_offset) {
    s16 x = g_state.bg_w.bgw[index].position_x;
    s16 y = g_state.bg_w.bgw[index].position_y;
    y += y_offset;
    Scrn_Move_Set(index, x, y);
    x = -x & 0xFFFF;
    y = (BG_FAMILY_HEIGHT - (y & 0xFFFF)) & 0xFFFF;
    Family_Set_W(index + 1, x, y);
}

/** @brief Update parallax family positions for all active layers. */
void Bg_Family_Set() {
    s8 i;
    for (i = 0; i < g_state.bg_w.scno; i++)
        bg_family_set_layer(i, 0);
}

/** @brief Update parallax family for a specific layer index. */
void Bg_Family_Set_appoint(s32 num_of_bg) {
    bg_family_set_layer(num_of_bg, 0);
}

/** @brief Update parallax family positions (alternate method, +8 Y offset). */
void Bg_Family_Set_2() {
    s8 i;
    for (i = 0; i < g_state.bg_w.scno; i++)
        bg_family_set_layer(i, 8);
}

/** @brief Update parallax family for a specific layer (alternate, +8 Y offset). */
void Bg_Family_Set_2_appoint(s32 num_of_bg) {
    bg_family_set_layer(num_of_bg, 8);
}

/** @brief Update parallax family for the Akebono (dawn) layer. */
void ake_Family_Set2() {
    s16 x = g_state.bg_w.bgw[3].position_x;
    s16 y = g_state.bg_w.bgw[3].position_y;

    Scrn_Move_Set(3, x, y);
    x = BG_WRAP_WIDTH - g_state.bg_w.pos_offset;
    y = 0;
    x = -x & 0xFFFF;
    y = (BG_FAMILY_HEIGHT - (y & 0xFFFF)) & 0xFFFF;
    Family_Set_W(4, x, y);
}

/** @brief Apply position correction to a background layer, optionally with quake offsets. */
static void bg_pos_adjust_impl(s16 bg_no, s16 apply_quake) {
    u16 pos;
    s16 pos2;

    pos2 = g_state.bg_w.bgw[bg_no].wxy[0].disp.pos;
    pos = pos2 & 0xFFFF;

    pos -= g_state.bg_w.pos_offset;
    pos2 -= g_state.bg_w.pos_offset;

    if (apply_quake) {
        if (g_state.bg_w.quake_x_index >= QUAKE_TABLE_SIZE)
            g_state.bg_w.quake_x_index = QUAKE_TABLE_SIZE - 1;
        pos += quake_x_tbl[g_state.bg_w.quake_x_index];
        pos2 += quake_x_tbl[g_state.bg_w.quake_x_index];
    }

    g_state.bg_w.bgw[bg_no].position_x = pos & 0xFFFF;
    g_state.bg_w.bgw[bg_no].abs_x = pos2;

    pos2 = g_state.bg_w.bgw[bg_no].xy[1].disp.pos;
    pos = pos2 & 0xFFFF;

    if (apply_quake) {
        if (g_state.bg_w.quake_y_index >= QUAKE_TABLE_SIZE)
            g_state.bg_w.quake_y_index = QUAKE_TABLE_SIZE - 1;
        pos += quake_y_tbl[g_state.bg_w.quake_y_index];
        pos2 += quake_y_tbl[g_state.bg_w.quake_y_index];
    }

    g_state.bg_w.bgw[bg_no].position_y = pos & 0xFFFF;
    g_state.bg_w.bgw[bg_no].abs_y = pos2;
}

/** @brief Apply position correction to a single background layer (with quake). */
void bg_pos_adjust_sub2(s16 bg_no) {
    bg_pos_adjust_impl(bg_no, 1);
}

/** @brief Apply position correction to a bg layer (without quake). */
void bg_pos_adjust_sub3(s16 bg_no) {
    bg_pos_adjust_impl(bg_no, 0);
}

/** @brief Apply position correction to all active background layers. */
void bg_pos_adjust2() {
    s16 bg_no = 0;
    u16 pos;
    u16 pos2;

    while (bg_no < g_state.bg_w.scno) {
        if ((g_state.bg_w.chase_flag & CHASE_X_MASK) != 0) {
            pos2 = g_state.bg_w.bgw[bg_no].chase_xy[0].disp.pos;
        } else {
            pos2 = g_state.bg_w.bgw[bg_no].wxy[0].disp.pos;
        }

        pos = pos2 & 0xFFFF;
        pos -= g_state.bg_w.pos_offset;
        if (g_state.bg_w.quake_x_index >= QUAKE_TABLE_SIZE)
            g_state.bg_w.quake_x_index = QUAKE_TABLE_SIZE - 1;
        pos += quake_x_tbl[g_state.bg_w.quake_x_index];

        g_state.bg_w.bgw[bg_no].position_x = pos & 0xFFFF;

        pos2 -= g_state.bg_w.pos_offset;
        pos2 += quake_x_tbl[g_state.bg_w.quake_x_index];

        g_state.bg_w.bgw[bg_no].abs_x = pos2;

        if ((g_state.bg_w.chase_flag & CHASE_Y_MASK) != 0) {
            pos2 = g_state.bg_w.bgw[bg_no].chase_xy[1].disp.pos;
        } else {
            pos2 = g_state.bg_w.bgw[bg_no].xy[1].disp.pos;
        }

        pos = pos2 & 0xFFFF;
        if (g_state.bg_w.quake_y_index >= QUAKE_TABLE_SIZE)
            g_state.bg_w.quake_y_index = QUAKE_TABLE_SIZE - 1;
        pos += quake_y_tbl[g_state.bg_w.quake_y_index];
        pos2 += quake_y_tbl[g_state.bg_w.quake_y_index];

        g_state.bg_w.bgw[bg_no].position_y = pos & 0xFFFF;
        g_state.bg_w.bgw[bg_no].abs_y = pos2;

        bg_no++;
    }
}

/** @brief Get the horizontal center position between both players. */
s16 get_center_position() {
    if (g_state.Bonus_Game_Flag == BONUS_GAME_PARRY) {
        return 0x200;
    }

    return g_state.bg_w.bgw[1].wxy[0].disp.pos;
}

/** @brief Get the vertical height position between both players. */
s16 get_height_position() {
    return g_state.bg_w.bgw[1].xy[1].disp.pos;
}

/** @brief Clear all background work variables to defaults. */
void bg_work_clear() {
    s16 i;

    g_state.bg_w.bg_routine = 0;
    g_state.bg_w.bg_r_1 = 0;
    g_state.bg_w.bg_r_2 = 0;
    g_state.bg_w.compel_flag = 0;
    g_state.win_sp_flag = 0;
    g_state.bg_stop = 0;
    g_state.akebono_flag = 0;
    g_state.aku_flag = 0;
    g_state.sa_pa_flag = 0;
    g_state.bg_app = 0;
    g_state.bg_app_stop = 0;

    for (i = 0; i < 7; i++) {
        g_state.bg_w.bgw[i].r_no_0 = 0;
        g_state.bg_w.bgw[i].r_no_1 = 0;
        g_state.bg_w.bgw[i].r_no_2 = 0;
    }
}

/** @brief Force-reset background positions after stage transitions. */
void compel_bg_init_position() {
    s16 i;

    g_state.bg_w.compel_flag = 0;
    Zoomf_Init();
    g_state.bg_w.bg_f_x = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.bg_f_y = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.scr_stop = 0;
    g_state.bg_w.frame_flag = 0;
    g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;

    for (i = 0; i < 7; i++) {
        g_state.bg_w.bgw[(i)].xy[0].disp.pos = g_state.bg_w.bgw[(i)].wxy[0].disp.pos = g_state.bg_w.bgw[(i)].pos_x_work;
        g_state.bg_w.bgw[(i)].xy[1].disp.pos = g_state.bg_w.bgw[(i)].wxy[1].disp.pos = g_state.bg_w.bgw[(i)].pos_y_work;
        g_state.bg_w.bgw[(i)].xy[0].disp.low = g_state.bg_w.bgw[(i)].wxy[0].disp.low = 0;
        g_state.bg_w.bgw[(i)].xy[0].disp.low = g_state.bg_w.bgw[(i)].wxy[0].disp.low = 0;
        g_state.bg_w.bgw[(i)].old_pos_x = g_state.bg_w.bgw[(i)].wxy[0].disp.pos;
    }
}

/** @brief Common base-layer scroll movement handler. */
void bg_base_move_common() {
    bg_base_x_move_check();
    bg_base_y_move_check();
    bg_chase_move();
}

/** @brief Common foreground-layer scroll movement handler. */
void bg_move_common() {
    bg_x_move_check();
    bg_y_move_check();
}

/** @brief Initialize background layers for the current stage. */
void bg_initialize() {
    const s16* ptr;
    u8 i;

    Bg_Off_R(7);
    Family_Init();
    Scrn_Pos_Init();
    Zoomf_Init();
    g_state.bg_w.bg_opaque = stage_opaque[g_state.bg_w.stage];
    g_state.Screen_Switch = 0;
    g_state.Screen_Switch_Buffer = 0;
    g_state.bg_disp_off = 0;
    g_state.bg_w.bg_index = bg_index_tbl[g_state.bg_w.stage][g_state.bg_w.area];
    g_state.bg_w.scno = use_scr[g_state.bg_w.bg_index];
    g_state.bg_w.scrno = use_real_scr[g_state.bg_w.bg_index];
    g_state.y_fixed_flag = 0;
    g_state.y_fixed_pos = 0;

    if (g_state.fsm[0] != 2 || g_state.fsm[1] != 2 || g_state.fsm[2] != 2) {
        Bg_Texture_Load_EX();
    }

    Bg_Kakikae_Set();
    g_state.bg_w.pos_offset = BG_POS_OFFSET_DEFAULT;

    for (i = 0; i < 7; i++) {
        g_state.bg_w.bgw[i].pos_x_work = g_state.bg_w.bgw[i].pos_y_work = 0;
        g_state.bg_w.bgw[i].zuubun = 0;
        g_state.bg_w.bgw[i].xy[0].cal = 0;
        g_state.bg_w.bgw[i].xy[1].cal = 0;
        g_state.bg_w.bgw[i].wxy[0].cal = 0;
        g_state.bg_w.bgw[i].wxy[1].cal = 0;
        g_state.bg_w.bgw[i].hos_xy[0].cal = 0;
        g_state.bg_w.bgw[i].hos_xy[1].cal = 0;
        g_state.bg_w.bgw[i].speed_x = 0;
        g_state.bg_w.bgw[i].speed_y = 0;
        g_state.bg_w.bgw[i].rewrite_flag = 0;
        g_state.bg_w.bgw[i].fam_no = i;
        g_state.bg_w.bgw[i].r_no_1 = g_state.bg_w.bgw[i].r_no_2 = 0;
        g_state.bg_w.bgw[i].speed_x = 0;
    }

    g_state.bg_w.scr_stop = 0;
    g_state.bg_w.frame_flag = 0;
    g_state.bg_w.bg_f_x = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.bg_f_y = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;
    g_state.bg_w.max_x = 8;
    g_state.bg_w.old_chase_flag = g_state.bg_w.chase_flag = 0;
    g_state.bg_w.quake_x_index = 0;
    g_state.bg_w.quake_y_index = 0;
    g_state.bg_w.frame_deff = ZOOM_FRAME_DEFAULT;

    for (i = 0; i < g_state.bg_w.scno; i++) {
        g_state.bg_w.bgw[i].speed_x = *msp[g_state.bg_w.bg_index][i];
        g_state.bg_w.bgw[i].speed_y = msp[g_state.bg_w.bg_index][i][1];
        g_state.bg_w.bgw[i].rewrite_flag = 0;
        g_state.bg_w.bgw[i].xy[1].disp.pos = g_state.bg_w.bgw[i].wxy[1].disp.pos = g_state.bg_w.bgw[i].pos_y_work = 0;
        ptr = limit_tbl3[g_state.bg_w.bg_index][i];
        g_state.bg_w.bgw[i].l_limit2 = *ptr++;
        g_state.bg_w.bgw[i].r_limit2 = *ptr++;
        g_state.bg_w.bgw[i].y_limit = *ptr++;
        g_state.bg_w.bgw[i].y_limit2 = *ptr++;
        g_state.bg_w.bgw[i].frame_deff = 0;
        g_state.bg_w.bgw[i].max_x_limit = g_state.bg_w.bgw[i].speed_x * g_state.bg_w.max_x;
    }

    if (g_state.bg_w.stage != STAGE_ELENA) {
        g_state.base_y_pos = 0x28;
    } else {
        g_state.base_y_pos = 0x30;
    }

    if (g_state.bg_w.stage > STAGE_BONUS_THRESHOLD) {
        bg_pos_adjust_sub3(2);
        Bg_Family_Set_appoint(2);
    }

    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Initialize the Akebono (dawn sky) background layer. */
void akebono_initialize() {
    g_state.bg_w.bgw[3].xy[0].cal = g_state.bg_w.bgw[3].wxy[0].cal = 0x100000;
    g_state.bg_w.bgw[3].xy[1].cal = g_state.bg_w.bgw[3].wxy[1].cal = 0;
    g_state.bg_w.bgw[3].position_x = BG_POS_OFFSET_DEFAULT - g_state.bg_w.pos_offset;
    g_state.bg_w.bgw[3].position_y = 0;
    Bg_Family_Set_appoint(3);
    g_state.bg_w.bgw[3].r_no_1 = g_state.bg_w.bgw[3].r_no_2 = 0;
    g_state.bg_w.bgw[3].fam_no = 3;
    Bg_Off_R(8);
}

/** @brief Write miscellaneous background elements (suzi, etc.). */
void bg_etc_write(s16 type) {
    u8 i;

    Family_Init();
    Scrn_Pos_Init();
    Zoomf_Init();
    g_state.bg_w.bg_opaque = BG_OPAQUE_DEFAULT;
    g_state.Screen_Switch = 0;
    g_state.Screen_Switch_Buffer = 0;
    g_state.bg_disp_off = 0;
    g_state.bg_w.scno = g_state.bg_w.scrno = use_scr2[type];
    g_state.bg_w.pos_offset = BG_POS_OFFSET_DEFAULT;

    Bg_Texture_Load2((u8)type);

    for (i = 0; i < 7; i++) {
        g_state.bg_w.bgw[i].pos_x_work = 0;
        g_state.bg_w.bgw[i].pos_y_work = 0;
        g_state.bg_w.bgw[i].hos_xy[1].cal = g_state.bg_w.bgw[i].hos_xy[0].cal = g_state.bg_w.bgw[i].wxy[1].cal =
            g_state.bg_w.bgw[i].wxy[0].cal = g_state.bg_w.bgw[i].xy[1].cal = g_state.bg_w.bgw[i].xy[0].cal =
                g_state.bg_w.bgw[i].zuubun = 0;
        g_state.bg_w.bgw[i].rewrite_flag = 0;
        g_state.bg_w.bgw[i].fam_no = i;
        g_state.bg_w.bgw[i].speed_y = g_state.bg_w.bgw[i].speed_x = 0;
        g_state.bg_w.bgw[i].r_no_1 = g_state.bg_w.bgw[i].r_no_2 = 0;
    }

    g_state.bg_w.scr_stop = 0;
    g_state.bg_w.frame_flag = 0;
    g_state.bg_w.old_chase_flag = g_state.bg_w.chase_flag = 0;
    g_state.bg_w.bg_f_x = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.bg_f_y = ZOOM_FRAME_DEFAULT;
    g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;
    g_state.bg_w.max_x = 8;
    g_state.bg_w.quake_x_index = 0;
    g_state.bg_w.quake_y_index = 0;

    for (i = 0; i < g_state.bg_w.scno; i++) {
        g_state.bg_w.bgw[i].hos_xy[0].cal = g_state.bg_w.bgw[i].wxy[0].cal = g_state.bg_w.bgw[i].xy[0].cal =
            bg_pos_tbl2[type][i][0];
        g_state.bg_w.bgw[i].hos_xy[1].cal = g_state.bg_w.bgw[i].wxy[1].cal = g_state.bg_w.bgw[i].xy[1].cal =
            bg_pos_tbl2[type][i][1];
        g_state.bg_w.bgw[i].pos_y_work = g_state.bg_w.bgw[i].xy[1].disp.pos;
        g_state.bg_w.bgw[i].old_pos_x = g_state.bg_w.bgw[i].pos_x_work = g_state.bg_w.bgw[i].xy[0].disp.pos;
        g_state.bg_w.bgw[i].speed_x = msp2[type][i][0];
        g_state.bg_w.bgw[i].speed_y = msp2[type][i][1];
        g_state.bg_w.bgw[i].rewrite_flag = 0;
        g_state.bg_w.bgw[i].zuubun = 0;
        g_state.bg_w.bgw[i].frame_deff = ZOOM_FRAME_DEFAULT;
        g_state.bg_w.bgw[i].max_x_limit = g_state.bg_w.bgw[i].speed_x * g_state.bg_w.max_x;
    }

    g_state.base_y_pos = 40;
}

/** @brief Check whether an object is outside the visible stage range. */
s32 Ck_Range_Out_S(State_Other* ewk, s16 BG_No, s16 R) {
    s16 x;

    x = ewk->wu.xyz[0].disp.pos - g_state.bg_w.bgw[BG_No].wxy[0].disp.pos;

    if (x < 0) {
        x = -x;
    }

    if (x - R > BG_POS_OFFSET_DEFAULT) {
        return 1;
    }

    return 0;
}
