/**
 * @file training_prediction.c
 * @brief Forward simulation of physics and hitboxes for the Training AI.
 */

#include "training_prediction.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/pls02.h"

// Helper to simulate 1 frame of physics exactly like cal_mvxy_speed and add_mvxy_speed
static void simulate_physics_frame(PredictedPlayerState* p) {
    // X axis
    p->speed_x += p->accel_x;
    p->pos_x += p->speed_x;

    // Y axis
    p->speed_y += p->accel_y;
    p->pos_y += p->speed_y;

    if (p->pos_y < 0) {
        p->pos_y = 0; // Ground clamped
        p->speed_y = 0;
    }
}

void predict_physics_state(PredictedGameState* out, s16 frames_ahead) {
    if (!out)
        return;

    // Initialize from current true PLW state
    // P1
    out->p1.pos_x = plw[0].wu.xyz[0].disp.pos;
    out->p1.pos_y = plw[0].wu.xyz[1].disp.pos;
    out->p1.speed_x = plw[0].wu.mvxy.a[0].sp >> 8;
    out->p1.speed_y = plw[0].wu.mvxy.a[1].sp >> 8;
    out->p1.accel_x = plw[0].wu.mvxy.d[0].sp >> 8;
    out->p1.accel_y = plw[0].wu.mvxy.d[1].sp >> 8;
    out->p1.p_body = plw[0].wu.h_bod;
    out->p1.p_pushbox = plw[0].wu.h_hos;

    // P2
    out->p2.pos_x = plw[1].wu.xyz[0].disp.pos;
    out->p2.pos_y = plw[1].wu.xyz[1].disp.pos;
    out->p2.speed_x = plw[1].wu.mvxy.a[0].sp >> 8;
    out->p2.speed_y = plw[1].wu.mvxy.a[1].sp >> 8;
    out->p2.accel_x = plw[1].wu.mvxy.d[0].sp >> 8;
    out->p2.accel_y = plw[1].wu.mvxy.d[1].sp >> 8;
    out->p2.p_body = plw[1].wu.h_bod;
    out->p2.p_pushbox = plw[1].wu.h_hos;

    out->frame_offset = frames_ahead;

    // Advance N frames linearly
    for (s16 i = 0; i < frames_ahead; ++i) {
        simulate_physics_frame(&out->p1);
        simulate_physics_frame(&out->p2);
    }
}
