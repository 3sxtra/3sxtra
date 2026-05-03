/**
 * @file effk5.c
 * Effect: Lookup Index / Check Effect
 */

#include "sf33rd/Source/Game/effect/effect_k5_lookup_index_check.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"

// Types

typedef struct {
    s16 data[4];
} K5Data;

typedef struct {
    u16 index;
    u16 rno;
    Reg32SpReal a[4];
    Reg32SpReal d[4];
    Reg32CalPos r[4];
} MVJ;

typedef union {
    u32 swi;
    struct {
        u16 l;
        u16 h;
    } sws;
    struct {
        u8 ll;
        u8 l;
        u8 h;
        u8 hh;
    } swc;
} MVSW;

// Data

const s16 lookup_index[10] = { 0, 0, 0, 0, 1, 1, 1, 1, 0xFFFF, 0xFFFF };

const s8 k5_exc_check[125] = { 1, 2, 0, 2, 2, 2, 2, 1, 1, 1, 2, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,
                               2, 1, 2, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1, 0,
                               0, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1,
                               1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2 };

// Forward decls

static void k5_init_data(State* mwk, MVJ* mvj, u16* ixtbl);
static void k5_init_data_copy(MVJ* mvj, K5Data* dad, s16 num);
static void k5_init_data_copy2(K5Data* dad, MVJ* mvj, s16 num);
static void get_table_adrs_K5(State* wk);
static void k5_add_sub(MVJ* mvj);
static void get_okuri_time(State* ewk, State* mwk, MVJ* mvj);
static void k5_main_process(State* ewk, State* mwk, MVJ* mvj);
static void init_k5_work(State* ewk, State* mwk, MVJ* mvj);
static void get_master_table_address(State* ewk, State* mwk);
static s32 get_cal_work(State* wk);
static void k5_decode_new_hit_index(State* wk, MVJ* mvj, u16 mf);
static u32 decode_mvsw(u16 flag);

// Funcs

void effect_k5_move(State_Other* ewk) {
    State* mwk = (State*)ewk->my_master;
    MVJ* mvj;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0] += 1;

        if (get_cal_work(&ewk->wu) == -1) {
            ewk->wu.routine_no[0] = 3;
            return;
        }

        // This line is bullshit. Effect K5 needs some space for MVJ manipulation. Instead of allocating
        // space for that somewhere else they decided to use some of the space dedicated to effect work.
        // Why did they choose routine_no as the starting offset specifically? They did that because it's
        // the first var of State that is not used for effect scheduling. If they chose an earlier address
        // that would lead to crashes and infinite loops. Fun times!
        // There's one more line just like this one down below.
        mvj = (MVJ*)(((State*)ewk->wu.target_adrs)->routine_no);

        init_k5_work(&ewk->wu, mwk, mvj);
        ewk->wu.old_routine_no[1] = mwk->anim_hurtbox_index;
        get_table_adrs_K5(mwk);
        k5_init_data(mwk, mvj, (u16*)(&mwk->cg_ja));
        break;

    case 1:
        if (ewk->wu.dead_f == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            return;
        }

        if (((PLW*)mwk)->bbox_ram_index != ewk->wu.myself) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            return;
        }

        get_master_table_address(&ewk->wu, mwk);
        mvj = (MVJ*)(((State*)ewk->wu.target_adrs)->routine_no);

        if (mwk->phase_k5_exec_ok) {
            mwk->phase_k5_exec_ok = 0;

            if (mwk->phase_k5_init_flag || (ewk->wu.old_routine_no[1] != mwk->anim_hurtbox_index)) {
                mwk->phase_k5_init_flag = 0;
                ewk->wu.old_routine_no[1] = mwk->anim_hurtbox_index;
                ewk->wu.routine_no[1] = 0;
                k5_init_data(mwk, mvj, (u16*)(&mwk->cg_ja));
            }

            k5_main_process(&ewk->wu, mwk, mvj);
        }

        k5_init_data_copy2((K5Data*)&g_state.rambod[mwk->id], mvj, 4);
        k5_init_data_copy2((K5Data*)&g_state.ramhan[mwk->id], mvj + 4, 4);
        mwk->body_hurtbox = &g_state.rambod[mwk->id];
        mwk->hand_hurtbox = &g_state.ramhan[mwk->id];
        break;

    case 2:
        Release_Effect((State*)ewk->wu.target_adrs);
        /* fallthrough */

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void k5_main_process(State* ewk, State* mwk, MVJ* mvj) {
    s16 i;

    switch (ewk->routine_no[1]) {
    case 0:
        get_okuri_time(ewk, mwk, mvj);
        break;

    case 1:
        for (i = 0; i < 8; i++) {
            if (mvj[i].rno) {
                k5_add_sub(&mvj[i]);
            }
        }

        break;
    }
}

typedef union {
    u32* cpl;
    u16* cps;
    u8* cpc;
} GOTCP;

static void get_okuri_time(State* ewk, State* mwk, MVJ* mvj) {
    GOTCP gotcp;
    ST st;
    s16 exc;
    u16 now_mf;

    if ((mwk->char_graphic_data_type != 2) && (mwk->cg_ja.mf.full & 0x1010)) {
        now_mf = mwk->cg_ja.mf.full;
        exc = 0;
        ewk->old_routine_no[0] = mwk->cg_ctr;
        ewk->graphic_index = mwk->graphic_index;

        while (1) {
            ewk->graphic_index += mwk->char_graphic_data_type;
            gotcp.cpl = &mwk->set_char_ad[ewk->graphic_index];

            if (gotcp.cps[0] >= 0x100) {
                st.l = gotcp.cpl[2];
                st.l *= 8;
                ewk->anim_hurtbox_index = st.w.h & 0x1FF;

                if (ewk->old_routine_no[1] == ewk->anim_hurtbox_index) {
                    if ((gotcp.cpc[1] != 0xFF) || (gotcp.cpc[1] < 0xC8)) {
                        ewk->old_routine_no[0] += gotcp.cpc[1];
                    }

                    continue;
                }

                if (ewk->old_routine_no[0] >= 2) {
                    k5_decode_new_hit_index(ewk, mvj, now_mf);
                    ewk->routine_no[1] = 1;
                    return;
                }

                break;
            }

            if (k5_exc_check[gotcp.cps[0]] == 2) {
                break;
            }

            if (k5_exc_check[gotcp.cps[0]]) {
                continue;
            }

            if (exc++ >= 4) {
                break;
            }

            switch (gotcp.cps[0]) {
            case 2:
                ewk->graphic_index = (gotcp.cps[3] - 2) * mwk->char_graphic_data_type;
                break;

            case 49:
                if ((g_state.test_flag == 0) || (g_state.ixbfw_cut == 0)) {
                    ewk->graphic_index += (gotcp.cps[3] - 1) * mwk->char_graphic_data_type;
                }

                break;

            case 50:
                if ((g_state.test_flag == 0) || (g_state.ixbfw_cut == 0)) {
                    ewk->graphic_index -= (gotcp.cps[3] + 1) * mwk->char_graphic_data_type;
                }

                break;
            }
        }
    }

    ewk->old_routine_no[0] = 1;
    ewk->routine_no[1] = 2;
}

static void k5_decode_new_hit_index(State* wk, MVJ* mvj, u16 mf) {
    s16 i;
    s16 t0;
    s16 t1;
    MVSW mvsw;

    get_table_adrs_K5(wk);
    mvsw.swi = decode_mvsw(mf);

    if (wk->cg_ja.body_hurtbox_index != mvj[0].index) {
        for (i = 0; i < 4; i++) {
            if (mvj[i].r[1].pos.h != 0) {
                wk->xyz[0].disp.pos = mvj[i].r[0].pos.h;
                wk->xyz[1].disp.pos = mvj[i].r[1].pos.h;

                if ((t1 = wk->body_hurtbox->body_dm[i][1])) {
                    t0 = wk->body_hurtbox->body_dm[i][0];
                } else {
                    t0 = wk->xyz[0].disp.pos + wk->xyz[1].disp.pos / 2;
                }

                cal_all_speed_data(wk, wk->old_routine_no[0], t0, t1, mvsw.swc.hh, mvsw.swc.l);
                mvj[i].r[0].cal = wk->xyz[0].cal;
                mvj[i].r[1].cal = wk->xyz[1].cal;
                mvj[i].a[0].sp = wk->mvxy.a[0].sp;
                mvj[i].d[0].sp = wk->mvxy.d[0].sp;
                mvj[i].a[1].sp = wk->mvxy.a[1].sp;
                mvj[i].d[1].sp = wk->mvxy.d[1].sp;
                wk->xyz[0].disp.pos = mvj[i].r[2].pos.h;
                wk->xyz[1].disp.pos = mvj[i].r[3].pos.h;

                if ((t1 = wk->body_hurtbox->body_dm[i][3])) {
                    t0 = wk->body_hurtbox->body_dm[i][2];
                } else {
                    t0 = wk->xyz[0].disp.pos + wk->xyz[1].disp.pos / 2;
                }

                cal_all_speed_data(wk, wk->old_routine_no[0], t0, t1, mvsw.swc.h, mvsw.swc.ll);
                mvj[i].r[2].cal = wk->xyz[0].cal;
                mvj[i].r[3].cal = wk->xyz[1].cal;
                mvj[i].a[2].sp = wk->mvxy.a[0].sp;
                mvj[i].d[2].sp = wk->mvxy.d[0].sp;
                mvj[i].a[3].sp = wk->mvxy.a[1].sp;
                mvj[i].d[3].sp = wk->mvxy.d[1].sp;
                mvj[i].rno = 1;
            } else {
                mvj[i].rno = 0;
            }

            mvj[i].index = wk->cg_ja.body_hurtbox_index;
        }
    }

    if (mvj[4].index != (wk->cg_ja.behind_hurtbox_index + wk->cg_ja.hand_hurtbox_index)) {
        for (i = 4; i < 8; i++) {
            if (mvj[i].r[1].pos.h != 0) {
                wk->xyz[0].disp.pos = mvj[i].r[0].pos.h;
                wk->xyz[1].disp.pos = mvj[i].r[1].pos.h;

                if ((t1 = wk->hand_hurtbox->hand_dm[i - 4][1])) {
                    t0 = wk->hand_hurtbox->hand_dm[i - 4][0];
                } else {
                    t0 = wk->xyz[0].disp.pos + wk->xyz[1].disp.pos / 2;
                }

                cal_all_speed_data(wk, wk->old_routine_no[0], t0, t1, mvsw.swc.hh, mvsw.swc.l);
                mvj[i].r[0].cal = wk->xyz[0].cal;
                mvj[i].r[1].cal = wk->xyz[1].cal;
                mvj[i].a[0].sp = wk->mvxy.a[0].sp;
                mvj[i].d[0].sp = wk->mvxy.d[0].sp;
                mvj[i].a[1].sp = wk->mvxy.a[1].sp;
                mvj[i].d[1].sp = wk->mvxy.d[1].sp;
                wk->xyz[0].disp.pos = mvj[i].r[2].pos.h;
                wk->xyz[1].disp.pos = mvj[i].r[3].pos.h;

                if ((t1 = wk->hand_hurtbox->hand_dm[i - 4][3])) {
                    t0 = wk->hand_hurtbox->hand_dm[i - 4][2];
                } else {
                    t0 = wk->xyz[0].disp.pos + wk->xyz[1].disp.pos / 2;
                }

                cal_all_speed_data(wk, wk->old_routine_no[0], t0, t1, mvsw.swc.h, mvsw.swc.ll);
                mvj[i].r[2].cal = wk->xyz[0].cal;
                mvj[i].r[3].cal = wk->xyz[1].cal;
                mvj[i].a[2].sp = wk->mvxy.a[0].sp;
                mvj[i].d[2].sp = wk->mvxy.d[0].sp;
                mvj[i].a[3].sp = wk->mvxy.a[1].sp;
                mvj[i].d[3].sp = wk->mvxy.d[1].sp;
                mvj[i].rno = 1;
            } else {
                mvj[i].rno = 0;
            }

            mvj[i].index = wk->cg_ja.behind_hurtbox_index + wk->cg_ja.hand_hurtbox_index;
        }
    }
}

static u32 decode_mvsw(u16 flag) {
    MVSW mvsw;

    mvsw.swi = flag;

    if (flag & 0x1000) {
        mvsw.swc.hh = mvsw.swc.h = mvsw.swc.l;
        mvsw.swc.hh >>= 2;
        mvsw.swc.hh &= 3;
        mvsw.swc.h &= 3;
    } else {
        mvsw.sws.h = 0xFFFF;
    }

    if (flag & 0x10) {
        mvsw.swc.l = mvsw.swc.ll;
        mvsw.swc.l >>= 2;
        mvsw.swc.l &= 3;
        mvsw.swc.ll &= 3;
    } else {
        mvsw.sws.l = 0xFFFF;
    }

    return mvsw.swi;
}

static void get_table_adrs_K5(State* wk) {
    wk->cg_ja = wk->hit_ix_table[wk->anim_hurtbox_index];
    wk->body_hurtbox = &wk->body_adrs[wk->cg_ja.body_hurtbox_index];
    wk->hand_hurtbox = &wk->hand_adrs[wk->cg_ja.behind_hurtbox_index + wk->cg_ja.hand_hurtbox_index];
}

static void init_k5_work(State* ewk, State* mwk, MVJ* mvj) {
    s16 i;

    for (i = 0; i < 10; i++) {
        mvj[i].index = mvj[i].rno = 0;
    }

    ewk->anim_hurtbox_index = mwk->anim_hurtbox_index;
    ewk->hit_ix_table = mwk->hit_ix_table;
    ewk->body_adrs = mwk->body_adrs;
    ewk->hand_adrs = mwk->hand_adrs;
    mwk->phase_k5_init_flag = 1;
}

static void get_master_table_address(State* ewk, State* mwk) {
    ewk->hit_ix_table = mwk->hit_ix_table;
    ewk->body_adrs = mwk->body_adrs;
    ewk->hand_adrs = mwk->hand_adrs;
}

static void k5_init_data(State* mwk, MVJ* mvj, u16* ixtbl) {
    s16 i;

    for (i = 0; i < 8; i++) {
        mvj[i].rno = 0;
        mvj[i].index = ixtbl[lookup_index[i]];
    }

    k5_init_data_copy(mvj, (K5Data*)mwk->body_adrs[mwk->cg_ja.body_hurtbox_index].body_dm, 4);
    k5_init_data_copy(mvj + 4, (K5Data*)mwk->hand_adrs[mwk->cg_ja.behind_hurtbox_index + mwk->cg_ja.hand_hurtbox_index].hand_dm, 4);
}

static void k5_init_data_copy(MVJ* mvj, K5Data* dad, s16 num) {
    s16 i;

    for (i = 0; i < num; i++) {
        mvj[i].r[0].pos.h = dad[i].data[0];
        mvj[i].r[1].pos.h = dad[i].data[1];
        mvj[i].r[2].pos.h = dad[i].data[2];
        mvj[i].r[3].pos.h = dad[i].data[3];
    }
}

static void k5_init_data_copy2(K5Data* dad, MVJ* mvj, s16 num) {
    s16 i;

    for (i = 0; i < num; i++) {
        dad[i].data[0] = mvj[i].r[0].pos.h;
        dad[i].data[1] = mvj[i].r[1].pos.h;
        dad[i].data[2] = mvj[i].r[2].pos.h;
        dad[i].data[3] = mvj[i].r[3].pos.h;
    }
}

static s32 get_cal_work(State* wk) {
    State* fwk;
    s16 ix;

    if ((ix = Acquire_Effect(7)) == -1) {
        return -1;
    }

    fwk = (State*)frw[ix];
    wk->target_adrs = fwk;
    fwk->be_flag = 1;
    fwk->id = 0xCD;
    return 0;
}

static void k5_add_sub(MVJ* mvj) {
    s16 i;

    for (i = 0; i < 4; i++) {
        mvj->r[i].cal += mvj->a[i].sp;
    }

    for (i = 0; i < 4; i++) {
        mvj->a[i].sp += mvj->d[i].sp;
    }
}

s32 effect_k5_init(PLW* wk) {
    State_Other* ewk;
    s16 ix;

    if (g_state.Bonus_Game_Flag == 0x14) {
        return -1;
    }

    if ((ix = Acquire_Effect(0)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 0xCD;
    ewk->wu.work_id = 0x10;
    ewk->my_master = wk;
    wk->bbox_ram_index = ewk->wu.myself;
    return 0;
}
