/**
 * @file plcnt.c
 * Character Controller
 */

#include "sf33rd/Source/Game/engine/plcnt.h"
#include "game_state.h"
#include "common.h"
#include "constants.h"
#include "main.h"
#include "arcade/arcade_char_data.h"
#include "sf33rd/Source/Game/animation/win_pl.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff00.h"
#include "sf33rd/Source/Game/effect/eff01.h"
#include "sf33rd/Source/Game/effect/eff33.h"
#include "sf33rd/Source/Game/effect/effc9.h"
#include "sf33rd/Source/Game/effect/effd3.h"
#include "sf33rd/Source/Game/effect/effe3.h"
#include "sf33rd/Source/Game/effect/effe4.h"
#include "sf33rd/Source/Game/effect/effe5.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effj7.h"
#include "sf33rd/Source/Game/effect/effk5.h"
#include "sf33rd/Source/Game/effect/effk7.h"
#include "sf33rd/Source/Game/engine/charid.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/manage.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/plpdm.h"
#include "sf33rd/Source/Game/engine/pls01.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/training_state.h"
#include "sf33rd/Source/Game/ui/count.h"

#if DEBUG
#include "sf33rd/Source/Game/debug/debug_config.h"
#endif

#include "port/I_System.h"

static void pli_0000();
static void pli_1000();
static void move_player_work();
static void move_P1_move_P2();
static void move_P2_move_P1();
static void check_damage_adjust();
static void check_damage_adjust_throw(PLW* as, PLW* ds);
static void check_damage_adjust_strike(PLW* w1, PLW* w2);
static s32 time_over_check();
static s32 will_die();
static void setup_settle_rno(s16 kos);
static void settle_check();
static s32 check_sa_resurrection(PLW* wk);
static s16 nekorobi_check(s8 ix);
static s16 footwork_check(s8 ix);
static void setup_gouki_wins();
static void setup_any_data();
static void set_base_data(PLW* wk, s16 ix);
void set_base_data_metamorphose(PLW* wk, s16 dmid);
static void set_base_data_tiny(PLW* wk);
static void setup_other_data(PLW* wk);
static s16 remake_sa_store_max(s16 ix, s16 store_max);
static s16 remake_sa_gauge_len(s16 ix, s16 gauge_len);
void clear_super_arts_point(PLW* wk);
static void set_scrrrl();

// NOTE: g_state.rambod/g_state.ramhan are recalculated each frame by effk5.c from the effect state
// (which IS serialized in EffectState). These do not need GameState serialization.

// NOTE: omop_spmv_ng_table is computed once at match start from game settings by init_omop().
// In netplay, both players should have identical settings. Not gameplay state, no serialization needed.
u32 omop_spmv_ng_table[2];
u32 omop_spmv_ng_table2[2];
s8 vib_sel[2];

static void plcnt_init();
static void plcnt_move();
static void plcnt_die();

void (*const player_main_process[3])() = { plcnt_init, plcnt_move, plcnt_die };

static void init_app_10000();
static void init_app_20000();
static void init_app_30000();

void (*const appear_initalize[4])() = { init_app_10000, init_app_10000, init_app_20000, init_app_30000 };

static void settle_type_00000();
static void settle_type_10000();
static void settle_type_20000();
static void settle_type_30000();
static void settle_type_40000();

void (*const settle_process[5])() = {
    settle_type_00000, settle_type_10000, settle_type_20000, settle_type_30000, settle_type_40000
};

const s8 plid_data[20] = { 6, 3, 5, 1, 2, 9, 7, 4, 10, 8, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22 };

const s8 weight_lv_table[20] = { 2, 2, 1, 0, 1, 2, 3, 0, 1, 0, 0, 1, 1, 2, 1, 1, 1, 3, 2, 1 };

const s16 quake_table[64] = { 0, -1, 0, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -2, 1, -2, 1, -2, 1, -2, 1, -2,
                              1, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -2, 2, -3, 2, -3,
                              2, -3, 2, -3, 2, -3, 2, -3, 2, -3, 2, -3, 2, -3, 2, -3, 3, -3, 3, -3 };

const s16 kage_base[20][2] = { { 0, 21 },  { 0, 30 },  { 0, 21 },  { -4, 16 }, { 4, 21 }, { 6, 20 }, { -4, 26 },
                               { -4, 20 }, { 0, 25 },  { -3, 22 }, { -4, 16 }, { 0, 21 }, { 0, 21 }, { 0, 21 },
                               { 0, 21 },  { -8, 21 }, { 0, 23 },  { 0, 24 },  { 6, 25 }, { -6, 21 } };

const SA_DATA super_arts_data[20][4] = { { { 20, 24, 25, 0, 0, 0, 0, 3, 128, 1, 65536 },
                                           { 21, 24, 25, 0, 0, 0, 0, 3, 128, 1, 65536 },
                                           { 22, 24, 25, 0, 0, 0, 0, 3, 128, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 128, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 96, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 80, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 128, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 96, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65536 } },
                                         { { 22, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 88, 3, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 1, 72, 1, 16384 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 96, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 112, 1, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 80, 3, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 88, 1, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 104, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 72, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 128, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 88, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 0, 0, 0, 38, 0, 0, 0, 0, 88, 3, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 0, 80, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 80, 3, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 104, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 128, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 22, 0, 0, 0, 0, 0, 1, 1, 104, 1, 16384 },
                                           { 21, 24, 0, 0, 0, 0, 0, 0, 88, 3, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 1, 1, 112, 1, 13107 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 65536 } },
                                         { { 21, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 104, 2, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 1, 64, 1, 13107 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 104, 1, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 80, 3, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 72, 3, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 96, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 104, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 80, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 88, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 24, 25, 38, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 21, 24, 25, 0, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 22, 24, 25, 39, 0, 0, 0, 0, 112, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 88, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 104, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 72, 3, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 22, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 0, 88, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 1, 96, 1, 10922 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 96, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 1, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 1, 112, 1, 10922 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 96, 2, 65536 },
                                           { 0, 0, 0, 38, 0, 0, 0, 0, 112, 1, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 1, 128, 1, 6553 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 104, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 80, 1, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } } };

const SA_DATA super_arts_DATA[20][4] = { { { 20, 24, 25, 0, 0, 0, 0, 3, 120, 2, 65536 },
                                           { 21, 24, 25, 0, 0, 0, 0, 3, 120, 2, 65536 },
                                           { 22, 24, 25, 0, 0, 0, 0, 3, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65536 } },
                                         { { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 1, 120, 2, 16384 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 0, 0, 0, 38, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 22, 24, 0, 0, 0, 0, 1, 1, 120, 2, 16384 },
                                           { 21, 24, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 20, 24, 0, 0, 0, 0, 1, 1, 120, 2, 13107 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 65536 } },
                                         { { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 1, 120, 2, 13107 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 3, 64, 1, 65536 } },
                                         { { 20, 24, 25, 38, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 24, 25, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 24, 25, 39, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 3, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 1, 120, 2, 10922 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 1, 120, 2, 10922 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 38, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 1, 120, 2, 6553 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } },
                                         { { 20, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 22, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 21, 0, 0, 0, 0, 0, 0, 0, 120, 2, 65536 },
                                           { 0, 0, 0, 0, 0, 0, 0, 0, 64, 1, 65536 } } };

const s16 pl_piyo_tbl[NUM_CHARS] = {
    72, // Gill
    72, // Alex
    64, // Ryu
    64, // Yun
    72, // Dudley
    64, // Necro
    72, // Hugo
    64, // Ibuki
    64, // Elena
    72, // Oro
    64, // Yang
    64, // Ken
    64, // Sean
    64, // Urien
    56, // Akuma
#if CPS3
    56, // Shin Akuma
#endif
    64, // Chun-Li
    64, // Makoto
    72, // Q
    64, // Twelve
    56, // Remy
};

const s32 pl_nr_piyo_tbl[NUM_CHARS] = {
    3276, // Gill
    2849, // Alex
    2978, // Ryu
    2730, // Yun
    2978, // Dudley
    2849, // Necro
    3120, // Hugo
    2730, // Ibuki
    2978, // Elena
    2730, // Oro
    2730, // Yang
    2978, // Ken
    2849, // Sean
    3120, // Urien
    2849, // Akuma
#if CPS3
    2978, // Shin Akuma
#endif
    2978, // Chun-Li
    3276, // Makoto
    2978, // Q
    2849, // Twelve
    3120, // Remy
};

const s16 tsuujyou_dageki_00[16] = { 150, 150, 130, 130, 130, 110, 110, 110, 110, 110, 90, 90, 90, 90, 90, 90 };
const s16 tsuujyou_dageki_01[16] = { 150, 150, 150, 150, 130, 130, 130, 130, 110, 110, 110, 110, 90, 90, 90, 90 };
const s16 tsuujyou_dageki_02[16] = { 150, 150, 150, 150, 150, 150, 130, 130, 130, 130, 130, 110, 110, 110, 90, 90 };

const s16* tsuujyou_dageki[4] = { tsuujyou_dageki_00, tsuujyou_dageki_01, tsuujyou_dageki_02, tsuujyou_dageki_02 };

const s16 hissatsu_dageki_00[16] = { 210, 210, 190, 190, 190, 170, 170, 170, 170, 170, 150, 150, 150, 150, 150, 150 };
const s16 hissatsu_dageki_01[16] = { 210, 210, 210, 210, 190, 190, 190, 190, 170, 170, 170, 170, 150, 150, 150, 150 };
const s16 hissatsu_dageki_02[16] = { 210, 210, 210, 210, 210, 210, 190, 190, 190, 190, 190, 170, 170, 170, 150, 150 };

const s16* hissatsu_dageki[4] = {
    hissatsu_dageki_00,
    hissatsu_dageki_01,
    hissatsu_dageki_02,
    hissatsu_dageki_02,
};

const s16 tsuujyou_nage_00[16] = { 180, 180, 160, 160, 160, 140, 140, 140, 140, 140, 120, 120, 120, 120, 120, 120 };
const s16 tsuujyou_nage_01[16] = { 180, 180, 180, 180, 160, 160, 160, 160, 140, 140, 140, 140, 120, 120, 120, 120 };
const s16 tsuujyou_nage_02[16] = { 180, 180, 180, 180, 180, 180, 160, 160, 160, 160, 160, 140, 140, 140, 120, 120 };

const s16* tsuujyou_nage[4] = { tsuujyou_nage_00, tsuujyou_nage_01, tsuujyou_nage_02, tsuujyou_nage_02 };

const s16 hissatsu_nage_00[16] = { 240, 240, 200, 200, 200, 160, 160, 160, 160, 160, 120, 120, 120, 120, 120, 120 };
const s16 hissatsu_nage_01[16] = { 240, 240, 240, 240, 200, 200, 200, 200, 160, 160, 160, 160, 120, 120, 120, 120 };
const s16 hissatsu_nage_02[16] = { 240, 240, 240, 240, 240, 240, 200, 200, 200, 200, 200, 160, 160, 160, 120, 120 };

const s16* hissatsu_nage[4] = { hissatsu_nage_00, hissatsu_nage_01, hissatsu_nage_02, hissatsu_nage_02 };

const s16 super_arts_dageki_00[16] = { 240, 240, 200, 200, 200, 160, 160, 160, 160, 160, 120, 120, 120, 120, 120, 120 };
const s16 super_arts_dageki_01[16] = { 240, 240, 240, 240, 200, 200, 200, 200, 160, 160, 160, 160, 120, 120, 120, 120 };
const s16 super_arts_dageki_02[16] = { 240, 240, 240, 240, 240, 240, 200, 200, 200, 200, 200, 160, 160, 160, 120, 120 };

const s16* super_arts_dageki[4] = {
    super_arts_dageki_00, super_arts_dageki_01, super_arts_dageki_02, super_arts_dageki_02
};

const s16 super_arts_nage_00[16] = { 270, 270, 230, 230, 230, 190, 190, 190, 190, 190, 150, 150, 150, 150, 150, 150 };
const s16 super_arts_nage_01[16] = { 270, 270, 270, 270, 230, 230, 230, 230, 190, 190, 190, 190, 150, 150, 150, 150 };
const s16 super_arts_nage_02[16] = { 270, 270, 270, 270, 270, 270, 230, 230, 230, 230, 230, 190, 190, 190, 150, 150 };

const s16* super_arts_nage[4] = { super_arts_nage_00, super_arts_nage_01, super_arts_nage_02, super_arts_nage_02 };

const s16** kizetsu_timer_table[9] = { tsuujyou_dageki,   hissatsu_dageki,   tsuujyou_nage,
                                       hissatsu_nage,     super_arts_dageki, super_arts_nage,
                                       super_arts_dageki, super_arts_nage,   super_arts_dageki };

/** @brief Main player controller — dispatches per-player state updates for both sides. */
void Player_control() {
    pulpul_scene = 1;

    if (g_state.pcon_rno[0] + g_state.pcon_rno[1] != 0) {
        if (g_state.Game_pause || g_state.EXE_flag) {
            goto end;
        } else {
            if (!g_state.pcon_dp_flag) {
                if (--g_state.vital_inc_timer > 50) {
                    g_state.vital_inc_timer = 50;
                }

                if (--g_state.vital_dec_timer > 40) {
                    g_state.vital_dec_timer = 40;
                }
            } else {
                g_state.vital_inc_timer = 50;
                g_state.vital_dec_timer = 40;
                g_state.sag_inc_timer[0] = g_state.sag_inc_timer[1] = 20;
            }
        }
    }

    g_state.players_timer++;
    g_state.players_timer &= 0x7FFF;
    set_scrrrl();
    player_main_process[g_state.pcon_rno[0]]();
    check_body_touch();
    check_damage_adjust();
    set_quake(&g_state.plw[0]);
    set_quake(&g_state.plw[1]);

    if (!g_state.plw[0].invuln_flag && !g_state.plw[0].absolute_invuln_flag) {
        hit_push_request(&g_state.plw[0].wu);
    }

    if (!g_state.plw[1].invuln_flag && !g_state.plw[1].absolute_invuln_flag) {
        hit_push_request(&g_state.plw[1].wu);
    }

    add_next_position(&g_state.plw[0]);
    add_next_position(&g_state.plw[1]);
    check_cg_zoom();

end:
    if (g_state.Game_pause != 0x81) {
        store_player_after_image_data();
    }
}

/** @brief Requests drawing of both player sprites to the render queue. */
void reqPlayerDraw() {
    if (Debug_w[DEBUG_PLAYER_NO_DISP] == 0) {
        move_effect_work(6);
        sort_push_request(&g_state.plw[0].wu);
        sort_push_request(&g_state.plw[1].wu);
    }
}

/** @brief Initializes player control work for both sides at match start. */
static void plcnt_init() {
    g_state.plw[0].reserv_add_y = g_state.plw[1].reserv_add_y = 0;
    appear_initalize[g_state.appear_type]();
    move_player_work();
}

/** @brief Initializes player appear state type 1 (standard entrance). */
static void init_app_10000() {
    switch (g_state.pcon_rno[1]) {
    case 0:
        pli_0000();
        g_state.pcon_rno[1] = 2;
        g_state.pcon_dp_flag = false;
        g_state.round_slow_flag = false;
        g_state.dead_voice_flag = false;
        g_state.another_bg[0] = g_state.another_bg[1] = 0;
        g_state.plw[0].scr_pos_set_flag = g_state.plw[1].scr_pos_set_flag = 1;

        if (g_state.Play_Type == 0) {
            if (g_state.plw[0].wu.pl_operator) {
                mpp_w.useChar[g_state.My_char[0]]++;
            }

            if (g_state.plw[1].wu.pl_operator) {
                mpp_w.useChar[g_state.My_char[1]]++;
            }
        }

        break;

    case 1:
        pli_1000();
        break;

    case 2:
        g_state.pcon_rno[1] = 3;

        if (g_state.plw[0].wu.pl_operator) {
            g_state.parry_ctr_vs[0][0] = g_state.parry_ctr_ori[0];
        } else {
            g_state.parry_ctr_vs[0][0] = 0;
        }

        if (g_state.plw[1].wu.pl_operator) {
            g_state.parry_ctr_vs[0][1] = g_state.parry_ctr_ori[1];
        } else {
            g_state.parry_ctr_vs[0][1] = 0;
        }

        break;

    case 3:
        g_state.pcon_rno[1] = 1;
        pli_0002();
        break;
    }
}

/** @brief Initializes player appear state type 2 (between-round re-entrance). */
static void init_app_20000() {
    s16 i;

    switch (g_state.pcon_rno[1]) {
    case 0:
        g_state.pcon_rno[1]++;
        g_state.round_slow_flag = false;
        g_state.dead_voice_flag = false;
        g_state.pcon_dp_flag = false;
        g_state.another_bg[0] = g_state.another_bg[1] = 0;

        for (i = 0; i < 8; i++) {
            g_state.plw[0].wu.routine_no[i] = g_state.plw[1].wu.routine_no[i] = 0;
        }

        setup_any_data();
        g_state.plw[0].do_not_move = g_state.plw[1].do_not_move = 0;
        g_state.plw[0].scr_pos_set_flag = g_state.plw[1].scr_pos_set_flag = 1;
        break;

    case 1:
        pli_1000();
        break;
    }
}

/** @brief Initializes player appear state type 3 (victory pose setup). */
static void init_app_30000() {
    s16 i;

    switch (g_state.pcon_rno[1]) {
    case 0:
        g_state.pcon_rno[1]++;
        g_state.round_slow_flag = false;
        g_state.dead_voice_flag = false;

        for (i = 1; i < 8; i++) {
            g_state.plw[0].wu.routine_no[i] = g_state.plw[1].wu.routine_no[i] = 0;
        }

        g_state.plw[0].wu.routine_no[0] = g_state.plw[1].wu.routine_no[0] = 1;
        g_state.another_bg[0] = g_state.another_bg[1] = 0;
        g_state.plw[0].do_not_move = g_state.plw[1].do_not_move = 0;
        K7_muriyari_metamor_rebirth(&g_state.plw[0]);
        K7_muriyari_metamor_rebirth(&g_state.plw[1]);
        break;

    case 1:
        if (g_state.plw[0].wu.routine_no[0] != 3 || g_state.plw[1].wu.routine_no[0] != 3) {
            break;
        }

        g_state.pcon_rno[0] = 2;
        g_state.pcon_rno[1] = 3;
        g_state.pcon_rno[2] = 1;
        setup_EJG_index();
        effect_C9_init(g_state.plw, 0);
        effect_C9_init(g_state.plw, 1);
        effect_C9_init(g_state.plw, 2);
        load_any_color(0x3F, 0);
        load_any_texture_patnum(0x71E0, 0xE, 0);
        effect_work_kill(4, 0xD9);

        if (g_state.plw[0].player_number == 6) {
            effect_33_init(&g_state.plw[0].wu);
        }

        if (g_state.plw[1].player_number == 6) {
            effect_33_init(&g_state.plw[1].wu);
        }

        break;
    }
}

/** @brief Player init phase 0 — initial idle state setup. */
static void pli_0000() {
    g_state.pcon_rno[1]++;
    g_state.round_slow_flag = false;
    I_ZeroArray(g_state.plw);
    setup_base_and_other_data();
}

/** @brief Player init phase 1 — work data setup and state machine start. */
static void pli_1000() {
    if (g_state.plw[0].wu.routine_no[0] != 3) {
        return;
    }

    if (g_state.plw[1].wu.routine_no[0] != 3) {
        return;
    }

    if (!g_state.Allow_a_battle_f) {
        return;
    }

    g_state.pcon_rno[0] = 1;
    g_state.pcon_rno[1] = 0;
    g_state.plw[0].wu.routine_no[0] = 4;
    g_state.plw[1].wu.routine_no[0] = 4;
    ca_check_flag = 1;
}

/** @brief Player init phase 2 — additional init (currently a no-op). */
void pli_0002() {
    // Do nothing
}

/** @brief Per-frame player movement and state update (the core player tick). */
static void plcnt_move() {
    if (g_state.Mode_Type == MODE_NORMAL_TRAINING) {
        update_training_state();
    }

    if (time_over_check() != 0) {
        return;
    }

#if DEBUG
    if (DebugConfig_Get(DEBUG_PLAYER_1_INVINCIBLE)) {
        g_state.plw[0].wu.damage_vitality = 0;
    }

    if (DebugConfig_Get(DEBUG_PLAYER_2_INVINCIBLE)) {
        g_state.plw[1].wu.damage_vitality = 0;
    }

    if (DebugConfig_Get(DEBUG_PLAYER_1_NO_LIFE)) {
        g_state.plw[0].wu.vital_new = 0;
    }

    if (DebugConfig_Get(DEBUG_PLAYER_2_NO_LIFE)) {
        g_state.plw[1].wu.vital_new = 0;
    }
#endif

    if (g_state.No_Death) {
        g_state.plw[0].wu.damage_vitality = g_state.plw[1].wu.damage_vitality = 0;
    }

    if (g_state.Break_Into) {
        g_state.plw[0].wu.damage_vitality = g_state.plw[1].wu.damage_vitality = 0;
    }

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING && Training->contents[0][1][3] == 0) {
        g_state.plw[0].wu.dm_nodeathattack = 1;
        g_state.plw[1].wu.dm_nodeathattack = 1;
    }

    move_player_work();

    if (g_state.mutual_trade_flag) {
        subtract_dm_vital_aiuchi(&g_state.plw[0]);
        subtract_dm_vital_aiuchi(&g_state.plw[1]);

        if ((g_state.plw[0].dead_flag != 0) && (g_state.plw[1].dead_flag != 0)) {
            g_state.plw[0].wu.hit_stop = g_state.plw[1].wu.hit_stop = 2;
            g_state.plw[0].wu.damage_hit_stop = g_state.plw[1].wu.damage_hit_stop = 0;
            g_state.plw[0].wu.hit_quake = g_state.plw[1].wu.hit_quake = 4;
            g_state.plw[0].wu.damage_screen_shake = g_state.plw[1].wu.damage_screen_shake = 0;
        } else if ((g_state.plw[0].dead_flag != 0) || (g_state.plw[1].dead_flag != 0)) {
            g_state.plw[0].wu.hit_stop = g_state.plw[1].wu.hit_stop = 4;
            g_state.plw[0].wu.damage_hit_stop = g_state.plw[1].wu.damage_hit_stop = 0;
            g_state.plw[0].wu.hit_quake = g_state.plw[1].wu.hit_quake = 8;
            g_state.plw[0].wu.damage_screen_shake = g_state.plw[1].wu.damage_screen_shake = 0;
        }
    }

    settle_check();

    if (g_state.pcon_rno[0] == 2) {
        if (g_state.Round_Result & 0x980) {
            if ((g_state.Round_Result & 0x800) && g_state.gouki_wins) {
                effect_D3_init(1);
            } else {
                effect_D3_init(0);
            }
        }

        if ((g_state.plw[0].chip_death_flag == 1) || (g_state.plw[1].chip_death_flag == 1)) {
            g_state.Round_Result |= 0x200;
        }

        if (g_state.Winner_id != g_state.Loser_id) {
            grade_store_vitality(g_state.Winner_id + 0);
        }
    }

    grade_check_tairyokusa();
}

/** @brief Handles player death/KO finalization. */
static void plcnt_die() {
    g_state.plw[0].wu.damage_vitality = g_state.plw[1].wu.damage_vitality = 0;
    settle_process[g_state.pcon_rno[1]]();
    move_player_work();

    if (g_state.pcon_rno[1] == 3) {
        g_state.plw[0].scr_pos_set_flag = g_state.plw[1].scr_pos_set_flag = 0;
    }
}

/** @brief Settle type 0: Processes a normal KO conclusion. */
static void settle_type_00000() {
    switch (g_state.pcon_rno[2]) {
    case 0:
        g_state.plw[g_state.Winner_id].wu.dir_timer = 60;
        g_state.pcon_rno[2]++;
        /* fallthrough */

    case 1:
        if (nekorobi_check(g_state.Loser_id)) {
            g_state.pcon_rno[2]++;
            g_state.plw[g_state.Winner_id].wkey_flag = 1;
        }

        if (--g_state.plw[g_state.Winner_id].wu.dir_timer == 0) {
            g_state.plw[g_state.Winner_id].wkey_flag = 1;
        }

        break;

    case 2:
        if (footwork_check(g_state.Winner_id)) {
            grade_set_round_result(g_state.Winner_id + 0);
            g_state.pcon_rno[2]++;
            g_state.plw[g_state.Winner_id].wu.routine_no[2] = 40;
            g_state.plw[g_state.Winner_id].wu.routine_no[3] = 0;
            g_state.plw[g_state.Loser_id].wu.routine_no[1] = 0;
            g_state.plw[g_state.Loser_id].wu.routine_no[2] = 41;
            g_state.plw[g_state.Loser_id].wu.routine_no[3] = 0;
            g_state.plw[0].wu.cg_type = g_state.plw[1].wu.cg_type = 0;
            g_state.plw[0].image_setup_flag = g_state.plw[1].image_setup_flag = 0;
            complete_victory_pause();
        }

        break;

    case 3:
        if (g_state.plw[g_state.Winner_id].wu.routine_no[3] == 9) {
            g_state.pcon_rno[2]++;
        }

        break;
    }
}

/** @brief Settle type 1: Processes a time-over conclusion. */
static void settle_type_10000() {
    switch (g_state.pcon_rno[2]) {
    case 0:
        if (nekorobi_check(0) && nekorobi_check(1)) {
            g_state.pcon_rno[2]++;
        }

        break;

    case 1:
        complete_victory_pause();
        g_state.pcon_rno[2]++;
        g_state.plw[0].image_setup_flag = g_state.plw[1].image_setup_flag = 0;
        break;
    }
}

/** @brief Settle type 2: Processes a double-KO conclusion. */
static void settle_type_20000() {
    switch (g_state.pcon_rno[2]) {
    case 0:
        g_state.plw[0].wkey_flag = g_state.plw[1].wkey_flag = 1;
        g_state.plw[0].image_setup_flag = g_state.plw[1].image_setup_flag = 0;
        g_state.pcon_rno[2]++;
        /* fallthrough */

    case 1:
        if (footwork_check(0) && footwork_check(1)) {
            g_state.pcon_rno[2]++;
        }

        break;

    case 2:
        complete_victory_pause();

        if (g_state.plw[0].wu.vital_new == g_state.plw[1].wu.vital_new) {
            g_state.pcon_rno[2] = 4;
            return;
        }

        grade_set_round_result(g_state.Winner_id + 0);
        g_state.plw[g_state.Winner_id].wu.routine_no[2] = 40;
        g_state.plw[g_state.Loser_id].wu.routine_no[2] = 41;
        g_state.plw[0].wu.routine_no[1] = g_state.plw[1].wu.routine_no[1] = 0;
        g_state.plw[0].wu.routine_no[3] = g_state.plw[1].wu.routine_no[3] = 0;
        g_state.plw[0].wu.cg_type = g_state.plw[1].wu.cg_type = 0;
        g_state.pcon_rno[2]++;
        break;

    case 3:
        if ((g_state.plw[0].wu.routine_no[3] == 9) && (g_state.plw[1].wu.routine_no[3] == 9)) {
            g_state.pcon_rno[2]++;
        }

        break;
    }
}

/** @brief Settle type 3: Processes a draw-game conclusion. */
static void settle_type_30000() {
    switch (g_state.pcon_rno[2]) {
    case 0:
        break;

    case 1:
        if ((g_state.Event_Judge_Gals == -1) && g_state.Complete_Judgement) {
            g_state.plw[g_state.Winner_id].wu.routine_no[2] = 40;
            g_state.plw[g_state.Loser_id].wu.routine_no[2] = 41;
            g_state.plw[0].wu.routine_no[3] = g_state.plw[1].wu.routine_no[3] = 0;
            g_state.plw[0].wu.cg_type = g_state.plw[1].wu.cg_type = 0;
            grade_set_round_result(g_state.Winner_id + 0);
            complete_victory_pause();
            g_state.pcon_rno[2]++;
        }

        break;

    case 2:
        if ((g_state.plw[0].wu.routine_no[3] == 9) && (g_state.plw[1].wu.routine_no[3] == 9)) {
            g_state.pcon_rno[2]++;
        }

        break;
    }
}

/** @brief Settle type 4: Processes a complete victory (judgement gals) conclusion. */
static void settle_type_40000() {
    switch (g_state.pcon_rno[2]) {
    case 0:
        g_state.plw[g_state.Winner_id].wkey_flag = 1;
        g_state.pcon_rno[2] += 1;
        /* fallthrough */

    case 1:
        if (nekorobi_check(g_state.Loser_id) == 0) {
            break;
        }

        g_state.pcon_rno[2] += 1;
        /* fallthrough */

    case 2:
        if (footwork_check(g_state.Winner_id)) {
            g_state.pcon_rno[2]++;
            g_state.plw[g_state.Winner_id].wu.routine_no[2] = 40;
            g_state.plw[g_state.Winner_id].wu.routine_no[3] = 0;
            g_state.plw[g_state.Loser_id].wu.routine_no[1] = 0;
            g_state.plw[g_state.Loser_id].wu.routine_no[2] = 41;
            g_state.plw[g_state.Loser_id].wu.routine_no[3] = 0;
            g_state.plw[g_state.Winner_id].wu.cg_type = 0;
            grade_set_round_result(g_state.Winner_id + 0);
            g_state.plw[0].image_setup_flag = g_state.plw[1].image_setup_flag = 0;
            g_state.plw[g_state.Winner_id].wu.dir_timer = 60;
            set_conclusion_slow();
        }

        break;

    case 3:
        if (--g_state.plw[g_state.Winner_id].wu.dir_timer <= 0) {
            complete_victory_pause();
            g_state.pcon_rno[2] += 1;
        }

        break;

    case 4:
        if (g_state.plw[g_state.Winner_id].wu.routine_no[3] == 9) {
            g_state.pcon_rno[2] += 1;
        }

        break;
    }
}

/** @brief Processes player work updates: movement, scroll, image data. */
static void move_player_work() {
    if (g_state.plw[0].reserv_add_y) {
        g_state.plw[0].wu.xyz[1].disp.pos += g_state.plw[0].reserv_add_y;
        g_state.plw[0].reserv_add_y = 0;
    }

    if (g_state.plw[1].reserv_add_y) {
        g_state.plw[1].wu.xyz[1].disp.pos += g_state.plw[1].reserv_add_y;
        g_state.plw[1].reserv_add_y = 0;
    }

    g_state.positional_relation = check_work_position(&g_state.plw[0].wu, &g_state.plw[1].wu);
    set_rl_move(&g_state.plw[0]);
    set_rl_move(&g_state.plw[1]);
    g_state.Timer_Freeze = 0;

    switch (g_state.plw[0].is_throwing + (g_state.plw[1].is_throwing * 2)) {
    case 1:
        move_P1_move_P2();
        break;

    case 2:
        move_P2_move_P1();
        break;

    default:
        switch (g_state.plw[0].wu.pl_operator + (g_state.plw[1].wu.pl_operator * 2)) {
        case 1:
            move_P1_move_P2();
            break;

        case 2:
            move_P2_move_P1();
            break;

        default:
            switch ((g_state.plw[0].wu.routine_no[1] == 4) + ((g_state.plw[1].wu.routine_no[1] == 4) * 2)) {
            case 1:
                move_P1_move_P2();
                break;

            case 2:
                move_P2_move_P1();
                break;

            default:
                if (g_state.Game_timer & 1) {
                    move_P1_move_P2();
                    break;
                }

                move_P2_move_P1();
                break;
            }

            break;
        }

        break;
    }
}

/** @brief Updates P1 first, then P2 (used for specific frame ordering). */
static void move_P1_move_P2() {
    if (g_state.plw[0].do_not_move == 0) {
        Player_move(&g_state.plw[0], processed_lvbt(Convert_User_Setting(0)));
    }

    if (g_state.bg_app_stop == 0 && g_state.bg_app == 0 &&
        set_field_adjust_flag(&g_state.plw[0], g_state.scrr, 1) != 0) {
        set_field_adjust_flag(&g_state.plw[0], g_state.scrl, 0);
    }

    if (g_state.plw[1].do_not_move == 0) {
        Player_move(&g_state.plw[1], processed_lvbt(Convert_User_Setting(1)));
    }

    if (g_state.bg_app_stop == 0 && g_state.bg_app == 0 &&
        set_field_adjust_flag(&g_state.plw[1], g_state.scrr, 1) != 0) {
        set_field_adjust_flag(&g_state.plw[1], g_state.scrl, 0);
    }
}

/** @brief Updates P2 first, then P1 (used for specific frame ordering). */
static void move_P2_move_P1() {
    if (g_state.plw[1].do_not_move == 0) {
        Player_move(&g_state.plw[1], processed_lvbt(Convert_User_Setting(1)));
    }

    if (g_state.bg_app_stop == 0 && g_state.bg_app == 0 &&
        set_field_adjust_flag(&g_state.plw[1], g_state.scrr, 1) != 0) {
        set_field_adjust_flag(&g_state.plw[1], g_state.scrl, 0);
    }

    if (g_state.plw[0].do_not_move == 0) {
        Player_move(&g_state.plw[0], processed_lvbt(Convert_User_Setting(0)));
    }

    if (g_state.bg_app_stop == 0 && g_state.bg_app == 0 &&
        set_field_adjust_flag(&g_state.plw[0], g_state.scrr, 1) != 0) {
        set_field_adjust_flag(&g_state.plw[0], g_state.scrl, 0);
    }
}

/** @brief Stores player sprite data for after-image (shadow trail) rendering. */
void store_player_after_image_data() {
    s16 i;

    for (i = 47; i > 0; i--) {
        g_state.zanzou_table[0][i] = g_state.zanzou_table[0][i - 1];
        g_state.zanzou_table[1][i] = g_state.zanzou_table[1][i - 1];
    }

    for (i = 0; i < 2; i++) {
        g_state.zanzou_table[i]->pos_x = g_state.plw[i].wu.position_x;
        g_state.zanzou_table[i]->pos_y = g_state.plw[i].wu.position_y;
        g_state.zanzou_table[i]->pos_z = g_state.plw[i].wu.position_z;
        g_state.zanzou_table[i]->cg_num = g_state.plw[i].wu.cg_number;
        g_state.zanzou_table[i]->renew = g_state.plw[i].wu.renew_attack;
        g_state.zanzou_table[i]->hit_ix = g_state.plw[i].wu.cg_hit_ix;
        g_state.zanzou_table[i]->flip = g_state.plw[i].wu.rl_flag;
        g_state.zanzou_table[i]->cg_flp = g_state.plw[i].wu.cg_flip;
        g_state.zanzou_table[i]->kowaza = g_state.plw[i].wu.attack_type;
    }
}

/** @brief Applies damage correction (adjust) based on difficulty and character. */
static void check_damage_adjust() {
    g_state.plw[0].forced_movement = g_state.plw[0].scaling_remainder;
    g_state.plw[1].forced_movement = g_state.plw[1].scaling_remainder;

    if (g_state.plw[0].is_throwing && g_state.plw[1].is_being_thrown) {
        check_damage_adjust_throw(&g_state.plw[0], &g_state.plw[1]);
    } else if (g_state.plw[1].is_throwing && g_state.plw[0].is_being_thrown) {
        check_damage_adjust_throw(&g_state.plw[1], &g_state.plw[0]);
    } else {
        switch ((g_state.plw[0].scaling_remainder != 0) + ((g_state.plw[1].scaling_remainder != 0) * 2)) {
        case 1:
            check_damage_adjust_strike(&g_state.plw[0], &g_state.plw[1]);
            break;
        case 2:
            check_damage_adjust_strike(&g_state.plw[1], &g_state.plw[0]);
            break;
        }
    }

    g_state.plw[0].scaling_remainder = g_state.plw[1].scaling_remainder = 0;
}

/** @brief Applies throw damage correction based on difficulty and mode. */
static void check_damage_adjust_throw(PLW* as, PLW* ds) {
    if (as->kind_of_catch) {
        if (ds->scaling_remainder != 0) {
            as->wu.xyz[0].disp.pos += ds->scaling_remainder;
            as->forced_movement += ds->scaling_remainder;
            return;
        }

        if (g_state.bg_app_stop == 0 && g_state.bg_app == 0 && set_field_adjust_flag(as, g_state.scrr, 1) != 0) {
            set_field_adjust_flag(as, g_state.scrl, 0);
        }

        if (as->scaling_remainder != 0) {
            ds->wu.xyz[0].disp.pos += as->scaling_remainder;
            ds->forced_movement += as->scaling_remainder;
        }
    } else if (ds->scaling_remainder != 0) {
        as->wu.xyz[0].disp.pos += ds->scaling_remainder;
        as->forced_movement += ds->scaling_remainder;
    }
}

/** @brief Applies strike damage correction based on difficulty and mode. */
static void check_damage_adjust_strike(PLW* w1, PLW* w2) {
    if ((w1->dm_hos_flag != 0) && (w2->wu.hit_stop == 0)) {
        w2->wu.xyz[0].disp.pos += w1->scaling_remainder;
        w2->forced_movement += w1->scaling_remainder;
    }
}

/** @brief Checks if the round timer has expired. */
static s32 time_over_check() {
    if ((will_die() != 0) && (g_state.round_timer == 0)) {
        g_state.Winner_id = 0;
        g_state.Loser_id = 1;

        if (g_state.plw[0].wu.vital_new < g_state.plw[1].wu.vital_new) {
            g_state.Winner_id = 1;
            g_state.Loser_id = 0;
        }

        g_state.Conclusion_Flag = 1;
        g_state.Conclusion_Type = 2;
        setup_settle_rno(2);

        if (g_state.Demo_Flag) {
            request_center_message(2);
        }

        g_state.plw[0].wu.damage_vitality = g_state.plw[1].wu.damage_vitality = 0;
        g_state.Round_Result |= 1;
        return 1;
    }

    return 0;
}

/** @brief Returns 1 if either player's vitality has reached zero. */
static s32 will_die() {
    if (g_state.plw[0].wu.damage_vitality > g_state.plw[0].wu.vital_new) {
        return 0;
    }

    if (g_state.plw[1].wu.damage_vitality > g_state.plw[1].wu.vital_new) {
        return 0;
    }

    return 1;
}

/** @brief Sets up the settle routine number for a given KO type. */
static void setup_settle_rno(s16 kos) {
    g_state.pcon_rno[0] = 2;
    g_state.pcon_rno[1] = kos;
    g_state.pcon_rno[2] = 0;
    ca_check_flag = 0;
    g_state.pcon_dp_flag = true;
}

/** @brief Checks for round conclusion conditions (KO, time-over, draw). */
static void settle_check() {
    while (1) {
        switch ((g_state.plw[0].dead_flag) + (g_state.plw[1].dead_flag * 2)) {
        case 1:
            g_state.Winner_id = 1;
            g_state.Loser_id = 0;
            goto jump;

        case 2:
            g_state.Winner_id = 0;
            g_state.Loser_id = 1;

        jump:
            if (check_sa_resurrection(&g_state.plw[g_state.Loser_id]) == 0) {
                setup_gouki_wins();
                g_state.Round_Result |= g_state.plw[g_state.Loser_id].wu.damage_kind_of_arts;

                if ((g_state.Round_Result & 0x800) && g_state.gouki_wins) {
                    g_state.Forbid_Break = -1;
                    g_state.Shin_Gouki_BGM = 1;
                    Control_Music_Fade(0x96);
                    setup_settle_rno(4);
                    break;
                }

                setup_settle_rno(0);
                g_state.Conclusion_Flag = 1;
                g_state.Conclusion_Type = 0;

                if (g_state.Demo_Flag) {
                    request_center_message(0);
                }
            }

            break;

        case 3:
            if ((check_sa_resurrection(&g_state.plw[0]) == 0) && (check_sa_resurrection(&g_state.plw[1]) == 0)) {
                g_state.Conclusion_Flag = 1;
                g_state.Conclusion_Type = 1;
                setup_settle_rno(1);

                if (g_state.Demo_Flag) {
                    request_center_message(1);
                }
            } else {
                continue;
            }

            break;

        default:
            break;
        }

        break;
    }
}

/** @brief Checks if a player's SA provides automatic resurrection (Gill). */
static s32 check_sa_resurrection(PLW* wk) {
    if (check_sa_type_rebirth(wk) == 0) {
        return 0;
    }

    wk->chip_death_flag = 0;
    wk->dead_flag = 0;
    wk->resurrection_resv = 1;
    return 1;
}

/** @brief Checks if a player's SA provides rebirth-type resurrection. */
s32 check_sa_type_rebirth(PLW* wk) {
    if ((wk->spmv_ng_flag & DIP_GROUND_SUPER_ART_DISABLED) || (wk->spmv_ng_flag & DIP_AIR_SUPER_ART_DISABLED)) {
        return 0;
    }

    if (wk->sa->gauge_type != 3) {
        return 0;
    }

    if (wk->sa->ok != 1) {
        return 0;
    }

    return 1;
}

/** @brief Returns whether a player is currently in a knocked-down state. */
static s16 nekorobi_check(s8 ix) {
    s16 rnum = 0;

    if ((g_state.plw[ix].wu.routine_no[1] == 1) && (g_state.plw[ix].wu.routine_no[2] == 0) &&
        (g_state.plw[ix].wu.routine_no[3] > 2)) {
        rnum = 1;
    }

    return rnum;
}

/** @brief Returns whether a player is currently performing footwork movement. */
static s16 footwork_check(s8 ix) {
    s16 rnum = 0;

    if (g_state.plw[ix].wu.routine_no[1] == 0 && g_state.plw[ix].wu.routine_no[2] == 1) {
        rnum = 1;
    }

    return rnum;
}

/** @brief Sets screen-quake parameters for a player's landing/impact. */
void set_quake(PLW* wk) {
    if (wk->wu.hit_quake) {
        wk->wu.hit_quake--;
        wk->wu.next_x = quake_table[wk->wu.hit_quake];

        if (wk->wu.rl_flag) {
            wk->wu.next_x = -wk->wu.next_x;
        }
    } else {
        wk->wu.next_x = 0;
    }
}

/** @brief Adds a delta to the player's next-frame position. */
void add_next_position(PLW* wk) {
    wk->wu.position_x = wk->wu.xyz[0].disp.pos + wk->wu.next_x;
    wk->wu.position_y = wk->wu.xyz[1].disp.pos + wk->wu.next_y;
    wk->wu.position_z = wk->wu.next_z;
    wk->wu.next_y = 0;
}

/** @brief Sets up Gouki's win conditions for the hidden boss encounter. */
static void setup_gouki_wins() {
    g_state.gouki_wins = 0;

    if (g_state.plw[g_state.Winner_id].player_number == 14) {
        g_state.gouki_wins = 1;
    }
}

/** @brief Erases extra player-effect work items (projectiles, helpers). */
void erase_extra_plef_work() {
    effect_work_list_init(0, 0);
    effect_work_list_init(1, 1);
    effect_work_list_init(3, 0x91);
    effect_work_list_init(3, 0x93);
    effect_work_list_init(3, 0x94);
    effect_work_list_init(4, 0x81);
    effect_work_list_init(4, 0x25);
    effect_work_list_init(4, 0xAC);
    effect_work_list_init(6, -1);
}

/** @brief Sets up per-character base data and animation/damage tables. */
void setup_base_and_other_data() {
    make_texcash_work(3);
    make_texcash_work(4);
    make_texcash_work(6);
    g_state.plw[0].wu.my_mts = 3;
    g_state.plw[1].wu.my_mts = 4;
    set_base_data(&g_state.plw[0], 0);
    set_base_data(&g_state.plw[1], 1);
    g_state.plw[0].sa = &g_state.super_arts[0];
    g_state.plw[1].sa = &g_state.super_arts[1];
    g_state.plw[0].py = &g_state.stun_type[0];
    g_state.plw[1].py = &g_state.stun_type[1];
    setup_other_data(&g_state.plw[0]);
    setup_other_data(&g_state.plw[1]);
    effect_work_list_init(6, 0xC5);
    g_state.plw[0].gill_ccch_go = g_state.plw[1].gill_ccch_go = 0;
    effect_J7_init(&g_state.plw[0]);
    effect_J7_init(&g_state.plw[1]);
    effect_E5_init(&g_state.plw[0]);
    effect_E5_init(&g_state.plw[1]);

    if (g_state.plw[0].wu.my_priority == g_state.plw[1].wu.my_priority) {
        g_state.plw[0].the_same_players = g_state.plw[1].the_same_players = 1;
    }

    g_state.poison_flag[0] = 0;
    g_state.poison_flag[1] = 0;

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING ||
        g_state.Mode_Type == MODE_TRIALS) {
        effect_e3_init(&g_state.plw[0]);
        effect_e3_init(&g_state.plw[1]);
        effect_E4_init(&g_state.plw[0]);
        effect_E4_init(&g_state.plw[1]);
    }
}

/** @brief Sets up miscellaneous per-character data (colors, weight, shadow). */
static void setup_any_data() {
    set_base_data_tiny(&g_state.plw[0]);
    set_base_data_tiny(&g_state.plw[1]);
    setup_other_data(&g_state.plw[0]);
    setup_other_data(&g_state.plw[1]);
    effect_work_list_init(6, 0xC5);
    g_state.plw[0].gill_ccch_go = g_state.plw[1].gill_ccch_go = 0;
    effect_J7_init(&g_state.plw[0]);
    effect_J7_init(&g_state.plw[1]);
    effect_E5_init(&g_state.plw[0]);
    effect_E5_init(&g_state.plw[1]);

    if (g_state.plw[0].wu.my_priority == g_state.plw[1].wu.my_priority) {
        g_state.plw[0].the_same_players = g_state.plw[1].the_same_players = 1;
    }
}

/** @brief Sets base data (move tables, animation data) for a player work item. */
static void set_base_data(PLW* wk, s16 ix) {
    wk->wu.be_flag = 1;
    wk->wu.disp_flag = 0;
    wk->wu.blink_timing = ix;
    wk->wu.id = ix;
    wk->wu.work_id = 1;
    wk->wu.pl_operator = g_state.Operator_Status[ix];
    wk->wu.charset_id = plid_data[g_state.My_char[ix]];
    wk->wkey_flag = wk->dead_flag = 0;
    set_char_base_data(&wk->wu);
    wk->wu.target_adrs = &g_state.plw[(ix + 1) & 1];
    wk->player_number = g_state.My_char[ix];
    wk->wu.hit_adrs = wk->wu.target_adrs;
    wk->wu.dmg_adrs = wk->wu.target_adrs;
    cmd_init(wk);

    if (ix) {
        wk->wu.my_col_code |= 0x10;
    }

    wk->spmv_ng_flag = omop_spmv_ng_table[wk->wu.id];
    wk->special_move_disabled_flag2 = omop_spmv_ng_table2[wk->wu.id];
    wk->wu.weight_level = weight_lv_table[wk->player_number];
    set_player_shadow(wk);
    wk->wu.cg_olc_ix = wk->wu.cg_hit_ix = 0;
    wk->wu.graphic_overlap_index = wk->wu.olc_ix_table[wk->wu.cg_olc_ix];
    wk->wu.cg_ja = wk->wu.hit_ix_table[wk->wu.cg_hit_ix];

    set_jugde_area(&wk->wu);
}

/** @brief Sets base data for a metamorphosed (transformed) character. */
void set_base_data_metamorphose(PLW* wk, s16 dmid) {
    set_char_base_data(&wk->wu);

    if (wk->wu.id) {
        wk->wu.my_col_code |= 0x10;
    }

    cmd_init(wk);
    wk->spmv_ng_flag = omop_spmv_ng_table[dmid];
    wk->special_move_disabled_flag2 = omop_spmv_ng_table2[dmid];
    set_player_shadow(wk);
}

/** @brief Sets up compact base data for a reduced-data character variant. */
static void set_base_data_tiny(PLW* wk) {
    wk->wu.charset_id = plid_data[g_state.My_char[wk->wu.id]];
    wk->player_number = g_state.My_char[wk->wu.id];
    set_char_base_data(&wk->wu);

    if (wk->wu.id) {
        wk->wu.my_col_code |= 0x10;
    }

    wk->wu.be_flag = 1;
    wk->wu.disp_flag = 0;
    wk->wkey_flag = wk->dead_flag = 0;
    cmd_init(wk);
    wk->spmv_ng_flag = omop_spmv_ng_table[wk->wu.id];
    wk->special_move_disabled_flag2 = omop_spmv_ng_table2[wk->wu.id];
    wk->wu.weight_level = weight_lv_table[wk->player_number];
    set_player_shadow(wk);
}

/** @brief Configures the player's shadow sprite parameters. */
void set_player_shadow(PLW* wk) {
    wk->wu.shadow_flag = 1;
    wk->wu.shadow_prio = 68;
    wk->wu.shadow_x = kage_base[wk->player_number][0];
    wk->wu.shadow_char = kage_base[wk->player_number][1];
}

/** @brief Sets up other per-player data (SA config, damage tables). */
static void setup_other_data(PLW* wk) {
    s16 i;

    if (wk->player_number == 0) {
        setup_GILL_exsa_obj();
    }

    for (i = 0; i < 4; i++) {
        effect_01_init(&wk->wu, i);
    }

    effect_k5_init(wk);
    effect_00_init(&wk->wu);
}

/** @brief Clears the chain-combo EX check state for a player. */
void clear_chainex_check(s16 ix) {
    s16 i;

    for (i = 0; i < 36; i++) {
        chainex_check[ix][i] = 0;
    }
}

/** @brief Sets the stun (kizetsu/piyo) status parameters for a player. */
void set_kizetsu_status(s16 ix) {
    s16 plnum = g_state.My_char[ix];

    g_state.stun_type[ix].flag = 0;
    g_state.stun_type[ix].time = 0;
    g_state.stun_type[ix].now.timer = 0;
    g_state.stun_type[ix].store = 0;
    g_state.stun_type[ix].recover = pl_nr_piyo_tbl[plnum];
    g_state.stun_type[ix].genkai = pl_piyo_tbl[plnum] + stun_gauge_len_omake[omop_stun_gauge_len[ix]];

    if (g_state.stun_type[ix].genkai < 56) {
        g_state.stun_type[ix].genkai = 56;
    }

    if (g_state.stun_type[ix].genkai > 72) {
        g_state.stun_type[ix].genkai = 72;
    }
}

/** @brief Clears the stun gauge and related work for a player. */
void clear_kizetsu_point(PLW* wk) {
    wk->py->flag = 0;
    wk->py->time = 0;
    wk->py->now.timer = 0;
    wk->py->store = 0;
    wk->py->recover = pl_nr_piyo_tbl[wk->player_number];
}

/** @brief Sets up the Super Arts status (gauge, stock, type) for a player. */
void set_super_arts_status(s16 ix) {
    const SA_DATA* saptr;

    if (g_state.cmd_sel[ix] || g_state.no_sa[ix]) {
        saptr = &super_arts_DATA[g_state.My_char[ix]][g_state.Super_Arts[ix]];
    } else {
        saptr = &super_arts_data[g_state.My_char[ix]][g_state.Super_Arts[ix]];
    }

    g_state.super_arts[ix].kind_of_arts = g_state.Super_Arts[ix];
    g_state.super_arts[ix].nmsa_g_ix = saptr->nmsa_g_ix;
    g_state.super_arts[ix].exsa_g_ix = saptr->exsa_g_ix;
    g_state.super_arts[ix].exs2_g_ix = saptr->exs2_g_ix;
    g_state.super_arts[ix].nmsa_a_ix = saptr->nmsa_a_ix;
    g_state.super_arts[ix].exsa_a_ix = saptr->exsa_a_ix;
    g_state.super_arts[ix].exs2_a_ix = saptr->exs2_a_ix;
    g_state.super_arts[ix].ex4th_full = saptr->ex4th_full;
    g_state.super_arts[ix].gauge_type = saptr->gauge_type;
    g_state.super_arts[ix].gt2 = saptr->gauge_type;
    g_state.super_arts[ix].gauge_len = remake_sa_gauge_len(ix, saptr->gauge_len);
    g_state.super_arts[ix].store_max = remake_sa_store_max(ix, saptr->store_max);
    g_state.super_arts[ix].dtm = saptr->dtm;
    g_state.super_arts[ix].dtm_mul = 1;
    g_state.super_arts[ix].store = 0;
    g_state.super_arts[ix].gauge.s.h = 0;
    g_state.super_arts[ix].gauge.s.l = -1;
    g_state.super_arts[ix].sa_rno = 0;
    g_state.super_arts[ix].ok = 0;
}

/** @brief Adjusts SA stock maximum based on character-specific rules. */
static s16 remake_sa_store_max(s16 ix, s16 store_max) {
    s16 num = store_max + sag_stock_omake[omop_sag_max_ix[ix]];

    if (num <= 0) {
        num = 1;
    }

    if (num > 9) {
        num = 9;
    }

    return num;
}

/** @brief Adjusts SA gauge length based on character-specific rules. */
static s16 remake_sa_gauge_len(s16 ix, s16 gauge_len) {
    s16 num = gauge_len + sag_length_omake[omop_sag_len_ix[ix]] * 8;

    if (num < 0x40) {
        num = 0x40;
    }

    if (num > 0x80) {
        num = 0x80;
    }

    return num;
}

/** @brief Sets up SA status using DC (Dreamcast) balance data. */
void set_super_arts_status_dc(s16 ix) {
    const SA_DATA* saptr;

    if (g_state.cmd_sel[ix] || g_state.no_sa[ix]) {
        saptr = &super_arts_DATA[g_state.My_char[ix]][g_state.Super_Arts[ix]];
    } else {
        saptr = &super_arts_data[g_state.My_char[ix]][g_state.Super_Arts[ix]];
    }

    g_state.super_arts[ix].kind_of_arts = g_state.Super_Arts[ix];
    g_state.super_arts[ix].nmsa_g_ix = saptr->nmsa_g_ix;
    g_state.super_arts[ix].exsa_g_ix = saptr->exsa_g_ix;
    g_state.super_arts[ix].exs2_g_ix = saptr->exs2_g_ix;
    g_state.super_arts[ix].nmsa_a_ix = saptr->nmsa_a_ix;
    g_state.super_arts[ix].exsa_a_ix = saptr->exsa_a_ix;
    g_state.super_arts[ix].exs2_a_ix = saptr->exs2_a_ix;
    g_state.super_arts[ix].ex4th_full = saptr->ex4th_full;
    g_state.super_arts[ix].gauge_type = saptr->gauge_type;
    g_state.super_arts[ix].gauge_len = remake_sa_gauge_len(ix, saptr->gauge_len);
    g_state.super_arts[ix].store_max = remake_sa_store_max(ix, saptr->store_max);
    g_state.super_arts[ix].dtm = saptr->dtm;
    g_state.super_arts[ix].dtm_mul = 1;
}

/** @brief Clears a player's SA gauge and stock to zero. */
void clear_super_arts_point(PLW* wk) {
    wk->sa->store = 0;
    wk->sa->gauge.s.h = 0;
    wk->sa->gauge.s.l = -1;
    wk->sa->mp_rno = 0;
    wk->sa->mp_rno2 = 0;
    wk->sa->sa_rno = 0;
    wk->sa->sa_rno2 = 0;
    wk->sa->ex_rno = 0;
    wk->sa->mp = 0;
    wk->sa->ok = 0;
    wk->sa->ex = 0;
    wk->sa->bacckup_g_h = 0;
}

/** @brief Checks if an active combo has ended and finalizes combo data. */
s16 check_combo_end(s16 ix) {
    s16 rnum;

    if (g_state.plw[ix].py->flag) {
        return 1;
    }

    if (g_state.plw[ix].is_being_thrown) {
        return 1;
    }

    if (g_state.pcon_rno[0] == 2 && g_state.pcon_rno[1] == 0 && g_state.pcon_rno[2] == 2) {
        return 0;
    }

    if (g_state.plw[ix].wu.cg_ja.boix == 0 && g_state.plw[ix].wu.cg_ja.cuix == 0 &&
        g_state.plw[ix].wu.pat_status == 38) {
        return 0;
    }

    if (g_state.plw[ix].invuln_flag) {
        return 0;
    }

    if (g_state.plw[ix].wu.routine_no[1] != 1 && g_state.plw[ix].wu.routine_no[1] != 3) {
        return 0;
    }

    if (g_state.plw[ix].old_gdflag != g_state.plw[ix].guard_flag) {
        if (g_state.plw[ix].guard_flag == 0) {
            rnum = 0;
        } else {
            rnum = 1;
        }
    } else if (g_state.plw[ix].guard_flag == 0) {
        rnum = 0;
    } else {
        rnum = 1;
    }

    return rnum;
}

/** @brief Sets the scroll direction flags for left/right screen boundaries. */
static void set_scrrrl() {
    s16 scrc = get_center_position();

    g_state.scrr = scrc + 192;
    g_state.scrl = scrc - 192;
}
