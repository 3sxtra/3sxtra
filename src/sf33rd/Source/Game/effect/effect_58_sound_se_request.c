/**
 * @file eff58.c
 * Effect: Sound / SE Request Effect
 */

#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

static s32 SF33rd_Logo(State_Other* ewk);
static void EFF58_Type_01(State_Other* ewk);
static void Fade_In_58_Sub(State_Other* ewk);

void effect_58_move(State_Other* ewk) {
    s16 xx;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.routine_no[0]++;
        }

        break;

    case 1:
        switch (ewk->wu.routine_no[1]) {
        case 0:
            g_state.bg_w.bgw[ewk->wu.direction].wxy[0].cal += g_state.bg_w.bgw[ewk->wu.direction].speed_x;

            if (0 < g_state.bg_w.bgw[ewk->wu.direction].speed_x) {
                if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] <=
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                    g_state.Next_Step |= 1;
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                        g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                    ewk->wu.routine_no[0]++;
                }
            } else if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] >=
                       g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                g_state.Next_Step |= 1;
                g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                    g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                ewk->wu.routine_no[0]++;
            }
            break;

        case 1:
            EFF58_Type_01(ewk);
            break;

        case 4:
            Fade_In_58_Sub(ewk);
            break;

        case 6:
            if (g_state.Demo_Flag != 0) {
                SsRequest(ewk->wu.direction);
            }

            ewk->wu.routine_no[0]++;
            break;

        case 7:
            g_state.Next_Step = 1;
            ewk->wu.routine_no[0]++;
            break;

        case 8:
            if (g_state.Demo_Flag != 0 && g_state.PB_Music_Off == 0) {
                BGM_Request(ewk->wu.direction);
            }

            ewk->wu.routine_no[0]++;
            break;

        case 9:
            if (g_state.Demo_Flag != 0 && g_state.PB_Music_Off == 0) {
                SsBgmFadeIn(ewk->wu.direction, 0x222);
            }

            ewk->wu.routine_no[0]++;
            break;

        case 10:
            SF3_logo(SF33rd_Logo(ewk));
            break;

        case 12:
            if (g_state.Cut_Scroll == 0) {
                xx = 3;
            } else {
                xx = Cut_Cut_Sub(3);
            }

            g_state.bg_w.bgw[ewk->wu.direction].wxy[0].cal += g_state.bg_mvxy.a[0].sp * xx;
            g_state.bg_mvxy.a[0].sp += g_state.bg_mvxy.d[0].sp;

            if (0 < g_state.bg_mvxy.a[0].sp) {
                if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] <=
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                    g_state.Next_Step |= 1;
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                        g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                    ewk->wu.routine_no[0]++;
                }
            } else if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] >=
                       g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                g_state.Next_Step |= 1;
                g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                    g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                ewk->wu.routine_no[0]++;
            }

            break;

        case 13:
            if (g_state.Cut_Scroll == 0) {
                xx = 5;
            } else {
                xx = Cut_Cut_Sub(5);
            }

            g_state.bg_w.bgw[ewk->wu.direction].wxy[0].cal += g_state.bg_mvxy.a[0].sp * xx;
            g_state.bg_mvxy.a[0].sp += g_state.bg_mvxy.d[0].sp;

            if (0 < g_state.bg_mvxy.a[0].sp) {
                if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] <=
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                    g_state.Next_Step |= 1;
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                        g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                    ewk->wu.routine_no[0]++;
                }
            } else if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] >=
                       g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                g_state.Next_Step |= 1;
                g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                    g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                ewk->wu.routine_no[0]++;
            }

            break;

        case 14:
            g_state.bg_w.bgw[ewk->wu.direction].wxy[0].cal += g_state.bg_mvxy.a[0].sp;
            g_state.bg_mvxy.a[0].sp += g_state.bg_mvxy.d[0].sp;

            if (0 < g_state.bg_mvxy.a[0].sp) {
                if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] <=
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                    g_state.Next_Step |= 1;
                    g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                        g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                    ewk->wu.routine_no[0]++;
                }
            } else if (g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction] >=
                       g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos) {
                g_state.Next_Step |= 1;
                g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos =
                    g_state.Target_BG_X[ewk->wu.direction] + g_state.Offset_BG_X[ewk->wu.direction];
                ewk->wu.routine_no[0]++;
            }

            break;

        case 15:
            g_state.Suicide[ewk->wu.direction] = 1;
            ewk->wu.routine_no[0]++;
            break;

        case 16:
            g_state.bg_w.bgw[ewk->wu.direction].wxy[1].disp.pos += 256;
            Setup_BG(ewk->wu.direction,
                     g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos,
                     g_state.bg_w.bgw[ewk->wu.direction].wxy[1].disp.pos);
            ewk->wu.routine_no[0]++;
            break;

        case 17:
            g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos += 512;
            Setup_BG(ewk->wu.direction,
                     g_state.bg_w.bgw[ewk->wu.direction].wxy[0].disp.pos,
                     g_state.bg_w.bgw[ewk->wu.direction].wxy[1].disp.pos);
            ewk->wu.routine_no[0]++;
            break;

        case 18:
            Setup_BG(2, 0x480, 0);
            ewk->wu.routine_no[0]++;
            break;
        }

        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_58_init(s16 id, s16 time0, s16 option) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 58;
    ewk->wu.work_id = 16;
    ewk->wu.dir_timer = time0;
    ewk->wu.dir_old = time0;
    ewk->wu.direction = option;
    ewk->wu.routine_no[1] = id;
    return 0;
}

static s32 SF33rd_Logo(State_Other* ewk) {
    switch (ewk->wu.routine_no[2]) {
    case 0:
        ewk->wu.routine_no[2]++;
        ewk->wu.dir_timer = 1;
        g_state.Disappear_LOGO = 0;
        /* fallthrough */

    case 1:
        if (--ewk->wu.dir_timer != 0) {
            break;
        }

        ewk->wu.dir_timer = 2;
        ewk->wu.direction++;

        if (ewk->wu.direction >= 8) {
            ewk->wu.direction = 8;
            ewk->wu.routine_no[2]++;
        }

        break;

    case 2:
        if (g_state.Disappear_LOGO) {
            ewk->wu.routine_no[2]++;
            ewk->wu.dir_timer = 1;
        }

        break;

    default:
        if (--ewk->wu.dir_timer != 0) {
            break;
        }

        ewk->wu.dir_timer = 2;
        ewk->wu.direction++;

        if (ewk->wu.direction > 16) {
            ewk->wu.direction = 16;
            sort_push_request4(&ewk->wu);
        }

        break;
    }

    return ewk->wu.direction;
}

static void EFF58_Type_01(State_Other* ewk) {
    switch (ewk->wu.routine_no[2]) {
    case 0:
        Switch_Screen(1);

        if (!--g_state.Cover_Timer) {
            ewk->wu.routine_no[2]++;
            Switch_Screen_Init(1);
        }

        break;

    case 1:
        if (Switch_Screen_Revival(1)) {
            ewk->wu.routine_no[0] = 99;
        }

        break;
    }
}

static void Fade_In_58_Sub(State_Other* ewk) {
    switch (ewk->wu.routine_no[2]) {
    case 0:
        ewk->wu.routine_no[2]++;
        ewk->wu.dir_timer = 10;
        ewk->wu.dir_old = 0xFF;
        /* fallthrough */

    case 1:
        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.routine_no[2]++;
            ewk->wu.dir_timer = 4;
        }

        ToneDown(ewk->wu.dir_old, ewk->wu.direction);
        break;

    default:
        if (ewk->wu.dir_old < 0) {
            ewk->wu.dir_old = 0;
            ewk->wu.routine_no[0]++;
        }

        ToneDown(ewk->wu.dir_old, ewk->wu.direction);

        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.dir_old -= 16;
            ewk->wu.dir_timer = 4;
        }

        break;
    }
}
