/**
 * @file plpcu.c
 * Player Caught/Grabbed Controller
 */

#include "sf33rd/Source/Game/engine/plpcu.h"
#include "bin2obj/buttobi.h"
#include "bin2obj/etc.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effa7.h"
#include "sf33rd/Source/Game/effect/effd9.h"
#include "sf33rd/Source/Game/effect/effe2.h"
#include "sf33rd/Source/Game/engine/caldir.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plpdm.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"

static void setup_caught_process_flags(PLW* wk);
static void caught_cg_type_check(PLW* wk, PLW* emwk);
static s32 check_tsukamare_keizoku_check(PLW* wk, PLW* emwk);

static void scdmd_12000(PLW* wk);
static void scdmd_16000(PLW* wk);
static void scdmd_17000(PLW* wk);
static void scdmd_18000(PLW* wk);

#define PLPCU_DISPATCH_COUNT 4
#define SETUP_CU_DM_COUNT 20

void (*const setup_cu_dm_init_data[20])(PLW* wk);
void (*const plpcu_lv_00[4])(PLW*, PLW*);

/** @brief Top-level caught/grabbed state dispatcher. */
void Player_caught(PLW* wk) {
    PLW* emwk = (PLW*)wk->wu.dmg_adrs;

    setup_caught_process_flags(wk);

    if (wk->wu.routine_no[3] == 0) {
        wk->ukemi_ok_timer = wk->backup_ok_timer = emwk->wu.cmyd.koc;
        wk->uot_cd_ok_flag = 0;
        wk->ukemi_success = 0;
        wk->wu.dir_old = 1;
        pp_pulpara_caught(&wk->wu);
        clear_chainex_check(wk->wu.id);
    }

    if (wk->wu.routine_no[2] < PLPCU_DISPATCH_COUNT)
        plpcu_lv_00[wk->wu.routine_no[2]](wk, emwk);
}

/** @brief Clears per-frame process flags for the caught state. */
static void setup_caught_process_flags(PLW* wk) {
    wk->wu.next_z = wk->wu.my_priority;
    wk->running_f = 0;
    wk->guard_flag = 3;
    wk->guard_chuu = 0;
    wk->tsukami_f = false;
    wk->tsukamare_f = true;
    wk->scr_pos_set_flag = 0;
    wk->dm_hos_flag = 0;
    wk->zuru_timer = 0;
    wk->zuru_ix_counter = 0;
    wk->atemi_flag = 0;
    wk->caution_flag = 0;
    wk->sa->saeff_ok = 0;
    wk->sa->saeff_mp = 0;
    wk->cancel_timer = 0;
    wk->cmd_request = 0;
    wk->hsjp_ok = 0;
    wk->high_jump_flag = 0;
    wk->wu.swallow_no_effect = 0;

    if (wk->wu.routine_no[3]) {
        wk->sa_stop_flag = 0;
    }
}

/** @brief Caught state 00 — no-op placeholder. */
static void Caught_00000(PLW* /* unused */, PLW* /* unused */) {}

/** @brief Caught state 01 — grounded grab hold. */
static void Caught_01000(PLW* wk, PLW* emwk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        set_char_move_init(&wk->wu, 3, emwk->wu.cmyd.ix);
        emwk->kind_of_catch = 0;
        wk->wu.cmwk[11] = 0;
        /* fallthrough */

    case 1:
        if (check_tsukamare_keizoku_check(wk, emwk) != 0) {
            break;
        }

        if (emwk->wu.curr_rca == NULL) {
            break;
        }

        if (emwk->wu.curr_rca->catch_nix == wk->wu.dir_old) {
            char_move(&wk->wu);
        } else {
            char_move_index(&wk->wu, emwk->wu.curr_rca->catch_nix);
            wk->wu.dir_old = emwk->wu.curr_rca->catch_nix;
        }

        wk->wu.rl_flag = emwk->wu.rl_flag ^ emwk->wu.curr_rca->catch_flip;

        if (emwk->wu.rl_flag) {
            wk->wu.xyz[0].disp.pos = emwk->wu.xyz[0].disp.pos - emwk->wu.curr_rca->catch_hos_x;
        } else {
            wk->wu.xyz[0].disp.pos = emwk->wu.xyz[0].disp.pos + emwk->wu.curr_rca->catch_hos_x;
        }

        wk->wu.xyz[1].disp.pos = emwk->wu.xyz[1].disp.pos + emwk->wu.curr_rca->catch_hos_y;

        if (emwk->wu.curr_rca->catch_prio == 2) {
            wk->wu.next_z = emwk->wu.next_z - 1;
        } else {
            wk->wu.next_z = emwk->wu.next_z + 1;
        }

        caught_cg_type_check(wk, emwk);
    }
}

/** @brief Caught state 02 — air grab hold. */
static void Caught_02000(PLW* wk, PLW* emwk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        set_char_move_init(&wk->wu, 3, emwk->wu.cmyd.ix);
        emwk->kind_of_catch = 1;
        wk->wu.cmwk[11] = 0;
        /* fallthrough */

    case 1:
        if (check_tsukamare_keizoku_check(wk, emwk) != 0) {
            break;
        }

        if (emwk->wu.curr_rca == NULL) {
            break;
        }

        if (emwk->wu.curr_rca->catch_nix == wk->wu.dir_old) {
            char_move(&wk->wu);
        } else {
            char_move_index(&wk->wu, emwk->wu.curr_rca->catch_nix);
            wk->wu.dir_old = emwk->wu.curr_rca->catch_nix;
        }

        wk->wu.rl_flag = emwk->wu.rl_flag ^ emwk->wu.curr_rca->catch_flip;

        if (emwk->wu.rl_flag) {
            emwk->wu.xyz[0].disp.pos = wk->wu.xyz[0].disp.pos + emwk->wu.curr_rca->catch_hos_x;
        } else {
            emwk->wu.xyz[0].disp.pos = wk->wu.xyz[0].disp.pos - emwk->wu.curr_rca->catch_hos_x;
        }

        emwk->wu.xyz[1].disp.pos = wk->wu.xyz[1].disp.pos - emwk->wu.curr_rca->catch_hos_y;

        if (emwk->wu.curr_rca->catch_prio == 2) {
            wk->wu.next_z = emwk->wu.next_z - 1;
        } else {
            wk->wu.next_z = emwk->wu.next_z + 1;
        }

        caught_cg_type_check(wk, emwk);
    }
}

/** @brief Caught state 03 — no-op end of catch. */
static void Caught_03000(PLW* /* unused */, PLW* /* unused */) {}

/** @brief Handles cg_type transitions during the caught state. */
static void caught_cg_type_check(PLW* wk, PLW* emwk) {
    switch (wk->wu.cg_type) {
    case 2:
        wk->wu.hit_quake = wk->wu.dm_quake;
        wk->wu.dm_quake = 0;
        wk->wu.cg_type = 0;
        break;

    case 3:
        effect_A7_init(wk);
        wk->wu.cg_type = 0;
        break;

    case 9:
        if (wk->wu.now_koc == 3 && wk->wu.char_index == 60) {
            if (wk->dead_flag) {
                char_move_cmms(&wk->wu);
            } else {
                char_move_z(&wk->wu);
            }

            wk->wu.cmmd.koc = 1;
            wk->wu.cmmd.ix = 12;
            wk->wu.cmmd.pat = 1;
        } else if (wk->dead_flag) {
            char_move_cmms(&wk->wu);
        } else {
            char_move_z(&wk->wu);
        }

        wk->wu.routine_no[1] = wk->wu.cmmd.koc;
        wk->wu.routine_no[2] = wk->wu.cmmd.ix;
        wk->wu.routine_no[3] = wk->wu.cmmd.pat;
        wk->dm_ix = wk->wu.char_index;

        if (wk->wu.xyz[1].disp.pos < 0) {
            wk->wu.xyz[1].cal = 0;
        }

        s16 cu_dm_idx = wk->wu.routine_no[2] - 12;
        if (cu_dm_idx >= 0 && cu_dm_idx < SETUP_CU_DM_COUNT)
            setup_cu_dm_init_data[cu_dm_idx](wk);
        get_catch_off_data(wk, emwk->wu.att.reaction);

        if (wk->ukemi_success == 0) {
            wk->ukemi_ok_timer = wk->backup_ok_timer;
            wk->uot_cd_ok_flag = 0;
        }

        break;
    }
}

/** @brief Checks if the caught hold should continue (tsukamare keizoku). */
static s32 check_tsukamare_keizoku_check(PLW* wk, PLW* emwk) {
    if (!emwk->tsukami_f) {
        wk->wu.routine_no[1] = 1;
        wk->wu.routine_no[2] = 88;
        wk->wu.routine_no[3] = 0;
        wk->wu.dm_stop = wk->wu.hit_stop = 0;

        if (wk->wu.cg_flip & 2) {
            wk->reserv_add_y = -getObjectHeight(wk->wu.cg_number);
        } else {
            wk->reserv_add_y = 0;
        }

        return 1;
    }

    return 0;
}

/** @brief Sets up caught-damage init data for states 12–13 (standing hit). */
static void scdmd_12000(PLW* wk) {
    wk->dm_step_tbl = _dm_step_data[_select_hit_dsd[wk->wu.dm_impact][get_weight_point(&wk->wu)]];

    if (!wk->wu.dm_attribute) {
        return;
    }

    setup_accessories(wk, wk->wu.pat_status);

    if (wk->wu.dm_attribute != 2) {
        effect_D9_init(wk, (u8)wk->wu.dm_attribute);
    }
}

/** @brief Sets up caught-damage init data for states 14–15, 21–22, 27 (launch/stagger/zero-Y). */
static void scdmd_14000(PLW* wk) {
    setup_butt_own_data(&wk->wu);
    wk->wu.mvxy.a[1].sp = wk->wu.mvxy.d[1].sp = wk->wu.mvxy.kop[1] = 0;
}

/** @brief Sets up caught-damage init data for states 16, 19, 30 (blow-away/ground-to-air/extended). */
static void scdmd_16000(PLW* wk) {
    setup_butt_own_data(&wk->wu);
    cal_initial_speed_y(&wk->wu, _buttobi_time_table[wk->wu.char_index][wk->wu.dm_attlv], 0);
}

/** @brief Sets up caught-damage init data for states 17, 28 (air hit/stun KO). */
static void scdmd_17000(PLW* wk) {
    setup_butt_own_data(&wk->wu);
    cal_initial_speed_y(&wk->wu, _buttobi_time_table[wk->wu.char_index][wk->wu.dm_attlv], wk->wu.xyz[1].disp.pos);
}

/** @brief Sets up caught-damage init data for state 18 (attribute air hit). */
static void scdmd_18000(PLW* wk) {
    setup_butt_own_data(&wk->wu);
    cal_initial_speed_y(&wk->wu, _buttobi_time_table[wk->wu.char_index][wk->wu.dm_attlv], wk->wu.xyz[1].disp.pos);

    if (!wk->wu.dm_attribute) {
        return;
    }

    setup_accessories(wk, wk->wu.pat_status);

    if (wk->wu.dm_attribute != 2) {
        effect_D9_init(wk, (u8)wk->wu.dm_attribute);
    }
}

/** @brief Sets up caught-damage init data for states 20, 26, 31 (redirect/groundbounce/throw release). */
static void scdmd_20000(PLW* wk) {
    setup_butt_own_data(&wk->wu);
}

/** @brief Sets up caught-damage init data for state 23 (crumple-fall). */
static void scdmd_23000(PLW* wk) {
    if (wk->wu.xyz[1].disp.pos < 0) {
        wk->wu.xyz[1].cal = 0;
    }

    setup_butt_own_data(&wk->wu);
}

/** @brief Sets up caught-damage init data for state 24 (spiral-down). */
static void scdmd_24000(PLW* wk) {
    wk->wu.routine_no[2] = 0;
    wk->wu.routine_no[3] = 1;
}

/** @brief No-op caught-damage init for states 25, 29 (wallbounce/SA cinematic). */
static void scdmd_noop(PLW* wk) { (void)wk; }

/*                                          idx: 12         13         14         15         16
                                                  17         18         19         20         21
                                                  22         23         24         25         26
                                                  27         28         29         30         31  */
void (*const setup_cu_dm_init_data[20])(PLW* wk) = { scdmd_12000, scdmd_12000, scdmd_14000, scdmd_14000, scdmd_16000,
                                                     scdmd_17000, scdmd_18000, scdmd_16000, scdmd_20000, scdmd_14000,
                                                     scdmd_14000, scdmd_23000, scdmd_24000, scdmd_noop,  scdmd_20000,
                                                     scdmd_14000, scdmd_17000, scdmd_noop,  scdmd_16000, scdmd_20000 };

void (*const plpcu_lv_00[4])(PLW*, PLW*) = { Caught_00000, Caught_01000, Caught_02000, Caught_03000 };
