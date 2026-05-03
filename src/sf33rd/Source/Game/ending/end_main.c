/**
 * @file end_main.c
 * Ending Movie Controller
 */

#include "sf33rd/Source/Game/ending/end_main.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_e6_ending_scene_gill_general.h"
#include "sf33rd/Source/Game/effect/effect_e9_ending_renderer.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_f9_text_message.h"
#include "sf33rd/Source/Game/ending/ending_00_gill.h"
#include "sf33rd/Source/Game/ending/ending_01_alex.h"
#include "sf33rd/Source/Game/ending/ending_02_ryu.h"
#include "sf33rd/Source/Game/ending/ending_03_yun.h"
#include "sf33rd/Source/Game/ending/ending_04_dudley.h"
#include "sf33rd/Source/Game/ending/ending_05_necro.h"
#include "sf33rd/Source/Game/ending/ending_06_hugo.h"
#include "sf33rd/Source/Game/ending/ending_07_ibuki.h"
#include "sf33rd/Source/Game/ending/ending_08_elena.h"
#include "sf33rd/Source/Game/ending/ending_09_oro.h"
#include "sf33rd/Source/Game/ending/ending_10_yang.h"
#include "sf33rd/Source/Game/ending/ending_11_ken.h"
#include "sf33rd/Source/Game/ending/ending_12_sean.h"
#include "sf33rd/Source/Game/ending/ending_13_urien.h"
#include "sf33rd/Source/Game/ending/ending_14_akuma.h"
#include "sf33rd/Source/Game/ending/ending_16_chun_li.h"
#include "sf33rd/Source/Game/ending/ending_17_makoto.h"
#include "sf33rd/Source/Game/ending/ending_18_q.h"
#include "sf33rd/Source/Game/ending/ending_19_twelve.h"
#include "sf33rd/Source/Game/ending/ending_20_remy.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/screen/name_input.h"
#include "sf33rd/Source/Game/screen/staff.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

static void normal_ending(s16 pl_num);
static void end_main_move(s16 pl_num);
static void end_reset_etc();
static void end_fade_bgm();
static s32 Cut_Cut_Cut_t();

#define END_MAIN_COUNT 20
#define ENDING_SKIP_BUTTON_MASK 0xFF0

static void (*end_main_jp[20])(s16) = { end_00000, end_01000, end_02000, end_03000, end_04000, end_05000, end_06000,
                                        end_07000, end_08000, end_09000, end_10000, end_11000, end_12000, end_13000,
                                        end_14000, end_16000, end_17000, end_18000, end_19000, end_20000 };

/** @brief Initialize ending sequence state variables. */
void Ending_init() {
    g_state.end_w.r_no_0 = 0;
    g_state.end_w.r_no_1 = 0;
    g_state.end_w.r_no_2 = 0;
    g_state.end_w.type = 0;
    g_state.end_w.end_flag = 0;
    g_state.Game_timer = 0;
    ending_all_end = 0;
    end_fade_timer = 0;
    end_fade_flag = 0;
    end_no_cut = 0;
    staff_r_no = 0;
    end_staff_flag = 0;
}

/** @brief Main ending entry point — advances the ending sequence each frame. */
s8 Ending_main(s16 pl_num) {
    g_state.Game_timer++;
    normal_ending(pl_num);
    return ending_all_end;
}

/** @brief State machine for the normal ending flow (intro → scenes → staff roll → fade). */
static void normal_ending(s16 pl_num) {
    switch (g_state.end_w.r_no_0) {
    case 0:
        g_state.end_w.r_no_0++;
        Switch_Screen(1);
        Bg_Off_R(7);
        System_all_clear_Level_B();
        g_state.Cover_Timer = 29;
        if (pl_num < 0 || pl_num >= END_MAIN_COUNT)
            break;
        end_main_jp[pl_num](pl_num);
        break;

    case 1:
        Switch_Screen(1);

        if (!(g_state.Cover_Timer -= 1)) {
            g_state.end_w.r_no_0++;
            Switch_Screen_Init(1);
            if (pl_num < 0 || pl_num >= END_MAIN_COUNT)
                break;
            end_main_jp[pl_num](pl_num);
        }

        break;

    case 2:
        if (Switch_Screen_Revival(1)) {
            g_state.end_w.r_no_0++;
            g_state.Ignore_Entry[g_state.LOSER] = 0;
            g_state.Forbid_Break = -1;
        }

        break;

    case 3:
        end_main_move(pl_num);

        if (g_state.end_w.end_flag) {
            g_state.end_w.r_no_0++;
            end_no_cut = 1;
            effect_work_kill(4, 0x9F);
            SsBgmFadeOut(0x111);
        } else if (Cut_Cut_Cut_t()) {
            g_state.end_w.timer = 0;
            effect_work_kill(4, 0x9F);
            end_main_move(pl_num);
            end_fade_flag = 0;
        }

        g_state.Forbid_Break = -1;
        break;

    case 4:
        if (end_no_cut == 0) {
            fadeout_to_staff_roll();
        } else {
            g_state.end_w.r_no_0++;
        }

        g_state.Forbid_Break = -1;
        break;

    case 5:
        if (end_fade_complete()) {
            g_state.end_w.r_no_0++;
            g_state.end_w.r_no_2++;
            overwrite_panel(0xFF000000, 0x12);

            if (g_state.end_w.type == 4) {
                Zoomf_Init();
            }

            g_state.end_w.r_no_0++;
            end_no_cut = 1;
            g_state.bg_w.bgw[0].xy[0].disp.pos = 256;
            g_state.bg_w.bgw[0].abs_x = 512;
            g_state.bg_w.bgw[0].xy[1].disp.pos = 0;
            g_state.bg_w.bgw[0].abs_y = 0;
            end_scn_pos_set2();
            end_bg_pos_adjust2();
            end_fam_set2();
            Bg_Off_W(0xF);
        }

        g_state.Forbid_Break = -1;
        break;

    case 6:
        overwrite_panel(0xFF000000, 0x12);
        g_state.end_w.r_no_0++;
        break;

    case 7:
        g_state.end_w.r_no_0++;
        overwrite_panel(0xFF000000, 0x12);
        Request_Fade(6);
        end_no_cut = 1;
        g_state.Forbid_Break = -1;
        break;

    case 8:
        if (end_fade_complete()) {
            g_state.end_w.r_no_0++;
            end_no_cut = 0;
            Bg_Close();
        }

        g_state.Forbid_Break = -1;
        break;

    case 9:
        scr_calc(5);

        if (staff_credits(end_staff_flag)) {
            g_state.end_w.r_no_0++;
        }

        break;

    case 10:
        g_state.end_w.r_no_0++;

        if (name_wk[g_state.WINNER].timer >= 1) {
            name_wk[g_state.WINNER].timer = 0;
            end_name_cut[g_state.WINNER] = 1;
            g_state.end_w.timer = 180;

            if (bgm_play_status() == 2) {
                SsBgmFadeOut(0xB6);
            }

            break;
        }

        g_state.end_w.timer = 60;

        if (bgm_play_status() == 2) {
            SsBgmFadeOut(0x222);
        }

        break;

    case 11:
        g_state.end_w.timer--;

        if (g_state.end_w.timer < 0) {
            g_state.end_w.r_no_0++;
            ending_all_end = 1;
        }

        break;

    case 12:
        ending_all_end = 1;
        break;
    }
}

/** @brief Tick the per-character ending handler and process BGM fade. */
static void end_main_move(s16 pl_num) {
    if (pl_num < 0 || pl_num >= END_MAIN_COUNT)
        return;
    end_main_jp[pl_num](pl_num);
    end_fade_bgm();
}

/** @brief Request a fade-out transition before the staff roll begins. */
void fadeout_to_staff_roll() {
    Request_Fade(7);
    end_no_cut = 1;
    g_state.Forbid_Break = -1;
}

/** @brief Common ending initialization — load textures, set up backgrounds for the given character. */
void common_end_init00(s16 pl_num) {
    s16 i;

    Family_Init();
    Scrn_Pos_Init();
    Zoomf_Init();
    if (pl_num < 0 || pl_num >= END_MAIN_COUNT)
        return;
    g_state.bg_w.bg_opaque = ending_opaque[pl_num];
    g_state.Screen_Switch = 0;
    g_state.Screen_Switch_Buffer = 0;
    g_state.bg_disp_off = 0;
    g_state.end_w.type = pl_num;
    g_state.bg_w.scno = end_use_scr[g_state.end_w.type];
    g_state.bg_w.scrno = end_use_real_scr[g_state.end_w.type];
    Bg_Texture_Load_Ending(g_state.end_w.type);
    ending_obj_load();
    g_state.bg_w.pos_offset = 192;
    g_state.base_y_pos = 40;

    for (i = 0; i < 7; i++) {
        g_state.bg_w.bgw[i].r_no_0 = 0;
        g_state.bg_w.bgw[i].r_no_1 = 0;
        g_state.bg_w.bgw[i].r_no_2 = 0;
        g_state.bg_w.bgw[i].pos_x_work = g_state.bg_w.bgw[i].pos_y_work = 0;
        g_state.bg_w.bgw[i].zuubun = 0;
        g_state.bg_w.bgw[i].xy[0].cal = 0;
        g_state.bg_w.bgw[i].xy[1].cal = 0;
        g_state.bg_w.bgw[i].wxy[0].cal = 0;
        g_state.bg_w.bgw[i].wxy[1].cal = 0;
        g_state.bg_w.bgw[i].hos_xy[0].cal = 0;
        g_state.bg_w.bgw[i].hos_xy[1].cal = 0;
        g_state.bg_w.bgw[i].rewrite_flag = 0;
    }

    for (i = 0; i < g_state.bg_w.scno; i++) {
        g_state.bg_w.bgw[i].r_no_1 = g_state.bg_w.bgw[i].r_no_2 = 0;
        g_state.bg_w.bgw[i].fam_no = i;
    }
}

/** @brief Second-stage common ending init — reset scroll, quake, and set up message layer. */
void common_end_init01() {
    g_state.bg_w.scr_stop = 0;
    g_state.bg_w.frame_flag = 0;
    g_state.bg_w.bg_f_x = 9;
    g_state.bg_w.bg_f_y = 9;
    g_state.bg_w.bg2_sp_x2 = g_state.bg_w.bg2_sp_x = 0;
    g_state.bg_w.max_x = 6;
    g_state.bg_w.old_chase_flag = g_state.bg_w.chase_flag = 0;
    g_state.bg_w.quake_x_index = 0;
    g_state.bg_w.quake_y_index = 0;
    end_etc_flag = 0;
    end_reset_etc();
    g_state.bg_w.bgw[3].wxy[0].cal = g_state.bg_w.bgw[3].xy[0].cal = 0x2000000;
    g_state.bg_w.bgw[3].xy[1].cal = 0;
    g_state.bg_w.bgw[3].position_x = 512 - g_state.bg_w.pos_offset;
    g_state.bg_w.bgw[3].position_y = 0;
    end_fam_set(3);
    Scrn_Move_Set(3, g_state.bg_w.bgw[3].position_x, g_state.bg_w.bgw[3].position_y);
    effect_E9_init();
    effect_F9_init(g_state.end_w.type);
}

/** @brief Update family (parallax layer) position for a single background layer. */
void end_fam_set(s16 i) {
    s16 pos_work_x = g_state.bg_w.bgw[i].position_x;
    s16 pos_work_y = g_state.bg_w.bgw[i].position_y;

    pos_work_x = pos_work_x & 0xFFFF;
    pos_work_y = (pos_work_y + 16) & 0xFFFF;

    Family_Set_W(i + 1, pos_work_x, pos_work_y);
}

/** @brief Update family positions for all active background layers. */
void end_fam_set2() {
    s16 i;

    for (i = 0; i < g_state.bg_w.scno; i++) {
        end_fam_set(i);
    }
}

/** @brief Apply position offset correction to a single background layer. */
void end_bg_pos_adjust(s16 bg_no) {
    s16 pos_work = g_state.bg_w.bgw[bg_no].abs_x & 0xFFFF;
    pos_work -= g_state.bg_w.pos_offset;
    g_state.bg_w.bgw[bg_no].position_x = pos_work & 0xFFFF;
    pos_work = g_state.bg_w.bgw[bg_no].abs_y & 0xFFFF;
    g_state.bg_w.bgw[bg_no].position_y = pos_work;
}

/** @brief Apply position offset correction to all active background layers. */
void end_bg_pos_adjust2() {
    s16 bg_no;

    for (bg_no = 0; bg_no < g_state.bg_w.scno; bg_no++) {
        end_bg_pos_adjust(bg_no);
    }
}

/** @brief Commit scroll positions for all active ending screens. */
void end_scn_pos_set2() {
    s16 bg_no;

    for (bg_no = 0; bg_no < g_state.bg_w.scno; bg_no++) {
        Scrn_Move_Set(g_state.bg_w.bgw[bg_no].fam_no,
                      (g_state.bg_w.bgw[bg_no].xy[0].disp.pos & 0xFFFF) - g_state.bg_w.pos_offset,
                      g_state.bg_w.bgw[bg_no].xy[1].disp.pos);
        g_state.bg_w.bgw[bg_no].wxy[0].cal = g_state.bg_w.bgw[bg_no].xy[0].cal;
        g_state.bg_w.bgw[bg_no].wxy[1].cal = g_state.bg_w.bgw[bg_no].xy[1].cal;
    }
}

/** @brief Reset sub-state and positions for all active background layers. */
static void end_reset_etc() {
    s16 i;

    for (i = 0; i < g_state.bg_w.scno; i++) {
        g_state.bg_w.bgw[i].r_no_1 = 0;
        g_state.bg_w.bgw[i].abs_x = g_state.bg_w.bgw[i].xy[0].disp.pos = 512;
        g_state.bg_w.bgw[i].abs_y = g_state.bg_w.bgw[i].xy[1].disp.pos = 0;
    }
}

/** @brief Common ending cut command — turn off the current background layer. */
void end_X_com01() {
    switch (bgw_ptr->r_no_1) {
    case 0:
        bgw_ptr->r_no_1++;
        Bg_Off_W(1 << bgw_ptr->fam_no);
        break;

    case 1:
        break;
    }
}

/** @brief Count down the BGM fade timer each frame. */
static void end_fade_bgm() {
    if (end_fade_flag != 0) {
        end_fade_timer -= 1;

        if (end_fade_timer < 0) {
            end_fade_flag = 0;
        }
    }
}

/** @brief Check whether the current screen fade has completed. */
s16 end_fade_complete() {
    if (Check_Fade_Complete()) {
        g_state.Forbid_Break = -1;
        return 1;
    }

    return 0;
}

/** @brief Check if the player pressed a button to skip the current ending cut. */
static s32 Cut_Cut_Cut_t() {
    u16 sw_w;

    if (end_no_cut == 0) {
        if (g_state.WINNER) {
            sw_w = p2sw_0 & ~p2sw_1;
        } else {
            sw_w = p1sw_0 & ~p1sw_1;
        }

        if (sw_w & ENDING_SKIP_BUTTON_MASK) {
            return 1;
        }
    }

    return 0;
}
