/**
 * @file effe9.c
 * Effect: Ending / Renderer Effect
 */

#include "sf33rd/Source/Game/effect/effect_e9_ending_renderer.h"
#include "game_state.h"
#include "common.h"
#include "port/rendering/renderer.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/system/work_sys.h"

void effect_E9_move(State_Other* ewk) {
    PAL_CURSOR ita;
    PAL_CURSOR_P ita_p[4];
    PAL_CURSOR_P ita_pos[4];
    PAL_CURSOR_COL ita_col[4];
    f32 prio;

    ita.p = &ita_pos[0];
    ita.col = &ita_col[0];
    ita.num = 4;
    ita_col[0].color = ita_col[1].color = ita_col[2].color = ita_col[3].color = 0xFF000000;
    prio = PrioBase[ewk->wu.my_priority];

    if (ewk->wu.type < 2) {
        ita_p[0].x = ita_p[1].x = 0.0f;
        ita_p[2].x = ita_p[3].x = 384.0f;

        if (ewk->wu.type == 0) {
            ita_p[0].y = ita_p[3].y = 175.0f;
            ita_p[1].y = ita_p[2].y = 224.0f;
        } else {
            ita_p[0].y = ita_p[3].y = 0.0f;
            ita_p[1].y = ita_p[2].y = 33.0f;
        }
    } else {
        ita_p[0].y = ita_p[2].y = 0.0f;
        ita_p[1].y = ita_p[3].y = 224.0f;

        if (ewk->wu.type == 2) {
            ita_p[0].x = ita_p[1].x = 0.0f;
            ita_p[2].x = ita_p[3].x = 1.0f;
        } else {
            ita_p[0].x = ita_p[1].x = 384.0f;
            ita_p[2].x = ita_p[3].x = 385.0f;
        }
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;

        if (!No_Trans) {
            ita_pos[0] = ita_p[0];
            ita_pos[1] = ita_p[3];
            ita_pos[2] = ita_p[1];
            ita_pos[3] = ita_p[2];
            Renderer_Queue2DPrimitive((f32*)ita.p, prio, (uintptr_t)ita.col[0].color, 0);
        }

        break;

    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.routine_no[0] = 3;
            ewk->wu.disp_flag = 0;
            break;
        }

        if (g_state.End_PL == 14 && ewk->wu.type < 2) {
            if (ewk->wu.type) {
                ita_p[0].y = ita_p[3].y = 0.0f;
                ita_p[1].y = ita_p[2].y = (33.0f - ((33.0f * g_state.scr_sc) - 33.0f));
            } else {
                ita_p[0].y = ita_p[3].y = (224.0f - (1.0f + (48.0f - ((48.0f * g_state.scr_sc) - 48.0f))));
                ita_p[1].y = ita_p[2].y = 224.0f;
            }
        }

        if (g_state.end_w.r_no_0 >= 6) {
            ewk->wu.routine_no[0]++;
        }

        if (!No_Trans) {
            ita_pos[0] = ita_p[0];
            ita_pos[1] = ita_p[3];
            ita_pos[2] = ita_p[1];
            ita_pos[3] = ita_p[2];
            Renderer_Queue2DPrimitive((f32*)ita.p, prio, (uintptr_t)ita.col[0].color, 0);
            break;
        }

        break;

    case 2:
        if (ewk->wu.type) {
            ita_p[0].y = ita_p[3].y = 0.0f;
            ita_p[1].y = ita_p[2].y = 16.0f;
        } else {
            ita_p[0].y = ita_p[3].y = 207.0f;
            ita_p[1].y = ita_p[2].y = 224.0f;
        }

        if (!No_Trans) {
            ita_pos[0] = ita_p[0];
            ita_pos[1] = ita_p[3];
            ita_pos[2] = ita_p[1];
            ita_pos[3] = ita_p[2];
            Renderer_Queue2DPrimitive((f32*)ita.p, prio, (uintptr_t)ita.col[0].color, 0);
        }

        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_E9_init() {
    State_Other* ewk;
    s16 ix;
    s16 i;

    for (i = 0; i < 4; i++) {

        if ((ix = Acquire_Effect(4)) == -1) {
            return -1;
        }
        ewk = (State_Other*)frw[ix];
        ewk->wu.id = 149;
        ewk->wu.active_flag = 1;
        ewk->wu.type = i;
        ewk->wu.work_id = 16;
        ewk->wu.graphic_rom_type = 1;
        ewk->wu.my_col_mode = 0x4200;
        ewk->wu.my_family = 4;
        ewk->wu.my_priority = 19;
    }

    return 0;
}
