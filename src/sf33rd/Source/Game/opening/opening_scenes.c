/**
 * @file opening_scenes.c
 * @brief Opening scene handlers — 19 timed scenes synchronised to the soundtrack.
 *
 * Each scene handler (`op_100_move`..`op_118_move`) manages one segment of
 * the ~30-second opening cinematic, driving effect initialisation, BG layer
 * transitions, and sound-cue synchronisation.
 *
 * Part of the opening module.  Split from opening.c for maintainability.
 */

#include "sf33rd/Source/Game/opening/opening.h"
#include "game_state.h"
#include "common.h"

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_copyright.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_title_screen.h"
#include <stdbool.h>

#include "port/config/config.h"
#include "port/rendering/renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo00.h"
#include "sf33rd/Source/Game/effect/eff36.h"
#include "sf33rd/Source/Game/effect/eff48.h"
#include "sf33rd/Source/Game/effect/effe1.h"
#include "sf33rd/Source/Game/effect/efff5.h"
#include "sf33rd/Source/Game/effect/efff6.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/opening/op_sub.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

const s16 op_change_sound_tbl[OP_CHANGE_SOUND_COUNT] = { 101, 102, 103, 104, 105, 106, 107, 108, 109,
                                                         110, 111, 112, 113, 114, 115, 116, 117, 16 };

const s16 op_quake_y_tbl0[OP_QUAKE_Y_COUNT] = { 4, -8, 2, 1, -6, -3, 9, -3, 8, -2, 6, 3, -4, -9, 3, -1 };

static void op_100_move();
static void op_101_move();
static void op_102_move();
static void op_103_move();
static void op_104_move();
static void op_105_move();
static void op_106_move();
static void op_107_move();
static void op_108_move();
static void op_109_move();
static void op_110_move();
static void op_111_move();
static void op_112_move();
static void op_113_move();
static void op_114_move();
static void op_115_move();
static void op_116_move();
static void op_117_move();
static void op_118_move();
static void opening_title_01();

static void (*opening_move_jp[OPENING_SCENE_COUNT])() = { op_100_move, op_101_move, op_102_move, op_103_move,
                                                          op_104_move, op_105_move, op_106_move, op_107_move,
                                                          op_108_move, op_109_move, op_110_move, op_111_move,
                                                          op_112_move, op_113_move, op_114_move, op_115_move,
                                                          op_116_move, op_117_move, op_118_move };

/** @brief Main opening move loop — track sound cues and dispatch scene handlers. */
void opening_move() {
    s16 work2;

    if (Debug_w[DEBUG_OPENING_TEST]) {
        flPrintColor(0xFFFFFF8F);
        flPrintL(2, 1, "BAR %d", op_w.r_no_1);
    }

    op_plmove_timer += 1;
    sound_trg_move();

    if (op_w.r_no_1 < OP_CHANGE_SOUND_COUNT) {
        work2 = gSeqStatus[0];

        if (op_sound_status != work2) {
            if (work2 == op_change_sound_tbl[op_w.r_no_1]) {
                op_w.r_no_1 += 1;
                op_w.r_no_2 = 0;
                op_work_clear();
            }
        }
    }

    if (op_w.r_no_1 < 0 || op_w.r_no_1 >= OPENING_SCENE_COUNT) {
        return;
    }

    opening_move_jp[op_w.r_no_1]();
    op_sound_status = gSeqStatus[0];
}

/** @brief Scene 100 — start BGM and immediately chain into scene 101. */
static void op_100_move() {
    op_w.r_no_1 += 1;
    Go_BGM();
    Disp_Sound_Code();
    op_101_move();
}

const s16 op_101_sound[2] = { 0, 11 };

/** @brief Scene 101 — first fight scene (fade in, effect init, BG scroll). */
static void op_101_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        ToneDown(0xFF, 0);
        op_bg_move(0);
        effect_F6_init(0);
        effect_F6_init(1);
        op_obj_disp = 0;
        effect_48_init(0);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_101_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x65)) {
            op_w.r_no_2 += 1;
            op_w.index = 1;
            op_obj_disp = 1;
            op_work_clear();
            break;
        }

        op_bg_move(0);
        break;

    default:
        op_bg_move(1);
        break;
    }
}

const s16 op_102_sound[3] = { 0, 9, 12 };

/** @brief Scene 102 — second fight scene (multi-phase with three sound cues). */
static void op_102_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_w.index = 2;
        effect_F6_init(2);
        effect_F6_init(3);
        effect_F6_init(4);
        op_obj_disp = 0;
        effect_48_init(1);
        op_work_clear();
        op_bg_move(2);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_102_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x66)) {
            op_w.r_no_2 += 1;
            op_w.index = 3;
            op_obj_disp = 1;
            op_work_clear();
            op_bg_move(3);
            return;
        }

        op_bg_move(2);
        break;

    case 2:
        if (gSeqStatus[0] >= op_102_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_w.index = 4;
            op_work_clear();
            op_bg_move(4);
            return;
        }

        op_bg_move(3);
        break;

    default:
        op_bg_move(4);
        break;
    }
}

const s16 op_103_sound[12] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13 };

/** @brief Scene 103 — extended multi-phase scene (12 sound cues, 13 sub-phases). */
static void op_103_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_w.index = 5;
        effect_F6_init(5);
        effect_F6_init(6);
        effect_F6_init(7);
        effect_F6_init(8);
        effect_F6_init(9);
        effect_F6_init(10);
        op_work_clear();
        op_bg_move(5);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x67)) {
            op_w.r_no_2 += 1;
            op_w.index = 6;
            op_work_clear();
            effect_F6_init(11);
            effect_F6_init(12);
            effect_F6_init(13);
            effect_F6_init(14);
            effect_F6_init(15);
            effect_F6_init(16);
            return;
        }

        op_bg_move(5);
        break;

    case 2:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 7;
            op_obj_disp = 0;
            effect_48_init(8);
            return;
        }

        op_bg_move(6);
        break;

    case 3:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 8;
            op_obj_disp = 1;
            op_scrn_end = 0;
            effect_36_init(22);
            return;
        }

        op_bg_move(7);
        break;

    case 4:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 9;
            return;
        }

        op_bg_move(8);
        break;

    case 5:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 10;
            return;
        }

        op_bg_move(9);
        break;

    case 6:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 11;
            return;
        }

        op_bg_move(10);
        break;

    case 7:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 12;
            op_obj_disp = 0;
            effect_48_init(9);
            return;
        }

        op_bg_move(11);
        break;

    case 8:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 13;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(12);
        break;

    case 9:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 14;
            op_scrn_end = 0;
            effect_36_init(23);
            return;
        }

        op_bg_move(13);
        break;

    case 10:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 15;
        }

        op_bg_move(14);
        break;

    case 11:
        if (gSeqStatus[0] >= op_103_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 16;
            return;
        }

        op_bg_move(15);
        break;

    default:
        op_bg_move(16);
        break;
    }
}

s16 op_104_sound[7] = { 0, 5, 6, 7, 9, 10, 11 };

/** @brief Scene 104 — multi-phase scene (7 sound cues). */
static void op_104_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 17;
        op_bg_move(17);
        effect_F6_init(17);
        effect_F6_init(18);
        effect_F6_init(19);
        effect_F6_init(20);
        effect_F6_init(21);
        effect_F6_init(22);
        effect_F6_init(23);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x68)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 18;
            return;
        }

        op_bg_move(17);
        break;

    case 2:
        if (gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 19;
            op_obj_disp = 0;
            effect_48_init(10);
            return;
        }

        op_bg_move(18);
        break;

    case 3:
        if (gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 20;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(19);
        break;

    case 4:
        if (gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 21;
            op_obj_disp = 0;
            effect_48_init(11);
            return;
        }

        op_bg_move(20);
        break;

    case 5:
        if (gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 22;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(21);
        break;

    case 6:
        if (gSeqStatus[0] >= op_104_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 23;
            return;
        }

        op_bg_move(22);
        break;

    case 7:
    default:
        op_bg_move(23);
        break;
    }
}

/** @brief Scene 105 — short transitional scene. */
static void op_105_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 24;
        op_bg_move(24);
        effect_F6_init(24);
        op_obj_disp = 0;
        effect_48_init(12);
        break;

    case 1:
    default:
        op_bg_move(24);
        break;
    }
}

const s16 op_106_sound[4] = { 0, 1, 3, 7 };

/** @brief Scene 106 — scene with 4 sound-triggered sub-phases. */
static void op_106_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 25;
        op_obj_disp = 1;
        op_bg_move(25);
        effect_F6_init(25);
        effect_F6_init(26);
        effect_F6_init(27);
        effect_F6_init(28);
        effect_36_init(18);
        effect_36_init(19);
        effect_36_init(20);
        effect_36_init(21);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_106_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x6A)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 26;
            return;
        }

        op_bg_move(25);
        break;

    case 2:
        if (gSeqStatus[0] >= op_106_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 27;
            return;
        }

        op_bg_move(26);
        break;

    case 3:
        if (gSeqStatus[0] >= op_106_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 28;
            op_obj_disp = 0;
            effect_48_init(15);
            return;
        }

        op_bg_move(27);
        break;

    default:
        op_bg_move(28);
        break;
    }
}

const s16 op_107_sound[12] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13 };

/** @brief Scene 107 — extended scene (12 sound cues, multi-phase BG animation). */
static void op_107_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_plmove_timer += 1;
        op_obj_disp = 1;
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 29;
        op_bg_move(29);
        effect_F6_init(29);
        effect_F6_init(30);
        effect_F6_init(31);
        effect_F6_init(32);
        effect_F6_init(33);
        effect_F6_init(34);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x6B)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 30;
            return;
        }

        op_bg_move(29);
        break;

    case 2:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 31;
            return;
        }

        op_bg_move(30);
        break;

    case 3:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 32;
            op_obj_disp = 0;
            effect_48_init(13);
            return;
        }

        op_bg_move(31);
        break;

    case 4:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 33;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(32);
        break;

    case 5:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 34;
            effect_F6_init(35);
            effect_F6_init(36);
            effect_F6_init(37);
            effect_F6_init(38);
            effect_F6_init(39);
            effect_F6_init(40);
            return;
        }

        op_bg_move(33);
        break;

    case 6:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 35;
            return;
        }

        op_bg_move(34);
        return;

    case 7:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 36;
            op_obj_disp = 0;
            effect_48_init(14);
            return;
        }

        op_bg_move(35);
        break;

    case 8:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 37;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(36);
        break;

    case 9:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 38;
            return;
        }

        op_bg_move(37);
        break;

    case 10:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 39;
            return;
        }

        op_bg_move(38);
        break;

    case 11:
        if (gSeqStatus[0] >= op_107_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 40;
            return;
        }

        op_bg_move(39);
        break;

    default:
        op_bg_move(40);
        break;
    }
}

const s16 op_108_sound[13] = { 0, 4, 20, 24, 28, 32, 48, 52, 60, 64, 76, 80, 86 };

/** @brief Scene 108 — long scene with 13 sound-triggered sub-phases. */
static void op_108_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 41;
        op_bg_move(41);
        op_w.mv_ctr = 0;
        effect_36_init(0);
        effect_36_init(1);
        effect_36_init(2);
        effect_36_init(3);
        effect_36_init(4);
        effect_36_init(5);
        effect_36_init(6);
        effect_36_init(7);
        break;

    case 1:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 42;
            op_bg_move(42);
            return;
        }

        op_bg_move(41);
        break;

    case 2:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 43;
            op_bg_move(43);
            effect_36_init(8);
            effect_36_init(9);
            effect_36_init(10);
            effect_36_init(11);
            effect_36_init(12);
            effect_36_init(13);
            effect_36_init(14);
            effect_36_init(15);
            return;
        }

        break;

    default:
        break;

    case 3:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 44;
            op_bg_move(44);
            return;
        }

        op_bg_move(43);
        break;

    case 4:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 45;
            op_bg_move(45);
            return;
        }

        break;

    case 5:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 46;
            op_bg_move(46);
            return;
        }

        op_bg_move(45);
        break;

    case 6:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 47;
            op_bg_move(47);
            return;
        }

        break;

    case 7:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 48;
            op_bg_move(48);
            return;
        }

        op_bg_move(47);
        break;

    case 8:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 49;
            op_bg_move(49);
            return;
        }

        break;

    case 9:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 50;
            op_bg_move(50);
            return;
        }

        op_bg_move(49);
        break;

    case 10:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 51;
            op_bg_move(51);
            return;
        }

        break;

    case 11:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 52;
            op_bg_move(52);
            return;
        }

        op_bg_move(51);
        break;

    case 12:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_108_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 53;
            op_bg_move(53);
        }

        break;
    }
}

const s16 op_109_sound[5] = { 0, 3, 5, 7, 11 };

/** @brief Scene 109 — scene with 5 sound-triggered sub-phases. */
static void op_109_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_scrn_end = 0;
        op_work_clear();
        op_w.index = 54;
        op_bg_move(54);
        op_obj_disp = 0;
        effect_48_init(16);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_109_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x6D)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 55;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(54);
        break;

    case 2:
        if (gSeqStatus[0] >= op_109_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 56;
            return;
        }

        op_bg_move(55);
        break;

    case 3:
        if (gSeqStatus[0] >= op_109_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_scrn_end = 0;
            op_work_clear();
            op_w.index = 57;
            op_obj_disp = 0;
            effect_48_init(17);
            return;
        }

        op_bg_move(56);
        break;

    case 4:
        if (gSeqStatus[0] >= op_109_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 58;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(57);
        break;

    default:
        op_bg_move(58);
        break;
    }
}

const s16 op_110_sound[6] = { 0, 0, 3, 4, 7, 9 };

/** @brief Scene 110 — scene with 6 sound-triggered sub-phases. */
static void op_110_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        Zoom_Value_Set(64);
        op_work_clear();
        op_w.index = 59;
        op_bg_move(59);
        op_obj_disp = 0;
        effect_48_init(2);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_110_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x6E)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 60;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(59);
        break;

    case 2:
        if (gSeqStatus[0] >= op_110_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 61;
            op_obj_disp = 0;
            effect_48_init(3);
            return;
        }

        op_bg_move(60);
        break;

    case 3:
        if (gSeqStatus[0] >= op_110_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 62;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(61);
        break;

    case 4:
        if (gSeqStatus[0] >= op_110_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 63;
            op_obj_disp = 0;
            effect_48_init(4);
            return;
        }

        op_bg_move(62);
        break;

    case 5:
        if (gSeqStatus[0] >= op_110_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 64;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(63);
        break;

    default:
        op_bg_move(64);
        break;
    }
}

const s16 op_111_sound[5] = { 0, 2, 4, 7, 11 };

/** @brief Scene 111 — scene with 5 sound-triggered sub-phases. */
static void op_111_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 65;
        op_bg_move(65);
        effect_F6_init(41);
        effect_F6_init(42);
        effect_F6_init(43);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_111_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x6F)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 66;
            return;
        }

        op_bg_move(65);
        break;

    case 2:
        if (gSeqStatus[0] >= op_111_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 67;
            return;
        }

        op_bg_move(66);
        break;

    case 3:
        if (gSeqStatus[0] >= op_111_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 68;
            return;
        }

        op_bg_move(67);
        break;

    case 4:
        if (gSeqStatus[0] >= op_111_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 69;
            return;
        }

        op_bg_move(68);
        break;

    default:
        op_bg_move(69);
        break;
    }
}

const s16 op_112_sound[9] = { 0, 8, 8, 14, 19, 26, 34, 40, 44 };

/** @brief Scene 112 — extended scene with 9 sound-triggered sub-phases. */
static void op_112_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 70;
        op_bg_move(70);
        effect_F6_init(54);
        effect_F6_init(44);
        effect_F6_init(45);
        effect_F6_init(46);
        effect_F6_init(47);
        effect_F6_init(48);
        effect_F6_init(49);
        effect_F6_init(50);
        effect_F6_init(51);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_112_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x70)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 71;
            op_w.mv_ctr = 0;
            return;
        }

        op_bg_move(70);
        break;

    case 2:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 72;
            return;
        }

        op_bg_move(71);
        break;

    case 3:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 73;
            op_obj_disp = 0;
            effect_48_init(18);
            return;
        }

        op_bg_move(72);
        break;

    case 4:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 74;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(73);
        break;

    case 5:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 75;
            return;
        }

        op_bg_move(74);
        break;

    case 6:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 76;
            op_obj_disp = 0;
            effect_48_init(19);
            return;
        }

        op_bg_move(75);
        break;

    case 7:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 77;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(76);
        break;

    case 8:
        op_w.mv_ctr += 1;

        if (op_w.mv_ctr >= op_112_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 78;
            return;
        }

        op_bg_move(77);
        break;

    default:
        op_bg_move(78);
        break;
    }
}

const s16 op_113_sound[4] = { 0, 3, 7, 11 };

/** @brief Scene 113 — scene with 4 sound-triggered sub-phases. */
static void op_113_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_scrn_end = 0;
        op_work_clear();
        op_w.index = 79;
        op_bg_move(79);
        effect_F6_init(52);
        effect_F6_init(53);
        op_obj_disp = 0;
        effect_48_init(20);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_113_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x71)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 80;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(79);
        break;

    case 2:
        if (gSeqStatus[0] >= op_113_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_scrn_end = 0;
            op_work_clear();
            op_w.index = 81;
            op_obj_disp = 0;
            effect_48_init(21);
            return;
        }

        op_bg_move(80);
        break;

    case 3:
        if (gSeqStatus[0] >= op_113_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 82;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(81);
        break;

    default:
        op_bg_move(82);
        break;
    }
}

const s16 op_114_sound[6] = { 0, 2, 3, 4, 7, 9 };

/** @brief Scene 114 — scene with 6 sound-triggered sub-phases. */
static void op_114_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 83;
        op_bg_move(83);
        op_obj_disp = 0;
        effect_48_init(5);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_114_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x72)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 84;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(83);
        break;

    case 2:
        if (gSeqStatus[0] >= op_114_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 85;
            op_obj_disp = 0;
            effect_48_init(6);
            return;
        }

        op_bg_move(84);
        break;

    case 3:
        if (gSeqStatus[0] >= op_114_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 86;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(85);
        break;

    case 4:
        if (gSeqStatus[0] >= op_114_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            Zoom_Value_Set(64);
            op_work_clear();
            op_w.index = 87;
            op_obj_disp = 0;
            effect_48_init(7);
            return;
        }

        op_bg_move(86);
        break;

    case 5:
        if (gSeqStatus[0] >= op_114_sound[op_w.r_no_2]) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 88;
            op_obj_disp = 1;
            return;
        }

        op_bg_move(87);
        break;

    default:
        op_bg_move(88);
        break;
    }
}

const s16 op_115_sound[2] = { 0, 7 };

/** @brief Scene 115 — short scene with 2 sound-triggered sub-phases. */
static void op_115_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        Zoom_Value_Set(64);
        op_work_clear();
        op_w.index = 89;
        op_bg_move(89);
        effect_36_init(16);
        break;

    case 1:
        if ((gSeqStatus[0] >= op_115_sound[op_w.r_no_2]) && (gSeqStatus[0] != 0x73)) {
            op_w.r_no_2 += 1;
            op_work_clear();
            op_w.index = 90;
            return;
        }

        op_bg_move(89);
        break;

    default:
        op_bg_move(90);
        break;
    }
}

/** @brief Scene 116 — scene with screen quake and position reset. */
static void op_116_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_work_clear();
        op_w.index = 91;
        op_bg_move(91);
        effect_36_init(17);
        effect_36_init(28);
        op_w.mv_ctr = 88;
        FadeInit();
        break;

    case 1:
        op_bg_move(91);

        if (op_w.mv_ctr < 4) {
            FadeOut(0, 8, 8);
        }

        if (--op_w.mv_ctr <= 0) {
            op_w.r_no_2 += 1;
            op_w.mv_ctr = 16;
            return;
        }

        break;

    case 2:
        if ((FadeOut(0, 8, 8) != 0) && (--op_w.mv_ctr <= 0)) {
            op_w.index = 0x8000 - 1;
            op_end_flag = 1;
            op_w.r_no_2 += 1;
        }

        op_bg_move(91);
        break;

    default:
        op_bg_move(91);
        break;
    }
}

/** @brief Scene 117 — fade-out transition scene. */
static void op_117_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        Zoomf_Init();
        op_work_clear();
        op_w.index = 92;
        op_bg_move(92);
        effect_E1_init(1, 0, 1);
        effect_E1_init(0, 0, 1);
        effect_F5_init(16);
        effect_F5_init(17);
        effect_F5_init(18);
        effect_F5_init(9);
        op_bg_move(92);
        op_w.r_no_1 += 1;
        op_w.r_no_2 = 0;
        op_work_clear();
        break;
    }
}

/** @brief Scene 118 — final opening scene, advance to title screen. */
static void op_118_move() {
    switch (op_w.r_no_2) {
    case 0:
        op_w.r_no_2 += 1;
        op_w.mv_ctr = 60;
        break;

    case 1:
        op_w.mv_ctr -= 1;

        if (op_w.mv_ctr < 0) {
            op_w.r_no_2 += 1;
            return;
        }

        break;

    case 2:
        if (Request_Fade(0x28) != 0) {
            op_w.r_no_2 += 1;
            return;
        }

        break;

    case 3:
        if (Check_Fade_Complete() != 0) {
            op_w.r_no_2 += 1;
            Disp_Copyright();
            op_w.mv_ctr = 240;
        }

        op_w.index = 93;
        op_bg_move(93);
        break;

    case 4:
        op_w.mv_ctr -= 1;

        if (op_w.mv_ctr < 0) {
            op_w.r_no_2 += 1;
            return;
        }

        break;

    case 5:
        op_end_flag = 1;
        Bg_Off_W(14);
        op_bg_move(93);
        break;
    }
}

/** @brief Render the opening title card with zoom and screen effects. */
void opening_title() {
    switch (op_w.r_no_1) {
    case 0:
        op_w.r_no_1 += 1;
        break;

    case 1:
        op_w.free_work -= 1;

        if (op_w.free_work <= 0) {
            opening_title_01();
            op_w.r_no_1 += 1;
        }

        break;

    case 2:
        break;
    }
}

/** @brief Alternate title card rendering path (stage 01). */
static void opening_title_01() {
    s16 pos_work_x;
    s16 pos_work_y;

    effect_E1_init(1, 0, 1);
    effect_E1_init(0, 0, 1);
    effect_F5_init(0x10);
    effect_F5_init(0x11);
    effect_F5_init(0x12);
    effect_F5_init(9);
    Disp_Copyright();
    Bg_Off_W(0xFU);
    Scrn_Move_Set(0, 0x200 - g_state.bg_w.pos_offset, 0);
    Scrn_Move_Set(1, 0x200 - g_state.bg_w.pos_offset, 0);
    pos_work_x = -(0x200 - g_state.bg_w.pos_offset);
    pos_work_y = 0x300;
    Family_Set_W(1, pos_work_x, pos_work_y);
    op_end_flag = 1;
    g_state.bg_stop = 0;
    g_state.akebono_flag = 0;
    g_state.aku_flag = 0;
    g_state.sa_pa_flag = 0;
    g_state.bg_app = 0;
    g_state.bg_w.chase_flag = 0;
}
