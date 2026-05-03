/**
 * @file charset.c
 * The interpreter for character animation and logic scripts
 */

#include "sf33rd/Source/Game/engine/charset.h"
#include "game_state.h"
#include "arcade/arcade_balance.h"
#include "arcade/arcade_char_data.h"
#include "common.h"
#include "constants.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_xx_move_and_init_jumptables.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/player_special_attacks.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/sound/se_data.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/system_director.h"

#define LO_2_BYTES(_val) (((s16*)&(_val))[0])
#define HI_2_BYTES(_val) (((s16*)&(_val))[1])
#define WK_AS_PLW ((PLW*)wk)

extern s32 (*const decode_chcmd[125])();
extern s32 (*const decode_if_lever[16])();
extern const s16 jphos_table[16];
extern const s16 kezuri_pow_table[5];

static s16 decord_if_jump(State* wk, CommandState* cpc, s16 ix);
static u16 get_comm_if_lever(State* wk);
static u16 get_comm_if_shot(State* wk);
static u16 get_comm_if_shot_now_off(State* wk);
static u16 get_comm_if_shot_now(State* wk);
static u16 get_comm_if_lvsh(State* wk);
static u8 get_comm_djmp_lever_dir(PLW* wk);
static void setup_comm_retmj(State* wk);
static u16 check_xcopy_filter_se_req(State* wk);
static void check_cgd_patdat2(State* wk);
static void setup_metamor_kezuri(State* wk);

/** @brief Initializes character animation move with a given index. */
void set_char_move_init(State* wk, s16 kind_of_char, s16 index) {
    wk->current_char_type = kind_of_char;
    wk->char_index = index;

#if CPS3
    wk->set_char_ad = (u32*)wk->char_table[kind_of_char][index];

    const u32* src = wk->set_char_ad;
    u32* dst = (u32*)&wk->cg_ctr;

    for (int i = 0; i < 6; i++) {
        dst[i] = 0;
    }

    dst[-1] = src[-1];
    dst[-2] = src[-2];
#else
    wk->set_char_ad = &wk->char_table[kind_of_char][wk->char_table[kind_of_char][index] / sizeof(u32)];
    setupCharTableData(wk, 1, 1);
#endif

    wk->graphic_index = -wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->cg_next_ix = 0;
    wk->old_cgnum = 0;
    wk->cg_wca_ix = 0;
    wk->cmd_roa_state.kind_of_char = wk->current_char_type;
    wk->cmd_roa_state.ix = wk->char_index;
    wk->cmd_roa_state.pat = 1;
    wk->script_register_bank[8] = 0;
    wk->script_register_bank[15] = 0;

#if !CPS3
    wk->move_type = wk->attack_type;
#endif

    if (wk->work_id & 0xF) {
        wk->attack_art_type = acatkoa_table[wk->attack_type];
    }

    if (wk->work_id == 1) {
        ((PLW*)wk)->tc_1st_flag = 0; // TODO: Confirm CPS3 match

        if (wk->current_char_type == 4 || wk->current_char_type == 5) {
            grade_add_same_move(wk->id);
        }

        ((PLW*)wk)->jump_attack_routine = 0; // TODO: Confirm CPS3 match
        pp_pulpara_remake_at_init(wk);
    }

    wk->phase_k5_init_flag = 1; // TODO: Confirm CPS3 match
    char_move(wk);
}

/** @brief Sets up character table pointers and animation data. */
void setupCharTableData(State* wk, s32 clr, s32 info) {
    u32* src;
    s32 i;

    if (info != 0) {
        src = wk->set_char_ad;
        // Previously: dst[-1] = src[-1]; dst[-2] = src[-2];
        // dst[-1] corresponds to: char_graphic_data_type, pat_status, attack_type
        // dst[-2] corresponds to: sp_tech_id, total_att_set, total_paring, hit_range

        // Emulate the raw copy for header to maintain binary compatibility with assets
        // without relying on pointer arithmetic from &cg_type.
        // We assume CharState layout matches the packed expectation.

        // This is safe because CharState is unionized or packed to match.
        // Note: This relies on the new union in State.
        wk->char_state.header_block_2 = src[-1]; // Offset -1
        wk->char_state.header_block_1 = src[-2]; // Offset -2

        if (clr != 0) {
            for (i = 0; i < 6; i++) {
                wk->char_state.body.raw[i] = 0;
            }
        }
    } else {
        src = wk->set_char_ad + wk->graphic_index;

        // Copy body from src to body.raw
        for (i = 0; i < wk->char_graphic_data_type; i++) {
            wk->char_state.body.raw[i] = src[i];
        }
    }
}

/** @brief Extended char move init with IP and SCF parameters. */
void set_char_move_init2(State* wk, s16 kind_of_char, s16 index, s16 ip, s16 scf) {
    u8 pst;
    u8 move_type;

#if !CPS3
    if (index < 0) {
        index = 0;
    }

    if (ip <= 0) {
        ip = 1;
    }
#endif

    pst = wk->pat_status;
    move_type = wk->attack_type;
    wk->current_char_type = kind_of_char;
    wk->char_index = index;

#if CPS3
    wk->set_char_ad = (u32*)wk->char_table[kind_of_char][index];

    const u32* src = wk->set_char_ad;
    u32* dst = (u32*)&wk->cg_ctr;

    for (int i = 0; i < 6; i++) {
        dst[i] = 0;
    }

    dst[-1] = src[-1];
    dst[-2] = src[-2];
#else
    wk->set_char_ad = wk->char_table[kind_of_char] + (wk->char_table[kind_of_char][index] / sizeof(u32));
    setupCharTableData(wk, 1, 1);
#endif

    wk->graphic_index = (ip - 1) * wk->char_graphic_data_type - wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->cg_next_ix = 0;
    wk->old_cgnum = 0;
    wk->cg_wca_ix = 0;

    if (wk->cmd_roa_state.pat == 0) {
        wk->cmd_roa_state.kind_of_char = wk->current_char_type;
        wk->cmd_roa_state.ix = wk->char_index;
        wk->cmd_roa_state.pat = 1;
    }

    if (scf) {
        wk->pat_status = pst;
        wk->attack_type = move_type;
    } else {
#if !CPS3
        wk->move_type = wk->attack_type;
#endif
    }

    if (wk->work_id & 0xF) {
        wk->attack_art_type = acatkoa_table[wk->attack_type];
    }

    wk->phase_k5_init_flag = 1; // TODO: Confirm CPS3 match
    char_move(wk);
}

/** @brief Extended set char move init preserving current flags. */
void exset_char_move_init(State* wk, s16 kind_of_char, s16 index) {
    u8 now_ctr;

    wk->current_char_type = kind_of_char;
    wk->char_index = index;

#if CPS3
    wk->set_char_ad = (u32*)wk->char_table[kind_of_char][index];
#else
    wk->set_char_ad = &wk->char_table[kind_of_char][wk->char_table[kind_of_char][index] / sizeof(u32)];
#endif

    now_ctr = wk->cg_ctr;

#if CPS3
    u32* dst = (u32*)&wk->cg_ctr;
    const u32* src = wk->set_char_ad + wk->graphic_index;

    for (int i = 0; i < wk->char_graphic_data_type; i++) {
        dst[i] = src[i];
    }

#else
    setupCharTableData(wk, 0, 0);
#endif

    wk->cg_ctr = now_ctr;
    wk->cmd_roa_state.kind_of_char = wk->current_char_type;
    wk->cmd_roa_state.ix = wk->char_index;
    wk->cmd_roa_state.pat = 1;
    wk->phase_k5_init_flag = 1;  // TODO: Confirm CPS3 match
    check_cgd_patdat2(wk); // TODO: Confirm CPS3 match
}

/** @brief Advances the character animation Z-axis frame. */
void char_move_z(State* wk) {
    if (g_state.test_flag) {
        wk->cg_next_ix = 0;
    }

    wk->cg_ctr = 1;
    wk->phase_k5_init_flag = 1; // TODO: Confirm CPS3 match
    char_move(wk);
}

/** @brief Advances character animation with WCA (wait-for-command-A). */
void char_move_wca(State* wk) {
    wk->cg_next_ix = 0;
    wk->graphic_index = (wk->cg_wca_ix - 1) * wk->char_graphic_data_type - wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->phase_k5_init_flag = 1;
    char_move(wk);
}

/** @brief Initializes WCA mode for the character animation. */
void char_move_wca_init(State* wk) {
    wk->cg_next_ix = 0;
    wk->graphic_index = (wk->cg_wca_ix - 1) * wk->char_graphic_data_type - wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->phase_k5_init_flag = 1;
}

/** @brief Script command: WCA (wait-for-command-A) mode toggle. */
static s32 comm_wca(State* wk, CommandState* /* unused */) {
    char_move_wca_init(wk);
    return 1;
}

/** @brief Advances character animation at a specific index. */
void char_move_index(State* wk, s16 ix) {
    wk->cg_next_ix = 0;
    wk->graphic_index = (ix - 1) * wk->char_graphic_data_type - wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->phase_k5_init_flag = 1;
    char_move(wk);
}

/** @brief Character move to command-jump-A target. */
void char_move_cmja(State* wk) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_1.kind_of_char, wk->cmd_jump_addr_1.ix, wk->cmd_jump_addr_1.pat, 0);
}

#if CPS3
void char_move_cmj2(State* wk) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_2.kind_of_char, wk->cmd_jump_addr_2.ix, wk->cmd_jump_addr_2.pat, 0);
}

void char_move_cmj3(State* wk) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_3.kind_of_char, wk->cmd_jump_addr_3.ix, wk->cmd_jump_addr_3.pat, 0);
}
#endif

/** @brief Character move to command-jump-4 target. */
void char_move_cmj4(State* wk) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_4.kind_of_char, wk->cmd_jump_addr_4.ix, wk->cmd_jump_addr_4.pat, 0);
}

#if CPS3
void char_move_cmoa(State* wk) {
    set_char_move_init2(wk, wk->cmd_roa_state.kind_of_char, wk->cmd_roa_state.ix, wk->cmd_roa_state.pat, 0);
}
#endif

/** @brief Character move with command-move-set. */
void char_move_cmms(State* wk) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_move_jump_addr.kind_of_char, wk->cmd_move_jump_addr.ix, wk->cmd_move_jump_addr.pat, 0);
}

/** @brief Extended command-move-set with additional state check. */
void char_move_cmms2(State* wk) {
    s16 i;
    s16 now_cgd;

    setup_comm_back(wk);
    now_cgd = wk->char_graphic_data_type;
    wk->current_char_type = wk->cmd_move_jump_addr.kind_of_char;
    wk->char_index = wk->cmd_move_jump_addr.ix;

#if CPS3
    wk->set_char_ad = (u32*)wk->char_table[wk->current_char_type][wk->char_index];

    const u32* src = wk->set_char_ad;
    u32* dst = (u32*)&wk->cg_ctr;

    dst[-1] = src[-1];
    dst[-2] = src[-2];
#else
    wk->set_char_ad = &wk->char_table[wk->current_char_type][wk->char_table[wk->current_char_type][wk->char_index] / 4];
    setupCharTableData(wk, 0, 1);
#endif

    if (now_cgd > wk->char_graphic_data_type) {
        // Clear trailing words that are no longer used by the new smaller character state
        // cg_wca_ix corresponds to the start of raw[6] in the body.
        u32* clear_ptr = &wk->char_state.body.raw[6];

        for (i = 0; i < (now_cgd - wk->char_graphic_data_type); i++) {
            *--clear_ptr = 0;
        }
    }

    wk->graphic_index = (wk->cmd_move_jump_addr.pat - 1) * wk->char_graphic_data_type - wk->char_graphic_data_type;
    wk->cg_ctr = 1;
    wk->cg_next_ix = 0;
    wk->old_cgnum = 0;
    wk->cg_wca_ix = 0;

#if !CPS3
    wk->move_type = wk->attack_type;
#endif
}

/** @brief Command-move-set variant 3 with multi-hit tracking. */
s32 char_move_cmms3(PLW* wk) {
    CommandState* cpc;
    s16 i;
    s16 now_cgd;

    wk->link_jump_flag = 1;
    setup_comm_retmj(&wk->wu);
    setup_comm_back(&wk->wu);
    now_cgd = wk->wu.char_graphic_data_type;
    wk->wu.current_char_type = wk->wu.cmd_move_jump_addr.kind_of_char;
    wk->wu.char_index = wk->wu.cmd_move_jump_addr.ix;

#if CPS3
    wk->wu.set_char_ad = (u32*)wk->wu.char_table[wk->wu.current_char_type][wk->wu.char_index];

    const u32* src = wk->wu.set_char_ad;
    u32* dst = (u32*)&wk->wu.cg_ctr;

    dst[-1] = src[-1];
    dst[-2] = src[-2];
#else
    wk->wu.set_char_ad = &wk->wu.char_table[wk->wu.current_char_type][wk->wu.char_table[wk->wu.current_char_type][wk->wu.char_index] / 4];
    setupCharTableData(&wk->wu, 0, 1);
#endif

    wk->wu.graphic_index = wk->wu.cmd_move_jump_addr.pat * wk->wu.char_graphic_data_type - wk->wu.char_graphic_data_type;

#if !CPS3
    wk->wu.move_type = wk->wu.attack_type;
#endif

    while (1) {
        cpc = (CommandState*)(wk->wu.set_char_ad + wk->wu.graphic_index);

        if (cpc->code >= 0x100) {
            break;
        }

        if (decode_chcmd[cpc->code](wk, cpc) != 0) {
            wk->wu.graphic_index += wk->wu.char_graphic_data_type;
        } else if (wk->link_jump_flag != 0) {
            break;
        } else {
            return 0;
        }
    }

    if (now_cgd > wk->wu.char_graphic_data_type) {
        u32* clear_ptr = &wk->wu.char_state.body.raw[6];

        for (i = 0; i < now_cgd - wk->wu.char_graphic_data_type; i++) {
            *--clear_ptr = 0;
        }
    }

    wk->wu.graphic_index -= wk->wu.char_graphic_data_type;
    wk->wu.cg_ctr = 1;
    wk->wu.cg_next_ix = 0;
    wk->wu.old_cgnum = 0;
    wk->wu.cg_wca_ix = 0;
    wk->link_jump_flag = 0;
    return 1;
}

/** @brief Character move with command-hit-set processing. */
void char_move_cmd_hit_stop(PLW* wk) {
    if (wk->high_jump_ok != 0) {
        setup_comm_back(&wk->wu);
        wk->high_jump_ok = 0;
        set_char_move_init2(&wk->wu, wk->wu.cmd_hit_stop_backup.kind_of_char, wk->wu.cmd_hit_stop_backup.ix, wk->wu.cmd_hit_stop_backup.pat, 0);
    }
}

/** @brief Main character animation tick — processes script commands. */
void char_move(State* wk) {
    wk->phase_k5_exec_ok = 1;

    if (--wk->cg_ctr == 0) {
        check_cm_extended_code(wk);
    }
}

/** @brief Processes extended script commands at end-of-frame. */
void check_cm_extended_code(State* wk) {
    CommandState* cpc;

    if (wk->cg_next_ix) {
        wk->graphic_index = (wk->cg_next_ix - 1) * wk->char_graphic_data_type;
    } else {
        wk->graphic_index += wk->char_graphic_data_type;
    }

    while (1) {
        cpc = (CommandState*)(wk->set_char_ad + wk->graphic_index);

        if (cpc->code >= 0x100) {
            check_cgd_patdat(wk);
            break;
        }

        if (decode_chcmd[cpc->code](wk, cpc) == 0) {
            break;
        }

        wk->graphic_index += wk->char_graphic_data_type;
    }
}

/** @brief Script command: dummy — no-op. */
static s32 comm_dummy(State* /* unused */, CommandState* /* unused */) {
    return 1;
}

/** @brief Script command: ROA — read-once-and-advance. */
static s32 comm_roa(State* wk, CommandState* /* unused */) {
    if (wk->cmd_roa_state.pat == 0) {
        wk->cmd_roa_state.kind_of_char = wk->current_char_type;
        wk->cmd_roa_state.ix = wk->char_index;
        wk->cmd_roa_state.pat = 1;
    }

    set_char_move_init2(wk, wk->cmd_roa_state.kind_of_char, wk->cmd_roa_state.ix, wk->cmd_roa_state.pat, 0);
    return 0;
}

/** @brief Script command: END — sets end-of-script type. */
static s32 comm_end(State* wk, CommandState* ctc) {
    wk->graphic_index = (ctc->pat - 2) * wk->char_graphic_data_type;
    return 1;
}

/** @brief Script command: JMP — unconditional jump to label. */
static s32 comm_jmp(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 0);
    return 0;
}

/** @brief Script command: JPSS — jump to sub-script. */
static s32 comm_jpss(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
    return 0;
}

/** @brief Script command: JSR — jump-to-subroutine (push return). */
static s32 comm_jsr(State* wk, CommandState* ctc) {
    wk->cmd_subroutine_return.kind_of_char = wk->current_char_type;
    wk->cmd_subroutine_return.ix = wk->char_index;
    wk->cmd_subroutine_return.pat = (wk->graphic_index / wk->char_graphic_data_type) + 2;
    set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 0);
    return 0;
}

/** @brief Script command: RET — return from subroutine. */
static s32 comm_ret(State* wk, CommandState* /* unused */) {
    set_char_move_init2(wk, wk->cmd_subroutine_return.kind_of_char, wk->cmd_subroutine_return.ix, wk->cmd_subroutine_return.pat, 0);
    return 0;
}

/** @brief Script command: SPS — set position/speed. */
static s32 comm_sps(State* wk, CommandState* ctc) {
    wk->pat_status = ctc->pat;
    return 1;
}

/** @brief Script command: SETR — set register value. */
static s32 comm_setr(State* wk, CommandState* ctc) {
    wk->routine_no[ctc->kind_of_char] = ctc->ix;
    return 1;
}

/** @brief Script command: ADDR — add to register value. */
static s32 comm_addr(State* wk, CommandState* ctc) {
    wk->routine_no[ctc->kind_of_char] += ctc->ix;
    return 1;
}

/** @brief Script command: IF_L — conditional jump on lever input. */
static s32 comm_if_l(State* wk, CommandState* ctc) {
    u16 lvdat;
    u16 my_lvdat;

    if (ctc->kind_of_char & 0x4000) {
        my_lvdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_lvdat = ctc->kind_of_char;
    }

    lvdat = get_comm_if_lever(wk);

    if (!(my_lvdat & 0x7FFF)) {
        if (lvdat == 0) {
            return decord_if_jump(wk, ctc, ctc->ix);
        } else {
            return decord_if_jump(wk, ctc, ctc->pat);
        }
    } else if (my_lvdat & 0x8000) {
        if (lvdat == (my_lvdat & 0xF)) {
            return decord_if_jump(wk, ctc, ctc->ix);
        } else {
            return decord_if_jump(wk, ctc, ctc->pat);
        }
    } else {
        if (lvdat & my_lvdat) {
            return decord_if_jump(wk, ctc, ctc->ix);
        } else {
            return decord_if_jump(wk, ctc, ctc->pat);
        }
    }
}

/** @brief Script command: DJMP — direction-conditional jump. */
static s32 comm_djmp(State* wk, CommandState* ctc) {
    u8 ldir;

    if ((ldir = get_comm_djmp_lever_dir((PLW*)wk))) {
        if (ldir == 1) {
            return decord_if_jump(wk, ctc, ctc->ix);
        } else {
            return decord_if_jump(wk, ctc, ctc->pat);
        }
    } else {
        return decord_if_jump(wk, ctc, ctc->kind_of_char);
    }
}

/** @brief Script command: FOR — begin counted loop. */
static s32 comm_for(State* wk, CommandState* ctc) {
    if (ctc->pat & 0x4000) {
        wk->cmd_loop_counter_1.code = wk->script_register_bank[ctc->pat & 0xF];
    } else {
        wk->cmd_loop_counter_1.code = ctc->pat;
    }

    wk->cmd_loop_counter_1.kind_of_char = wk->current_char_type;
    wk->cmd_loop_counter_1.ix = wk->char_index;
    wk->cmd_loop_counter_1.pat = wk->graphic_index / wk->char_graphic_data_type + 2;
    return 1;
}

/** @brief Script command: NEX — next iteration of counted loop. */
static s32 comm_nex(State* wk, CommandState* ctc) {
    if (wk->cmd_loop_counter_1.code && --wk->cmd_loop_counter_1.code > 0) {
        set_char_move_init2(wk, wk->cmd_loop_counter_1.kind_of_char, wk->cmd_loop_counter_1.ix, wk->cmd_loop_counter_1.pat, 1);
        return 0;
    } else {
        return 1;
    }
}

/** @brief Script command: FOR2 — begin counted loop (variant 2). */
static s32 comm_for2(State* wk, CommandState* ctc) {
    if (ctc->pat & 0x4000) {
        wk->cmd_loop_counter_2.code = wk->script_register_bank[ctc->pat & 0xF];
    } else {
        wk->cmd_loop_counter_2.code = ctc->pat;
    }

    wk->cmd_loop_counter_2.kind_of_char = wk->current_char_type;
    wk->cmd_loop_counter_2.ix = wk->char_index;
    wk->cmd_loop_counter_2.pat = (wk->graphic_index / wk->char_graphic_data_type) + 2;
    return 1;
}

/** @brief Script command: NEX2 — next iteration (variant 2). */
static s32 comm_nex2(State* wk, CommandState* ctc) {
    if (wk->cmd_loop_counter_2.code && --wk->cmd_loop_counter_2.code > 0) {
        set_char_move_init2(wk, wk->cmd_loop_counter_2.kind_of_char, wk->cmd_loop_counter_2.ix, wk->cmd_loop_counter_2.pat, 1);
        return 0;
    } else {
        return 1;
    }
}

/** @brief Script command: RJA — relative jump if above. */
static s32 comm_rja(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_1.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_1.ix = ctc->ix;
    wk->cmd_jump_addr_1.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA — unconditional jump after. */
static s32 comm_uja(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_1.kind_of_char, wk->cmd_jump_addr_1.ix, wk->cmd_jump_addr_1.pat, 0);
    return 0;
}

/** @brief Script command: RJA2. */
static s32 comm_rja2(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_2.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_2.ix = ctc->ix;
    wk->cmd_jump_addr_2.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA2. */
static s32 comm_uja2(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_2.kind_of_char, wk->cmd_jump_addr_2.ix, wk->cmd_jump_addr_2.pat, 0);
    return 0;
}

/** @brief Script command: RJA3. */
static s32 comm_rja3(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_3.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_3.ix = ctc->ix;
    wk->cmd_jump_addr_3.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA3. */
static s32 comm_uja3(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_3.kind_of_char, wk->cmd_jump_addr_3.ix, wk->cmd_jump_addr_3.pat, 0);
    return 0;
}

/** @brief Script command: RJA4. */
static s32 comm_rja4(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_4.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_4.ix = ctc->ix;
    wk->cmd_jump_addr_4.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA4. */
static s32 comm_uja4(State* wk, CommandState* /* unused */) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_4.kind_of_char, wk->cmd_jump_addr_4.ix, wk->cmd_jump_addr_4.pat, 0);
    return 0;
}

/** @brief Script command: RJA5. */
static s32 comm_rja5(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_5.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_5.ix = ctc->ix;
    wk->cmd_jump_addr_5.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA5. */
static s32 comm_uja5(State* wk, CommandState* /* unused */) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_5.kind_of_char, wk->cmd_jump_addr_5.ix, wk->cmd_jump_addr_5.pat, 0);
    return 0;
}

/** @brief Script command: RJA6. */
static s32 comm_rja6(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_6.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_6.ix = ctc->ix;
    wk->cmd_jump_addr_6.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA6. */
static s32 comm_uja6(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_6.kind_of_char, wk->cmd_jump_addr_6.ix, wk->cmd_jump_addr_6.pat, 0);
    return 0;
}

/** @brief Script command: RJA7. */
static s32 comm_rja7(State* wk, CommandState* ctc) {
    wk->cmd_jump_addr_7.kind_of_char = ctc->kind_of_char;
    wk->cmd_jump_addr_7.ix = ctc->ix;
    wk->cmd_jump_addr_7.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UJA7. */
static s32 comm_uja7(State* wk, CommandState* ctc) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_jump_addr_7.kind_of_char, wk->cmd_jump_addr_7.ix, wk->cmd_jump_addr_7.pat, 0);
    return 0;
}

/** @brief Script command: RMJA — relative move-jump A. */
static s32 comm_rmja(State* wk, CommandState* ctc) {
    wk->cmd_move_jump_addr.kind_of_char = ctc->kind_of_char;
    wk->cmd_move_jump_addr.ix = ctc->ix;
    wk->cmd_move_jump_addr.pat = ctc->pat;
    return 1;
}

/** @brief Script command: UMJA — unconditional move-jump A. */
static s32 comm_umja(State* wk, CommandState* /* unused */) {
    setup_comm_back(wk);
    set_char_move_init2(wk, wk->cmd_move_jump_addr.kind_of_char, wk->cmd_move_jump_addr.ix, wk->cmd_move_jump_addr.pat, 0);
    return 0;
}

/** @brief Script command: MDAT — set movement data. */
static s32 comm_mdat(State* wk, CommandState* ctc) {
    wk->cmd_move_data.kind_of_char = ctc->kind_of_char;
    wk->cmd_move_data.ix = ctc->ix;
    wk->cmd_move_data.pat = ctc->pat;
    return 1;
}

/** @brief Script command: YDAT — set Y-axis data. */
static s32 comm_ydat(State* wk, CommandState* ctc) {
    wk->cmd_y_axis_data.kind_of_char = ctc->kind_of_char;
    wk->cmd_y_axis_data.ix = ctc->ix;
    wk->cmd_y_axis_data.pat = ctc->pat;
    return 1;
}

/** @brief Script command: MPOS — set movement position. */
static s32 comm_mpos(State* wk, CommandState* ctc) { // TODO: Confirm CPS3 match
    wk->att.hit_mark = ctc->kind_of_char;
    wk->hit_mark_x = ctc->ix;
    wk->hit_mark_y = ctc->pat;
    return 1;
}

/** @brief Script command: CAFR — set catch frame. */
static s32 comm_cafr(State* wk, CommandState* ctc) {
    wk->cmd_catch_frame.kind_of_char = ctc->kind_of_char;
    wk->cmd_catch_frame.ix = ctc->ix;
    wk->cmd_catch_frame.pat = ctc->pat;
    return 1;
}

/** @brief Script command: CARE — set catch release. */
static s32 comm_care(State* wk, CommandState* ctc) {
    wk->cmd_catch_release.kind_of_char = ctc->kind_of_char;
    wk->cmd_catch_release.ix = ctc->ix;
    wk->cmd_catch_release.pat = ctc->pat;
    return 1;
}

/** @brief Script command: PSXY — set position XY (absolute). */
static s32 comm_psxy(State* wk, CommandState* ctc) {
    State* emwk;

    switch (ctc->kind_of_char) {
    case 0:
        wk->xyz[0].disp.pos = ctc->ix;
        wk->xyz[1].disp.pos = ctc->pat;
        break;

    case 2:
        wk->xyz[0].disp.pos = ctc->ix;
        wk->xyz[1].disp.pos = ctc->pat;
        /* fallthrough */

    default:
        emwk = (State*)wk->target_adrs;
        emwk->xyz[0].disp.pos = ctc->ix;
        emwk->xyz[1].disp.pos = ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: PS_X — set position X. */
static s32 comm_ps_x(State* wk, CommandState* ctc) {
    State* emwk;

    switch (ctc->kind_of_char) {
    case 0:
        wk->xyz[0].disp.pos = ctc->ix;
        break;

    case 2:
        wk->xyz[0].disp.pos = ctc->ix;
        /* fallthrough */

    default:
        emwk = (State*)wk->target_adrs;
        emwk->xyz[0].disp.pos = ctc->ix;
        break;
    }

    return 1;
}

/** @brief Script command: PS_Y — set position Y. */
static s32 comm_ps_y(State* wk, CommandState* ctc) {
    State* emwk;

    if (wk->work_id == 1) {
        switch (ctc->kind_of_char) {
        case 0:
            // CPS3 compares to 21 here
            if (g_state.bg_w.stage == 20 && ((PLW*)wk)->bs2_on_car && ctc->pat < g_state.bs2_floor[2]) {
                wk->xyz[1].disp.pos = g_state.bs2_floor[2];
            } else {
                wk->xyz[1].disp.pos = ctc->pat;
            }

            break;

        case 2:
            wk->xyz[1].disp.pos = ctc->pat;
            /* fallthrough */

        default:
            emwk = (State*)wk->target_adrs;
            emwk->xyz[1].disp.pos = ctc->pat;
            break;
        }

        return 1;
    } else {
        switch (ctc->kind_of_char) {
        case 0:
            wk->xyz[1].disp.pos = ctc->pat;
            break;

        case 2:
            wk->xyz[1].disp.pos = ctc->pat;
            /* fallthrough */

        default:
            emwk = (State*)wk->target_adrs;
            emwk->xyz[1].disp.pos = ctc->pat;
            break;
        }

        return 1;
    }
}

/** @brief Script command: PAXY — add to position XY. */
static s32 comm_paxy(State* wk, CommandState* ctc) {
    State* emwk;

    switch (ctc->kind_of_char) {
    case 0:
        if (wk->rl_flag) {
            wk->xyz[0].cal += ctc->ix << 8;
        } else {
            wk->xyz[0].cal -= ctc->ix << 8;
        }

        wk->xyz[1].cal += ctc->pat << 8;
        break;

    case 2:
        if (wk->rl_flag) {
            wk->xyz[0].cal += ctc->ix << 8;
        } else {
            wk->xyz[0].cal -= ctc->ix << 8;
        }

        wk->xyz[1].cal += ctc->pat << 8;
        /* fallthrough */

    default:
        emwk = (State*)wk->target_adrs;

        if (emwk->rl_flag) {
            emwk->xyz[0].cal += ctc->ix << 8;
        } else {
            emwk->xyz[0].cal -= ctc->ix << 8;
        }

        emwk->xyz[1].cal += ctc->pat << 8;
        break;
    }

    return 1;
}

/** @brief Script command: PA_X — add to position X. */
static s32 comm_pa_x(State* wk, CommandState* ctc) {
    State* emwk;

    switch (ctc->kind_of_char) {
    case 0:
        if (wk->rl_flag) {
            wk->xyz[0].cal += ctc->ix << 8;
        } else {
            wk->xyz[0].cal -= ctc->ix << 8;
        }

        break;

    case 2:
        if (wk->rl_flag) {
            wk->xyz[0].cal += ctc->ix << 8;
        } else {
            wk->xyz[0].cal -= ctc->ix << 8;
        }

        /* fallthrough */

    default:
        emwk = (State*)wk->target_adrs;

        if (emwk->rl_flag) {
            emwk->xyz[0].cal += ctc->ix << 8;
        } else {
            emwk->xyz[0].cal -= ctc->ix << 8;
        }

        break;
    }

    return 1;
}

/** @brief Script command: PA_Y — add to position Y. */
static s32 comm_pa_y(State* wk, CommandState* ctc) {
    State* emwk;

    switch (ctc->kind_of_char) {
    case 0:
        wk->xyz[1].cal += ctc->pat << 8;
        break;

    case 2:
        wk->xyz[1].cal += ctc->pat << 8;
        /* fallthrough */

    default:
        emwk = (State*)wk->target_adrs;
        emwk->xyz[1].cal += ctc->pat << 8;
        break;
    }

    return 1;
}

/** @brief Script command: EXEC — execute external function call. */
static s32 comm_exec(State* wk, CommandState* ctc) {
    effinitjptbl[ctc->kind_of_char](wk, (u8)ctc->ix);
    return 1;
}

/** @brief Script command: RNGC — random number conditional. */
static s32 comm_rngc(State* wk, CommandState* ctc) {
    s16 rngdat;

    if (wk->work_id == 1) {
        rngdat = get_em_body_range(wk);
    } else {
        rngdat = get_em_body_range((State*)((State_Other*)wk)->my_master);
    }

    if (rngdat > ctc->kind_of_char) {
        return decord_if_jump(wk, ctc, ctc->pat);
    } else {
        return decord_if_jump(wk, ctc, ctc->ix);
    }
}

/** @brief Script command: MXYT — movement XY table load. */
static s32 comm_mxyt(State* wk, CommandState* ctc) {
    if (ctc->kind_of_char) {
        setup_mvxy_data(wk, ctc->kind_of_char);
    } else {
        reset_mvxy_data(wk);
    }

    return 1;
}

/** @brief Script command: PJMP — pattern-based jump. */
static s32 comm_pjmp(State* wk, CommandState* ctc) {
    if (random_32() < ctc->kind_of_char) {
        return decord_if_jump(wk, ctc, ctc->ix);
    } else {
        return decord_if_jump(wk, ctc, ctc->pat);
    }
}

/** @brief Script command: HJMP — hit-stop jump with save. */
static s32 comm_hjmp(State* wk, CommandState* ctc) {
    if (wk->frame_link_hit_flag != 0 && wk->hf.hit_flag != 0) {
        if (wk->hf.hit_flag & 0x303) {
            return decord_if_jump(wk, ctc, ctc->kind_of_char);
        }

        if (wk->hf.hit_flag & 0x3030) {
            return decord_if_jump(wk, ctc, ctc->ix);
        }

        if (wk->hf.hit_flag & 0xC0C0) {
            return decord_if_jump(wk, ctc, ctc->pat);
        }
    }

    return 1;
}

/** @brief Script command: HCLR — clear hit-stop jump data. */
static s32 comm_hclr(State* wk, CommandState* /* unused */) {
    wk->hf.hit_flag = 0;
    return 1;
}

/** @brief Script command: IXFW — index forward jump. */
static s32 comm_ixfw(State* wk, CommandState* ctc) {
    if (g_state.test_flag == 0 || g_state.ixbfw_cut == 0) {
        wk->graphic_index += (ctc->pat - 1) * wk->char_graphic_data_type;
    }

    return 1;
}

/** @brief Script command: IXBW — index backward jump. */
static s32 comm_ixbw(State* wk, CommandState* ctc) {
    if ((g_state.test_flag == 0) || (g_state.ixbfw_cut == 0)) {
        wk->graphic_index -= (ctc->pat + 1) * wk->char_graphic_data_type;
    }

    return 1;
}

/** @brief Script command: QUAX — set quake X. */
static s32 comm_quax(State* /* unused */, CommandState* ctc) {
    g_state.bg_w.quake_x_index = ctc->kind_of_char;
    return 1;
}

/** @brief Script command: QUAY — set quake Y. */
static s32 comm_quay(State* /* unused */, CommandState* ctc) {
    g_state.bg_w.quake_y_index = ctc->kind_of_char;
    pp_screen_quake(g_state.bg_w.quake_y_index);
    return 1;
}

/** @brief Script command: IF_S — conditional jump on shot input. */
static s32 comm_if_s(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_shot(wk);

    if (wk->work_id == 1 && ((PLW*)wk)->player_number == 16 && ((PLW*)wk)->spmv_ng_flag & DIP_TAUNT_AFTER_KO_DISABLED &&
        my_shdat == 0x440 && g_state.pcon_dp_flag) {
        shdat = 0;
    }

    if (my_shdat == shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: RAPP — read attack parameter (punch). */
static s32 comm_rapp(State* wk, CommandState* ctc) {
    if (wk->work_id == 1) {
        if (g_state.wcp[wk->id].move_state_flags[9]) {
            setup_comm_back(wk);
            set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
            return 0;
        }

        return 1;
    }

    if (g_state.wcp[((State_Other*)wk)->master_id & 1].move_state_flags[9]) {
        setup_comm_back(wk);
        set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
        return 0;
    }

    return 1;
}

/** @brief Script command: RAPK — read attack parameter (kick). */
static s32 comm_rapk(State* wk, CommandState* ctc) {
    if (wk->work_id == 1) {
        if (g_state.wcp[wk->id].move_state_flags[11]) {
            setup_comm_back(wk);
            set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
            return 0;
        }

        return 1;
    }

    if (g_state.wcp[((State_Other*)wk)->master_id & 1].move_state_flags[11]) {
        setup_comm_back(wk);
        set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
        return 0;
    }

    return 1;
}

/** @brief Script command: GETS — get shot button state. */
static s32 comm_gets(State* wk, CommandState* /* unused */) {
    setupCharTableData(wk, 0, 1);
    return 1;
}

/** @brief Script command: S123 — set shot data 1/2/3. */
static s32 comm_s123(State* wk, CommandState* ctc) {
    wk->routine_no[1] = ctc->kind_of_char;
    wk->routine_no[2] = ctc->ix;
    wk->routine_no[3] = ctc->pat;
    return 1;
}

/** @brief Script command: S456 — set shot data 4/5/6. */
static s32 comm_s456(State* wk, CommandState* ctc) {
    wk->routine_no[4] = ctc->kind_of_char;
    wk->routine_no[5] = ctc->ix;
    wk->routine_no[6] = ctc->pat;
    return 1;
}

/** @brief Script command: A123 — add attack data 1/2/3. */
static s32 comm_a123(State* wk, CommandState* ctc) {
    wk->routine_no[4] += ctc->kind_of_char;
    wk->routine_no[5] += ctc->ix;
    wk->routine_no[6] += ctc->pat;
    return 1;
}

/** @brief Script command: A456 — add attack data 4/5/6. */
static s32 comm_a456(State* wk, CommandState* ctc) {
    wk->routine_no[4] += ctc->kind_of_char;
    wk->routine_no[5] += ctc->ix;
    wk->routine_no[6] += ctc->pat;
    return 1;
}

/** @brief Script command: STOP — set hit-stop timer. */
static s32 comm_stop(PLW* wk, CommandState* ctc) {
    PLW* wk2;

    if (g_state.test_flag == 0) {
        wk->wu.damage_hit_stop = 0;
        wk->wu.hit_stop = ctc->kind_of_char;
        wk2 = (PLW*)wk->wu.target_adrs;
        wk2->wu.hit_stop = ctc->ix;
        wk2->sa_stop_sai = ctc->ix - 4;

        if (wk2->sa_stop_sai < 0) {
            wk2->sa_stop_sai = 1;
        }

        setup_shell_hit_stop(&wk->wu, ctc->ix, ctc->pat);
        setup_shell_hit_stop(&wk2->wu, ctc->ix, 0);
        wk->sa_stop_flag = 0;
        wk2->sa_stop_flag = 2;
        wk2->just_sa_stop_timer = g_state.Game_timer;
    }

    return 1;
}

/** @brief Script command: SMHF — set movement half-speed flag. */
static s32 comm_smhf(State* wk, CommandState* ctc) {
    wk->frame_link_hit_flag = ctc->kind_of_char;
    return 1;
}

/** @brief Script command: NGME — negate me (set negative attributes). */
static s32 comm_ngme(State* wk, CommandState* /* unused */) {
    State* emwk;

    emwk = (State*)wk->hit_adrs;
    emwk->routine_no[1] = 3;
    emwk->routine_no[2] = 1;
    emwk->routine_no[3] = 1;

    if (g_state.test_flag) {
        wk->cmd_y_axis_data.pat = 1;
    }

    return 1;
}

/** @brief Script command: NGEM — negate enemy (set negative attributes). */
static s32 comm_ngem(State* wk, CommandState* /* unused */) {
    State* emwk;

    emwk = (State*)wk->hit_adrs;
    emwk->routine_no[1] = 3;
    emwk->routine_no[2] = 2;
    emwk->routine_no[3] = 1;

    if (g_state.test_flag) {
        wk->cmd_y_axis_data.pat = 2;
    }

    return 1;
}

/** @brief Script command: IFLB — conditional on lever+button. */
static s32 comm_iflb(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_lvsh(wk);

    if (my_shdat == shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: ASXY — add speed XY. */
static s32 comm_asxy(State* wk, CommandState* ctc) {
    s16* from_rom2 = &wk->step_xy_table[ctc->kind_of_char];
    s32 st = *from_rom2++;

    st <<= 8;

    if (wk->rl_flag) {
        wk->xyz[0].cal += st;
    } else {
        wk->xyz[0].cal -= st;
    }

    st = *from_rom2;
    st <<= 8;
    wk->xyz[1].cal += st;
    return 1;
}

/** @brief Script command: SCHX — scroll check X. */
static s32 comm_schx(State* wk, CommandState* ctc) {
    switch (ctc->kind_of_char) {
    case 0:
        wk->mvxy.a[0].sp = (wk->mvxy.a[0].sp * ctc->ix) / ctc->pat;
        break;

    case 2:
        wk->mvxy.a[0].sp = (wk->mvxy.a[0].sp * ctc->ix) / ctc->pat;
        /* fallthrough */

    case 1:
        wk->mvxy.d[0].sp = (wk->mvxy.d[0].sp * ctc->ix) / ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: SCHY — scroll check Y. */
static s32 comm_schy(State* wk, CommandState* ctc) {
    switch (ctc->kind_of_char) {
    case 0:
        wk->mvxy.a[1].sp = (wk->mvxy.a[1].sp * ctc->ix) / ctc->pat;
        break;

    case 2:
        wk->mvxy.a[1].sp = (wk->mvxy.a[1].sp * ctc->ix) / ctc->pat;
        /* fallthrough */

    case 1:
        wk->mvxy.d[1].sp = (wk->mvxy.d[1].sp * ctc->ix) / ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: BACK — store backup state. */
static s32 comm_back(State* wk, CommandState* /* unused */) {
    set_char_move_init2(wk, wk->cmd_state_backup_1.kind_of_char, wk->cmd_state_backup_1.ix, wk->cmd_state_backup_1.pat, 0);
    return 0;
}

/** @brief Script command: MVIX — move index set. */
static s32 comm_mvix(State* wk, CommandState* ctc) {
    wk->mvxy.index = ctc->kind_of_char;
    return 1;
}

/** @brief Script command: SAJP — super-art jump. */
static s32 comm_sajp(State* wk, CommandState* ctc) {
    PLW* pwk;

    if (wk->work_id == 1) {
        if (g_state.My_char[wk->id] != 18 && ((PLW*)wk)->sa->kind_of_arts == ctc->kind_of_char && ((PLW*)wk)->sa->ok == -1) {
            return decord_if_jump(wk, ctc, ctc->ix);
        }
    } else {
        pwk = (PLW*)((State_Other*)wk)->my_master;

        if (pwk->wu.work_id == 1 && pwk->sa->kind_of_arts == ctc->kind_of_char && pwk->sa->ok == -1) {
            return decord_if_jump(&pwk->wu, ctc, ctc->ix);
        }
    }

    return 1;
}

/** @brief Script command: CCCH — cancel chain check. */
static s32 comm_ccch(State* wk, CommandState* ctc) {
    if (ctc->kind_of_char) {
        wk->extra_col += ctc->ix;
        wk->extra_col &= 0x2FFF;
    } else {
        wk->extra_col = ctc->ix;
    }

    return 1;
}

/** @brief Script command: WSET — set work register. */
static s32 comm_wset(State* wk, CommandState* ctc) {
    switch (ctc->ix) {
    default:
        wk->script_register_bank[ctc->kind_of_char & 0xF] = ctc->pat;
        break;

    case 1:
        wk->script_register_bank[ctc->kind_of_char & 0xF] &= ctc->pat;
        break;

    case 2:
        wk->script_register_bank[ctc->kind_of_char & 0xF] |= ctc->pat;
        break;

    case 3:
        wk->script_register_bank[ctc->kind_of_char & 0xF] += ctc->pat;
        break;

    case 4:
        wk->script_register_bank[ctc->kind_of_char & 0xF] -= ctc->pat;
        break;

    case 5:
        wk->script_register_bank[ctc->kind_of_char & 0xF] *= ctc->pat;
        break;

    case 6:
        wk->script_register_bank[ctc->kind_of_char & 0xF] /= ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: WSWK — set work register from another work. */
static s32 comm_wswk(State* wk, CommandState* ctc) {
    switch (ctc->ix) {
    default:
        wk->script_register_bank[ctc->kind_of_char & 0xF] = wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 1:
        wk->script_register_bank[ctc->kind_of_char & 0xF] &= wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 2:
        wk->script_register_bank[ctc->kind_of_char & 0xF] |= wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 3:
        wk->script_register_bank[ctc->kind_of_char & 0xF] += wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 4:
        wk->script_register_bank[ctc->kind_of_char & 0xF] -= wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 5:
        wk->script_register_bank[ctc->kind_of_char & 0xF] *= wk->script_register_bank[ctc->pat & 0xF];
        break;

    case 6:
        wk->script_register_bank[ctc->kind_of_char & 0xF] /= wk->script_register_bank[ctc->pat & 0xF];
        break;
    }

    return 1;
}

/** @brief Script command: WADD — add to work register. */
static s32 comm_wadd(State* wk, CommandState* ctc) {
    wk->script_register_bank[ctc->kind_of_char & 0xF] += ctc->ix;
    wk->script_register_bank[ctc->kind_of_char & 0xF] &= ctc->pat;
    return 1;
}

/** @brief Script command: WCEQ — work compare-equal. */
static s32 comm_wceq(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] == ctc->ix) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WCNE — work compare-not-equal. */
static s32 comm_wcne(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] != ctc->ix) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }
    return 1;
}

/** @brief Script command: WCGT — work compare-greater-than. */
static s32 comm_wcgt(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] > ctc->ix) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WCLT — work compare-less-than. */
static s32 comm_wclt(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] < ctc->ix) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WADD2 — add to work register (variant 2). */
static s32 comm_wadd2(State* wk, CommandState* ctc) {
    wk->script_register_bank[ctc->kind_of_char & 0xF] += wk->script_register_bank[ctc->ix & 0xF];
    wk->script_register_bank[ctc->kind_of_char & 0xF] &= ctc->pat;
    return 1;
}

/** @brief Script command: WCEQ2 — work compare-equal (variant 2). */
static s32 comm_wceq2(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] == wk->script_register_bank[ctc->ix & 0xF]) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WCNE2 — compare-not-equal (variant 2). */
static s32 comm_wcne2(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] != wk->script_register_bank[ctc->ix & 0xF]) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WCGT2 — compare-greater-than (variant 2). */
static s32 comm_wcgt2(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] > wk->script_register_bank[ctc->ix & 0xF]) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: WCLT2 — compare-less-than (variant 2). */
static s32 comm_wclt2(State* wk, CommandState* ctc) {
    if (wk->script_register_bank[ctc->kind_of_char & 0xF] < wk->script_register_bank[ctc->ix & 0xF]) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: RAPP2 — read attack param (punch, variant 2). */
static s32 comm_rapp2(State* wk, CommandState* ctc) {
    if (wk->work_id == 1) {
        if (g_state.wcp[wk->id].move_state_flags[8]) {
            setup_comm_back(wk);
            set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
            return 0;
        }

        return 1;
    }

    if (g_state.wcp[((State_Other*)wk)->master_id & 1].move_state_flags[8]) {
        setup_comm_back(wk);
        set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
        return 0;
    }

    return 1;
}

/** @brief Script command: RAPK2 — read attack param (kick, variant 2). */
static s32 comm_rapk2(State* wk, CommandState* ctc) {
    if (wk->work_id == 1) {
        if (g_state.wcp[wk->id].move_state_flags[10]) {
            setup_comm_back(wk);
            set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
            return 0;
        }

        return 1;
    }

    if (g_state.wcp[((State_Other*)wk)->master_id & 1].move_state_flags[10]) {
        setup_comm_back(wk);
        set_char_move_init2(wk, ctc->kind_of_char, ctc->ix, ctc->pat, 1);
        return 0;
    }

    return 1;
}

/** @brief Script command: IFLG — conditional on flag state. */
static s32 comm_iflg(State* wk, CommandState* ctc) {
    if (ctc->kind_of_char == 0) {
        if (wk->script_register_bank[11] < ctc->ix) {
            return 1;
        }

        return decord_if_jump(wk, ctc, ctc->pat);
    }

    if (((State*)wk->target_adrs)->script_register_bank[11] < ctc->ix) {
        return 1;
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: MPCY — my position-copy Y. */
static s32 comm_mpcy(State* wk, CommandState* ctc) {
    s16 ans = 0;

    switch (ctc->ix) {
    case 1:
        if (wk->xyz[1].disp.pos > ctc->kind_of_char) {
            ans = 1;
        }

        break;

    case 2:
        if (wk->xyz[1].disp.pos < ctc->kind_of_char) {
            ans = 1;
        }

        break;

    default:
        if (wk->xyz[1].disp.pos == ctc->kind_of_char) {
            ans = 1;
        }

        break;
    }

    if (ans == 0) {
        return 1;
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: EPCY — enemy position-copy Y. */
static s32 comm_epcy(State* wk, CommandState* ctc) {
    State* emwk = (State*)wk->target_adrs;
    s16 ans = 0;

    switch (ctc->ix) {
    case 1:
        if (emwk->xyz[1].disp.pos > ctc->kind_of_char) {
            ans = 1;
        }

        break;

    case 2:
        if (emwk->xyz[1].disp.pos < ctc->kind_of_char) {
            ans = 1;
        }

        break;

    default:
        if (emwk->xyz[1].disp.pos == ctc->kind_of_char) {
            ans = 1;
        }

        break;
    }

    if (ans == 0) {
        return 1;
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: IMGS — image set (start rendering effect). */
static s32 comm_imgs(PLW* wk, CommandState* ctc) {
    PLW* tk;

    if (g_state.test_flag == 0) {
        tk = (PLW*)wk->wu.target_adrs;

        switch (ctc->kind_of_char) {
        case 0:
            wk->image_setup_flag = 2;
            wk->image_data_index = ctc->ix;
            break;

        default:
            wk->image_setup_flag = 2;
            wk->image_data_index = ctc->ix;
            /* fallthrough */

        case 1:
            tk->image_setup_flag = 2;
            tk->image_data_index = ctc->ix;
            break;
        }
    }

    return 1;
}

/** @brief Script command: IMGC — image clear (stop rendering effect). */
static s32 comm_imgc(PLW* wk, CommandState* ctc) {
    PLW* tk = (PLW*)wk->wu.target_adrs;

    switch (ctc->kind_of_char) {
    case 0:
        wk->image_setup_flag = 0;
        break;

    default:
        wk->image_setup_flag = 0;
        /* fallthrough */

    case 1:
        tk->image_setup_flag = 0;
        break;
    }

    return 1;
}

/** @brief Script command: RVXY — relative velocity XY. */
static s32 comm_rvxy(State* wk, CommandState* ctc) {
    State* emwk = (State*)wk->target_adrs;

    switch (ctc->kind_of_char) {
    case 0:
        if (wk->rl_flag) {
            wk->xyz[0].cal = emwk->xyz[0].cal + (ctc->ix << 8);
        } else {
            wk->xyz[0].cal = emwk->xyz[0].cal - (ctc->ix << 8);
        }

        wk->xyz[1].cal = emwk->xyz[1].cal + (ctc->pat << 8);
        break;

    case 2:
        if (wk->rl_flag) {
            wk->xyz[0].cal = emwk->xyz[0].cal + (ctc->ix << 8);
        } else {
            wk->xyz[0].cal = emwk->xyz[0].cal - (ctc->ix << 8);
        }

        wk->xyz[1].cal = emwk->xyz[1].cal + (ctc->pat << 8);
        /* fallthrough */

    default:
        if (wk->rl_flag) {
            emwk->xyz[0].cal = wk->xyz[0].cal + (ctc->ix << 8);
        } else {
            emwk->xyz[0].cal = wk->xyz[0].cal - (ctc->ix << 8);
        }

        emwk->xyz[1].cal = wk->xyz[1].cal + (ctc->pat << 8);
        break;
    }

    return 1;
}

/** @brief Script command: RV_X — relative velocity X. */
static s32 comm_rv_x(State* wk, CommandState* ctc) {
    State* emwk = (State*)wk->target_adrs;

    switch (ctc->kind_of_char) {
    case 0:
        if (wk->rl_flag) {
            wk->xyz[0].cal = emwk->xyz[0].cal + (ctc->ix << 8);
        } else {
            wk->xyz[0].cal = emwk->xyz[0].cal - (ctc->ix << 8);
        }

        break;

    case 2:
        if (wk->rl_flag) {
            wk->xyz[0].cal = emwk->xyz[0].cal + (ctc->ix << 8);
        } else {
            wk->xyz[0].cal = emwk->xyz[0].cal - (ctc->ix << 8);
        }

        /* fallthrough */

    default:
        if (wk->rl_flag) {
            emwk->xyz[0].cal = wk->xyz[0].cal + (ctc->ix << 8);
        } else {
            emwk->xyz[0].cal = wk->xyz[0].cal - (ctc->ix << 8);
        }

        break;
    }

    return 1;
}

/** @brief Script command: RV_Y — relative velocity Y. */
static s32 comm_rv_y(State* wk, CommandState* ctc) {
    State* emwk = (State*)wk->target_adrs;

    switch (ctc->kind_of_char) {
    case 0:
        wk->xyz[1].cal = emwk->xyz[1].cal + (ctc->pat << 8);
        break;

    case 2:
        wk->xyz[1].cal = emwk->xyz[1].cal + (ctc->pat << 8);
        /* fallthrough */

    default:
        emwk->xyz[1].cal = wk->xyz[1].cal + (ctc->pat << 8);
        break;
    }

    return 1;
}

/** @brief Script command: CCFL — cancel chain flag. */
static s32 comm_ccfl(PLW* wk, CommandState* /* unused */) {
    wk->caution_flag = 0;
    return 1;
}

/** @brief Script command: MYHP — branch based on my HP level. */
static s32 comm_myhp(State* wk, CommandState* ctc) {
    s16 num = 0;
    s32 cmpvital = (g_state.Max_vitality * ctc->ix) / 100;

    switch (ctc->kind_of_char) {
    case 1:
        if (wk->vital_new > cmpvital) {
            num = 1;
        }

        break;

    case 2:
        if (wk->vital_new < cmpvital) {
            num = 1;
        }

        break;

    default:
        if (wk->vital_new == cmpvital) {
            num = 1;
        }

        break;
    }

    if (num) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: EMHP — branch based on enemy HP level. */
static s32 comm_emhp(State* wk, CommandState* ctc) {
    State* emwk = (State*)wk->target_adrs;
    s16 num = 0;
    s32 cmpvital = (g_state.Max_vitality * ctc->ix) / 100;

    switch (ctc->kind_of_char) {
    case 1:
        if (cmpvital < emwk->vital_new) {
            num = 1;
        }

        break;

    case 2:
        if (emwk->vital_new < cmpvital) {
            num = 1;
        }

        break;

    default:
        if (emwk->vital_new == cmpvital) {
            num = 1;
        }

        break;
    }

    if (num) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return 1;
}

/** @brief Script command: EXBGS — extra BG start (no-op stub). */
static s32 comm_exbgs(State* /* unused */, CommandState* /* unused */) {
    return 1;
}

/** @brief Script command: EXBGC — extra BG clear (no-op stub). */
static s32 comm_exbgc(State* /* unused */, CommandState* /* unused */) {
    return 1;
}

/** @brief Script command: ATMF — set attack metamorphosis flag. */
static s32 comm_atmf(PLW* wk, CommandState* ctc) {
    wk->parry_flag = ctc->kind_of_char;
    wk->parry_point = ctc->ix;
    return 1;
}

/** @brief Script command: CHKWF — check move flag conditional. */
static s32 comm_chkwf(PLW* wk, CommandState* ctc) {
    if (wk->cp->move_state_flags[ctc->kind_of_char] == 0 || wk->cp->move_state_flags[ctc->kind_of_char] == -1) {
        return decord_if_jump(&wk->wu, ctc, ctc->pat);
    }

    move_flag_clear_only_1(wk->wu.id, ctc->kind_of_char);
    return decord_if_jump(&wk->wu, ctc, ctc->ix);
}

/** @brief Script command: RETMJ — return from move-jump. */
static s32 comm_retmj(PLW* wk, CommandState* /* unused */) {
    wk->wu.current_char_type = wk->wu.cmd_state_backup_2.kind_of_char;
    wk->wu.char_index = wk->wu.cmd_state_backup_2.ix;
    wk->wu.graphic_index = wk->wu.cmd_state_backup_2.pat;
    wk->wu.set_char_ad = &wk->wu.char_table[wk->wu.current_char_type][wk->wu.char_table[wk->wu.current_char_type][wk->wu.char_index] / 4];
    setupCharTableData(&wk->wu, 0, 1);
    wk->link_jump_flag = 0;
    return 0;
}

/** @brief Script command: SSTX — step speed table X. */
static s32 comm_sstx(State* wk, CommandState* ctc) {
    SST sstx;

    sstx.patl = 0;
    sstx.pats.h = ctc->pat;
    sstx.patl >>= 8;

    switch (ctc->kind_of_char) {
    case 0:
        switch (ctc->ix) {
        default:
            wk->mvxy.a[0].sp = sstx.patl;
            break;

        case 1:
            wk->mvxy.a[0].sp &= sstx.patl;
            break;

        case 2:
            wk->mvxy.a[0].sp |= sstx.patl;
            break;

        case 3:
            wk->mvxy.a[0].sp += sstx.patl;
            break;

        case 4:
            wk->mvxy.a[0].sp -= sstx.patl;
            break;

        case 5:
            wk->mvxy.a[0].sp *= sstx.patl;
            break;

        case 6:
            wk->mvxy.a[0].sp /= sstx.patl;
            break;
        }

        break;

    case 2:
        switch (ctc->ix) {
        default:
            wk->mvxy.a[0].sp = sstx.patl;
            break;

        case 1:
            wk->mvxy.a[0].sp &= sstx.patl;
            break;

        case 2:
            wk->mvxy.a[0].sp |= sstx.patl;
            break;

        case 3:
            wk->mvxy.a[0].sp += sstx.patl;
            break;

        case 4:
            wk->mvxy.a[0].sp -= sstx.patl;
            break;

        case 5:
            wk->mvxy.a[0].sp *= sstx.patl;
            break;

        case 6:
            wk->mvxy.a[0].sp /= sstx.patl;
            break;
        }

        /* fallthrough */

    case 1:
        switch (ctc->ix) {
        default:
            wk->mvxy.d[0].sp = sstx.patl;
            break;

        case 1:
            wk->mvxy.d[0].sp &= sstx.patl;
            break;

        case 2:
            wk->mvxy.d[0].sp |= sstx.patl;
            break;

        case 3:
            wk->mvxy.d[0].sp += sstx.patl;
            break;

        case 4:
            wk->mvxy.d[0].sp -= sstx.patl;
            break;

        case 5:
            wk->mvxy.d[0].sp *= sstx.patl;
            break;

        case 6:
            wk->mvxy.d[0].sp /= sstx.patl;
            break;
        }

        break;

    default:
        wk->mvxy.physics_curve_type[0] = ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: SSTY — step speed table Y. */
static s32 comm_ssty(State* wk, CommandState* ctc) {
    SST ssty;

    ssty.patl = 0;
    ssty.pats.h = ctc->pat;
    ssty.patl >>= 8;

    switch (ctc->kind_of_char) {
    case 0:
        switch (ctc->ix) {
        default:
            wk->mvxy.a[1].sp = ssty.patl;
            break;

        case 1:
            wk->mvxy.a[1].sp &= ssty.patl;
            break;

        case 2:
            wk->mvxy.a[1].sp |= ssty.patl;
            break;

        case 3:
            wk->mvxy.a[1].sp += ssty.patl;
            break;

        case 4:
            wk->mvxy.a[1].sp -= ssty.patl;
            break;

        case 5:
            wk->mvxy.a[1].sp *= ssty.patl;
            break;

        case 6:
            wk->mvxy.a[1].sp /= ssty.patl;
            break;
        }

        break;

    case 2:
        switch (ctc->ix) {
        default:
            wk->mvxy.a[1].sp = ssty.patl;
            break;

        case 1:
            wk->mvxy.a[1].sp &= ssty.patl;
            break;

        case 2:
            wk->mvxy.a[1].sp |= ssty.patl;
            break;

        case 3:
            wk->mvxy.a[1].sp += ssty.patl;
            break;

        case 4:
            wk->mvxy.a[1].sp -= ssty.patl;
            break;

        case 5:
            wk->mvxy.a[1].sp *= ssty.patl;
            break;

        case 6:
            wk->mvxy.a[1].sp /= ssty.patl;
            break;
        }

        /* fallthrough */

    case 1:
        switch (ctc->ix) {
        default:
            wk->mvxy.d[1].sp = ssty.patl;
            break;

        case 1:
            wk->mvxy.d[1].sp &= ssty.patl;
            break;

        case 2:
            wk->mvxy.d[1].sp |= ssty.patl;
            break;

        case 3:
            wk->mvxy.d[1].sp += ssty.patl;
            break;

        case 4:
            wk->mvxy.d[1].sp -= ssty.patl;
            break;

        case 5:
            wk->mvxy.d[1].sp *= ssty.patl;
            break;

        case 6:
            wk->mvxy.d[1].sp /= ssty.patl;
            break;
        }

        break;

    default:
        wk->mvxy.physics_curve_type[1] = ctc->pat;
        break;
    }

    return 1;
}

/** @brief Script command: NGDA — set negate damage amount. */
static s32 comm_ngda(State* wk, CommandState* ctc) {
    wk->cmd_y_axis_data.kind_of_char = ctc->kind_of_char;
    wk->cmd_y_axis_data.ix = ctc->ix;
    wk->cmd_y_axis_data.pat = ctc->pat;
    return 1;
}

/** @brief Script command: FLIP — flip sprite horizontally. */
static s32 comm_flip(State* wk, CommandState* /* unused */) {
    wk->rl_flag = (wk->rl_flag + 1) & 1;
    return 1;
}

/** @brief Script command: KAGE — set shadow display flag. */
static s32 comm_kage(State* wk, CommandState* ctc) {
    wk->shadow_x = ctc->kind_of_char;
    wk->shadow_y = ctc->ix;
    wk->shadow_char = ctc->pat;
    return 1;
}

/** @brief Script command: DSPF — set display flag. */
static s32 comm_dspf(State* wk, CommandState* ctc) {
    wk->disp_flag = ctc->kind_of_char;
    return 1;
}

/** @brief Script command: IFRLF — conditional on RL flag. */
static s32 comm_ifrlf(State* wk, CommandState* ctc) {
    if (ctc->kind_of_char) {
        if (wk->rl_flag == wk->active_move) {
            return decord_if_jump(wk, ctc, ctc->pat);
        }

        return decord_if_jump(wk, ctc, ctc->ix);
    }

    if (wk->rl_flag == wk->active_move) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: SRLF — set RL flag. */
static s32 comm_srlf(State* wk, CommandState* ctc) {
    if (ctc->kind_of_char) {
        if (wk->rl_flag != wk->active_move) {
            wk->rl_flag = wk->active_move;
        }
    } else if (wk->rl_flag == wk->active_move) {
        wk->rl_flag = (wk->rl_flag + 1) & 1;
    }

    return 1;
}

/** @brief Script command: BGRLF — branch based on background RL flag. */
static s32 comm_bgrlf(State* wk, CommandState* ctc) {
    if (wk->rl_flag) {
        if (wk->position_x > g_state.bg_w.bgw[1].pos_x_work) {
            return decord_if_jump(wk, ctc, ctc->pat);
        }

        return decord_if_jump(wk, ctc, ctc->ix);
    }

    if (wk->position_x < g_state.bg_w.bgw[1].pos_x_work) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return decord_if_jump(wk, ctc, ctc->ix);
}

/** @brief Script command: SCMD — set sub-command. */
static s32 comm_scmd(PLW* wk, CommandState* ctc) {
    wk->cmd_request = ctc->kind_of_char;
    return 1;
}

/** @brief Script command: RLJMP — RL-conditional jump. */
static s32 comm_rljmp(State* wk, CommandState* ctc) {
    if (wk->rl_flag) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return decord_if_jump(wk, ctc, ctc->ix);
}

/** @brief Script command: IFS2 — conditional on shot (variant 2). */
static s32 comm_ifs2(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_shot(wk);

    if (my_shdat & shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: ABBAK — absolute back position restore. */
static s32 comm_abbak(State* wk, CommandState* /* unused */) {
    set_char_move_init2(wk, wk->cmd_state_backup_3.kind_of_char, wk->cmd_state_backup_3.ix, wk->cmd_state_backup_3.pat, 0);
    return 0;
}

/** @brief Script command: SSE — sound SE request. */
static s32 comm_sse(State* wk, CommandState* ctc) {
    u16* seadrs;

    wk->cg_se = ctc->kind_of_char;

    if (wk->cg_se & 0x800) {
        seadrs = (u16*)&wk->se_random_table[wk->se_random_table[wk->cg_se & 0x7FF] / 4];
        wk->cg_se = seadrs[random_16()];
    }

    if (wk->cg_se) {
        Se_Dispatch(wk->cg_se, check_xcopy_filter_se_req(wk), wk);
    }

    return 1;
}

/** @brief Script command: S_CHG — sound change. */
static s32 comm_s_chg(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_shot_now_off(wk);

    if (my_shdat == shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: SCHG2 — sound change (variant 2). */
static s32 comm_schg2(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_shot_now_off(wk);

    if (my_shdat & shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Script command: RHSJA — read hit-stop jump A. */
static s32 comm_rhsja(PLW* wk, CommandState* ctc) {
    wk->wu.cmd_hit_stop_backup.kind_of_char = ctc->kind_of_char;
    wk->wu.cmd_hit_stop_backup.ix = ctc->ix;
    wk->wu.cmd_hit_stop_backup.pat = ctc->pat;
    wk->high_jump_ok = 1;
    return 1;
}

/** @brief Script command: UHSJA — unconditional hit-stop jump A. */
static s32 comm_uhsja(PLW* wk, CommandState* /* unused */) {
    setup_comm_back(&wk->wu);
    wk->high_jump_ok = 0;
    set_char_move_init2(&wk->wu, wk->wu.cmd_hit_stop_backup.kind_of_char, wk->wu.cmd_hit_stop_backup.ix, wk->wu.cmd_hit_stop_backup.pat, 0);
    return 0;
}

/** @brief Script command: IFCOM — conditional on COM flag. */
static s32 comm_ifcom(State* wk, CommandState* ctc) {
    if (wk->pl_operator) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return decord_if_jump(wk, ctc, ctc->ix);
}

/** @brief Script command: AXJMP — X-axis conditional jump. */
static s32 comm_axjmp(State* wk, CommandState* ctc) {
    if (wk->mvxy.a[0].real.h > 2) {
        return decord_if_jump(wk, ctc, ctc->kind_of_char);
    }

    if (wk->mvxy.a[0].real.h < -2) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return decord_if_jump(wk, ctc, ctc->ix);
}

/** @brief Script command: AYJMP — Y-axis conditional jump. */
static s32 comm_ayjmp(State* wk, CommandState* ctc) {
    if (wk->mvxy.a[1].real.h > 0) {
        return decord_if_jump(wk, ctc, ctc->kind_of_char);
    }

    if (wk->mvxy.a[1].real.h < 0) {
        return decord_if_jump(wk, ctc, ctc->pat);
    }

    return decord_if_jump(wk, ctc, ctc->ix);
}

/** @brief Script command: IFS3 — conditional on shot (variant 3). */
static s32 comm_ifs3(State* wk, CommandState* ctc) {
    u16 shdat;
    u16 my_shdat;

    if (ctc->kind_of_char & 0x4000) {
        my_shdat = wk->script_register_bank[ctc->kind_of_char & 0xF];
    } else {
        my_shdat = ctc->kind_of_char;
    }

    shdat = get_comm_if_shot_now(wk);

    if (my_shdat & shdat) {
        return decord_if_jump(wk, ctc, ctc->ix);
    }

    return decord_if_jump(wk, ctc, ctc->pat);
}

/** @brief Decodes an if-jump target for script conditional commands. */
static s16 decord_if_jump(State* wk, CommandState* cpc, s16 ix) {
    s16 rnum;

    switch (ix & 0xE000) {
    case 0x4000:
        wk->graphic_index += ((ix & 0xFF) - 1) * wk->char_graphic_data_type;
        rnum = 1;
        break;

    case 0x8000:
        wk->graphic_index -= ((ix & 0xFF) + 1) * wk->char_graphic_data_type;
        rnum = 1;
        break;

    case 0x2000:
        rnum = decode_if_lever[ix & 0xFF](wk, cpc);
        break;

    default:
        wk->graphic_index = (ix - 2) * wk->char_graphic_data_type;
        rnum = 1;
        break;
    }

    return rnum;
}

/** @brief Returns the lever direction for script if-commands. */
static u16 get_comm_if_lever(State* wk) {
    u16 num;

    if (wk->work_id == 1) {
        num = g_state.wcp[wk->id].input_pressed & 0xF;
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].input_pressed & 0xF;
    }

    return num;
}

/** @brief Returns the shot button state for script if-commands. */
static u16 get_comm_if_shot(State* wk) {
    u16 num;

    if (wk->work_id == 1) {
        num = g_state.wcp[wk->id].input_pressed & 0x770;
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].input_pressed & 0x770;
    }

    return num;
}

/** @brief Returns shot-now-off state for script if-commands. */
static u16 get_comm_if_shot_now_off(State* wk) {
    u16 num;

    if (wk->work_id == 1) {
        num = g_state.wcp[wk->id].input_current & 0x770;
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].input_current & 0x770;
    }

    if (wk->cg_cancel & 0x80) {
        if (wk->work_id == 1) {
            num |= g_state.wcp[wk->id].input_released & 0x770;
        } else {
            num |= g_state.wcp[((State_Other*)wk)->master_id & 1].input_released & 0x770;
        }
    }

    return num;
}

/** @brief Returns the shot-now state for script if-commands. */
static u16 get_comm_if_shot_now(State* wk) {
    u16 num;

    if (wk->work_id == 1) {
        num = g_state.wcp[wk->id].input_current & 0x770;
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].input_current & 0x770;
    }

    return num;
}

/** @brief Returns the lever+shot combined state for script if-commands. */
static u16 get_comm_if_lvsh(State* wk) {
    u16 num;

    if (wk->work_id == 1) {
        num = g_state.wcp[wk->id].input_pressed & 0x77F;
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].input_pressed & 0x77F;
    }

    return num;
}

/** @brief Returns the lever direction for direction-jump commands. */
static u8 get_comm_djmp_lever_dir(PLW* wk) {
    u8 num;

    if (wk->wu.work_id == 1) {
        if (wk->py->flag == 0) {
            num = g_state.wcp[wk->wu.id].lever_dir;
        } else {
            num = 0;
        }
    } else {
        num = g_state.wcp[((State_Other*)wk)->master_id & 1].lever_dir;
    }

    return num;
}

/** @brief Stores the backup state for comm_back. */
void setup_comm_back(State* wk) {
    wk->phase_k5_init_flag = 1;
    wk->cmd_state_backup_1.kind_of_char = wk->current_char_type;
    wk->cmd_state_backup_1.ix = wk->char_index;
    wk->cmd_state_backup_1.pat = (wk->graphic_index / wk->char_graphic_data_type) + 2;
}

/** @brief Stores the return-move-jump state. */
static void setup_comm_retmj(State* wk) {
    wk->cmd_state_backup_2.kind_of_char = wk->current_char_type;
    wk->cmd_state_backup_2.ix = wk->char_index;
    wk->cmd_state_backup_2.pat = wk->graphic_index;
}

/** @brief Stores the absolute-backup state. */
void setup_comm_abbak(State* wk) {
    wk->cmd_state_backup_3.kind_of_char = wk->current_char_type;
    wk->cmd_state_backup_3.ix = wk->char_index;
    wk->cmd_state_backup_3.pat = (wk->graphic_index / wk->char_graphic_data_type) + 2;
}

static int catch_table_offset(Character thrown_character) {
    if (ArcadeBalance_IsEnabled()) {
        // In arcade version Akuma is followed by Shin Akuma. To account for this
        // we have to increment all character numbers after Akuma
        if (thrown_character > CHAR_AKUMA) {
            thrown_character += 1;
        }

        return thrown_character - 24;
    } else {
        return thrown_character - 20;
    }
}

/** @brief Main pattern-data interpreter — processes CG data per frame. */
void check_cgd_patdat(State* wk) {
    ST st;

    u16* seAdrs;
    s16* from_rom2;

    setupCharTableData(wk, 0, 0);

    switch (wk->char_graphic_data_type) {
    case 6:
        if (wk->cg_add_xy) {
            from_rom2 = wk->step_xy_table + wk->cg_add_xy;
            st.l = *from_rom2++;
            st.l <<= 8;

            if (wk->rl_flag) {
                wk->xyz[0].cal += st.l;
            } else {
                wk->xyz[0].cal -= st.l;
            }

            st.l = *from_rom2;
            st.l <<= 8;
            wk->xyz[1].cal += st.l;
        }

        if (wk->cg_status & 0x80) {
            wk->pat_status = wk->cg_status & 0x7F;
        }

        /* fallthrough */

    case 4:
        wk->cg_tc_state = wk->anim_hurtbox_index & 0x1FFF;
        st.w.h = wk->anim_hitbox_index;
        st.w.l = wk->anim_hurtbox_index;
        wk->anim_hitbox_index >>= 6;
        st.l *= 8;
        wk->anim_hurtbox_index = st.w.h & 0x1FF;

        if (wk->anim_hitbox_index) {
            set_new_attnum(wk);
        }

        if (wk->cg_effect) {
            effinitjptbl[wk->cg_effect](wk, wk->cg_eftype);
        }

        break;
    }

    wk->cg_jphos = jphos_table[wk->anim_overlap_col_index & 0xF];
    wk->anim_overlap_col_index >>= 4;
    wk->cg_flip = wk->cg_se & 3;
    wk->cg_prio = (wk->cg_se & 0xF) >> 2;
    wk->cg_se >>= 4;

    if (wk->cg_se & 0x800) {
        seAdrs = (u16*)(wk->se_random_table + (wk->se_random_table[wk->cg_se & 0x7FF] / 4));
        wk->cg_se = seAdrs[random_16()];
    }

    if (wk->cg_se) {
        Se_Dispatch(wk->cg_se, check_xcopy_filter_se_req(wk), wk);
    }

    if (wk->work_id == 1) {
        if (wk->cg_rival == 0) {
            wk->curr_rca = NULL;
        } else {
            wk->curr_rca = wk->rival_catch_tbl + (wk->cg_rival + catch_table_offset(((PLW*)wk)->throw_target_id));
        }

        wk->graphic_overlap_index = wk->olc_ix_table[wk->anim_overlap_col_index];
    }

    if (wk->work_id < 16) {
        wk->cg_ja = wk->hit_ix_table[wk->anim_hurtbox_index];
        Set_Collision_Boxes(wk);
    }

    if ((wk->cg_type != 0xFF) && (wk->cg_type & 0x80)) {
        wk->cg_wca_ix = wk->cg_type & 0x7F;
        wk->cg_type = 0;
    }

    if (wk->work_id == 1) {
        if ((WK_AS_PLW->special_move_disabled_flag2 & DIP2_TARGET_COMBO_DISABLED) && (wk->cg_cancel & 8) && !(wk->move_type & 0xF8)) {
            if (wk->move_type & 6) {
                wk->cg_cancel &= 0xF7;
                wk->cg_tc_state = 0;
            } else if (wk->cg_tc_state & 0x110) {
                wk->cg_tc_state &= 0xF99F;
            } else {
                wk->cg_cancel &= 0xF7;
                wk->cg_tc_state = 0;
            }
        }

        if (WK_AS_PLW->special_move_disabled_flag2 & DIP2_SA_TO_SA_CANCEL_DISABLED) {
            if (wk->move_type & 0x60) {
                wk->cg_cancel &= 0xBF;
            }
        } else if ((wk->move_type & 0x60) && (wk->cg_cancel & 0x40)) {
            wk->frame_link_hit_flag = 1;
        }

        if (!(WK_AS_PLW->special_move_disabled_flag2 & DIP2_SPECIAL_TO_SPECIAL_CANCEL_DISABLED) && !(wk->move_type & 0x60) &&
            (wk->move_type & 0xF8) && (wk->cg_cancel & 0x40)) {
            wk->cg_cancel |= 0x60;
        }

        if (!(wk->move_type & 0xF8) && (wk->routine_no[1] == 4) && (wk->routine_no[2] < 16)) {
            switch (plpat_rno_filter[wk->routine_no[2]]) {
            case 9:
                if (wk->routine_no[3] != 1) {
                    break;
                }

                /* fallthrough */

            case 1:
                if (!(WK_AS_PLW->special_move_disabled_flag2 & DIP2_ALL_MOVES_CANCELLABLE_BY_HIGH_JUMP_DISABLED)) {
                    wk->cg_cancel |= 1;
                }

                if (!(WK_AS_PLW->special_move_disabled_flag2 & DIP2_ALL_MOVES_CANCELLABLE_BY_DASH_DISABLED)) {
                    wk->cg_cancel |= 2;
                }

                if (!(WK_AS_PLW->special_move_disabled_flag2 & DIP2_GROUND_CHAIN_COMBO_DISABLED)) {
                    if (WK_AS_PLW->player_number == 4) {
                        wk->cg_tc_state = ground_knockback_table[wk->move_type & 7];
                        wk->cg_cancel |= 8;
                        return;
                    }

                    wk->cg_tc_state = chain_normal_ground_table[wk->move_type & 7];
                    wk->cg_cancel |= 8;
                    return;
                }

                break;

            case 2:
                if (!(WK_AS_PLW->special_move_disabled_flag2 & DIP2_AIR_CHAIN_COMBO_DISABLED) && !hikusugi_check(wk)) {
                    if (WK_AS_PLW->player_number == 7) {
                        wk->cg_tc_state = air_knockback_table[wk->move_type & 7];
                        wk->cg_cancel |= 8;
                        return;
                    }

                    wk->cg_tc_state = chain_normal_air_table[wk->move_type & 7];
                    wk->cg_cancel |= 8;
                }

                break;
            }
        }
    }
}

/** @brief Checks xcopy filter and triggers SE request. */
static u16 check_xcopy_filter_se_req(State* wk) {
    u16 voif;

    if ((voif = wk->cg_se) < 0x160) {
        return voif;
    }

    if (wk->work_id != 1) {
        if (LO_2_BYTES(WK_AS_PLW->spmv_ng_flag) != 1) {
            return voif;
        }

        if ((u16)HI_2_BYTES(WK_AS_PLW->spmv_ng_flag) > 1) {
            return voif;
        }

        if (g_state.plw[HI_2_BYTES(WK_AS_PLW->spmv_ng_flag)].metamorphose == 0) {
            return voif;
        }

        return voif + 0x600;
    }

    if (WK_AS_PLW->metamorphose == 0) {
        return voif;
    }

    return voif + 0x600;
}

/** @brief Processes pattern data for auxiliary CG objects. */
static void check_cgd_patdat2(State* wk) {
    ST st;
    u16* seadrs;

    switch (wk->char_graphic_data_type) {
    case 6:
        if (wk->cg_status & 0x80) {
            wk->pat_status = wk->cg_status & 0x7F;
        }

        /* fallthrough */

    case 4:
        wk->cg_tc_state = wk->anim_hurtbox_index & 0x1FFF;
        st.w.h = wk->anim_hitbox_index;
        st.w.l = wk->anim_hurtbox_index;
        wk->anim_hitbox_index >>= 6;
        st.l *= 8;
        wk->anim_hurtbox_index = st.w.h & 0x1FF;

        if (wk->anim_hitbox_index) {
            set_new_attnum(wk);
        }

        break;
    }

    wk->cg_jphos = jphos_table[wk->anim_overlap_col_index & 0xF];
    wk->anim_overlap_col_index >>= 4;
    wk->cg_flip = wk->cg_se & 3;
    wk->cg_prio = (wk->cg_se & 0xF) >> 2;
    wk->cg_se >>= 4;

    if (wk->cg_se & 0x800) {
        seadrs = (u16*)&wk->se_random_table[wk->se_random_table[wk->cg_se & 0x7FF] / 4];
        wk->cg_se = seadrs[random_16()];
    }

    if (wk->work_id == 1) {
        if (wk->cg_rival == 0) {
            wk->curr_rca = NULL;
        } else {
            wk->curr_rca = wk->rival_catch_tbl + (wk->cg_rival + catch_table_offset(((PLW*)wk)->throw_target_id));
        }
    }

    wk->graphic_overlap_index = wk->olc_ix_table[wk->anim_overlap_col_index];
    wk->cg_ja = wk->hit_ix_table[wk->anim_hurtbox_index];

    Set_Collision_Boxes(wk);

    if (wk->cg_type != 0xFF && wk->cg_type & 0x80) {
        wk->cg_wca_ix = wk->cg_type & 0x7F;
        wk->cg_type = 0;
    }
}

/** @brief Sets a new attack number from the attack data tables. */
void set_new_attnum(State* wk) {
    s16 aag_sw;

    wk->renew_attack = wk->anim_hitbox_index;

    g_state.att_req += 1;
    g_state.att_req &= 0x7FFF;

    if (g_state.att_req == 0) {
        g_state.att_req += 1;
    }

    aag_sw = 0;

    if (wk->anim_hitbox_index < 0) {
        wk->anim_hitbox_index = -wk->anim_hitbox_index;
        wk->attack_num = g_state.att_req;
        wk->att_hit_ok = 1;
        aag_sw = 1;
        wk->frame_link_hit_flag = 0;

        if (wk->work_id == 1) {
            WK_AS_PLW->caution_flag = 1;
            WK_AS_PLW->total_att_hit_ok += 1;
        }

        grade_add_att_renew((State_Other*)wk);
    }

    wk->att = *(wk->att_ix_table + wk->anim_hitbox_index);
    wk->zu_flag = wk->att.level & 0x80;
    wk->jump_att_flag = wk->att.level & 0x40;
    wk->at_attribute = (wk->att.level >> 4) & 3;
    wk->no_death_attack = wk->att.level & 8;
    wk->att.level &= 7;
    wk->chip_damage_power = kezuri_pow_table[(wk->att.guard >> 6) & 3];
    wk->att.guard &= 0x3F;
    wk->attack_invuln = (wk->att.dir >> 4) & 7;
    wk->att.dir &= 0xF;
    wk->add_arts_point = (wk->att.stun_effect >> 4) & 0xF;
    wk->att.stun_effect &= 0xF;
    wk->vs_id = (wk->att.ng_type >> 4) & 0xF;
    wk->att.ng_type &= 0xF;
    wk->dir_atthit = cal_attdir(wk);

    if (aag_sw) {
        add_sp_arts_gauge_init((PLW*)wk);
    }

    if ((wk->work_id == 1) && !(WK_AS_PLW->spmv_ng_flag & DIP_EXTREME_CHIP_DAMAGE_DISABLED)) {
        setup_metamor_kezuri(wk);
    }
}

/** @brief Configures chip-damage (kezuri) for metamorphosis mode. */
static void setup_metamor_kezuri(State* wk) {
    if (wk->chip_damage_power == 0) {
        wk->chip_damage_power = kezuri_pow_table[4];
    }
}

/** @brief Sets up the judge (collision) area for the work object. */
void Set_Collision_Boxes(State* wk) {
    wk->body_hurtbox = wk->body_adrs + wk->cg_ja.body_hurtbox_index;
    wk->catch_box = wk->catch_adrs + wk->cg_ja.catch_box_index;
    wk->caught_box = wk->caught_adrs + wk->cg_ja.caught_box_index;
    wk->attack_hitbox = wk->attack_adrs + wk->cg_ja.attack_box_index;
    wk->pushbox = wk->adjust_adrs + wk->cg_ja.pushbox_index;
    wk->hand_hurtbox = wk->hand_adrs + (wk->cg_ja.behind_hurtbox_index + wk->cg_ja.hand_hurtbox_index);
}

/** @brief Captures current character data for after-image (zanzou). */
void get_char_data_zanzou(State* wk) {
    if (wk->anim_hitbox_index) {
        set_new_attnum(wk);
    }

    wk->cg_ja = wk->hit_ix_table[wk->anim_hurtbox_index];
    Set_Collision_Boxes(wk);
}

const s16 jphos_table[16] = { 0x0000, 0xFFF0, 0xFFF4, 0xFFF8, 0xFFFC, 0x0004, 0x0008, 0x000C,
                              0x0010, 0x0014, 0x0018, 0x001C, 0x0020, 0x0024, 0x0028, 0x002C };

const s16 kezuri_pow_table[5] = { 0, 4, 8, 16, 24 };

static s32 comm_dummy(State*, CommandState*);
static s32 comm_roa(State*, CommandState*);
static s32 comm_end(State*, CommandState*);
static s32 comm_jmp(State*, CommandState*);
static s32 comm_jpss(State*, CommandState*);
static s32 comm_jsr(State*, CommandState*);
static s32 comm_ret(State*, CommandState*);
static s32 comm_sps(State*, CommandState*);
static s32 comm_setr(State*, CommandState*);
static s32 comm_addr(State*, CommandState*);
static s32 comm_if_l(State*, CommandState*);
static s32 comm_djmp(State*, CommandState*);
static s32 comm_for(State*, CommandState*);
static s32 comm_nex(State*, CommandState*);
static s32 comm_for2(State*, CommandState*);
static s32 comm_nex2(State*, CommandState*);
static s32 comm_rja(State*, CommandState*);
static s32 comm_uja(State*, CommandState*);
static s32 comm_rja2(State*, CommandState*);
static s32 comm_uja2(State*, CommandState*);
static s32 comm_rja3(State*, CommandState*);
static s32 comm_uja3(State*, CommandState*);
static s32 comm_rja4(State*, CommandState*);
static s32 comm_uja4(State*, CommandState*);
static s32 comm_rja5(State*, CommandState*);
static s32 comm_uja5(State*, CommandState*);
static s32 comm_rja6(State*, CommandState*);
static s32 comm_uja6(State*, CommandState*);
static s32 comm_rja7(State*, CommandState*);
static s32 comm_uja7(State*, CommandState*);
static s32 comm_rmja(State*, CommandState*);
static s32 comm_umja(State*, CommandState*);
static s32 comm_mdat(State*, CommandState*);
static s32 comm_ydat(State*, CommandState*);
static s32 comm_mpos(State*, CommandState*);
static s32 comm_cafr(State*, CommandState*);
static s32 comm_care(State*, CommandState*);
static s32 comm_psxy(State*, CommandState*);
static s32 comm_ps_x(State*, CommandState*);
static s32 comm_ps_y(State*, CommandState*);
static s32 comm_paxy(State*, CommandState*);
static s32 comm_pa_x(State*, CommandState*);
static s32 comm_pa_y(State*, CommandState*);
static s32 comm_exec(State*, CommandState*);
static s32 comm_rngc(State*, CommandState*);
static s32 comm_mxyt(State*, CommandState*);
static s32 comm_pjmp(State*, CommandState*);
static s32 comm_hjmp(State*, CommandState*);
static s32 comm_hclr(State*, CommandState*);
static s32 comm_ixfw(State*, CommandState*);
static s32 comm_ixbw(State*, CommandState*);
static s32 comm_quax(State*, CommandState*);
static s32 comm_quay(State*, CommandState*);
static s32 comm_if_s(State*, CommandState*);
static s32 comm_rapp(State*, CommandState*);
static s32 comm_rapk(State*, CommandState*);
static s32 comm_gets(State*, CommandState*);
static s32 comm_s123(State*, CommandState*);
static s32 comm_s456(State*, CommandState*);
static s32 comm_a123(State*, CommandState*);
static s32 comm_a456(State*, CommandState*);
static s32 comm_stop(PLW*, CommandState*);
static s32 comm_smhf(State*, CommandState*);
static s32 comm_ngme(State*, CommandState*);
static s32 comm_ngem(State*, CommandState*);
static s32 comm_iflb(State*, CommandState*);
static s32 comm_asxy(State*, CommandState*);
static s32 comm_schx(State*, CommandState*);
static s32 comm_schy(State*, CommandState*);
static s32 comm_back(State*, CommandState*);
static s32 comm_mvix(State*, CommandState*);
static s32 comm_sajp(State*, CommandState*);
static s32 comm_ccch(State*, CommandState*);
static s32 comm_wset(State*, CommandState*);
static s32 comm_wswk(State*, CommandState*);
static s32 comm_wadd(State*, CommandState*);
static s32 comm_wceq(State*, CommandState*);
static s32 comm_wcne(State*, CommandState*);
static s32 comm_wcgt(State*, CommandState*);
static s32 comm_wclt(State*, CommandState*);
static s32 comm_wadd2(State*, CommandState*);
static s32 comm_wceq2(State*, CommandState*);
static s32 comm_wcne2(State*, CommandState*);
static s32 comm_wcgt2(State*, CommandState*);
static s32 comm_wclt2(State*, CommandState*);
static s32 comm_rapp2(State*, CommandState*);
static s32 comm_rapk2(State*, CommandState*);
static s32 comm_iflg(State*, CommandState*);
static s32 comm_mpcy(State*, CommandState*);
static s32 comm_epcy(State*, CommandState*);
static s32 comm_imgs(PLW*, CommandState*);
static s32 comm_imgc(PLW*, CommandState*);
static s32 comm_rvxy(State*, CommandState*);
static s32 comm_rv_x(State*, CommandState*);
static s32 comm_rv_y(State*, CommandState*);
static s32 comm_ccfl(PLW*, CommandState*);
static s32 comm_myhp(State*, CommandState*);
static s32 comm_emhp(State*, CommandState*);
static s32 comm_exbgs(State*, CommandState*);
static s32 comm_exbgc(State*, CommandState*);
static s32 comm_atmf(PLW*, CommandState*);
static s32 comm_chkwf(PLW*, CommandState*);
static s32 comm_retmj(PLW*, CommandState*);
static s32 comm_sstx(State*, CommandState*);
static s32 comm_ssty(State*, CommandState*);
static s32 comm_ngda(State*, CommandState*);
static s32 comm_flip(State*, CommandState*);
static s32 comm_kage(State*, CommandState*);
static s32 comm_dspf(State*, CommandState*);
static s32 comm_ifrlf(State*, CommandState*);
static s32 comm_srlf(State*, CommandState*);
static s32 comm_bgrlf(State*, CommandState*);
static s32 comm_scmd(PLW*, CommandState*);
static s32 comm_rljmp(State*, CommandState*);
static s32 comm_ifs2(State*, CommandState*);
static s32 comm_abbak(State*, CommandState*);
static s32 comm_sse(State*, CommandState*);
static s32 comm_s_chg(State*, CommandState*);
static s32 comm_schg2(State*, CommandState*);
static s32 comm_rhsja(PLW*, CommandState*);
static s32 comm_uhsja(PLW*, CommandState*);
static s32 comm_ifcom(State*, CommandState*);
static s32 comm_axjmp(State*, CommandState*);
static s32 comm_ayjmp(State*, CommandState*);
static s32 comm_ifs3(State*, CommandState*);

s32 (*const decode_chcmd[125])() = {
    comm_dummy, comm_roa,   comm_end,   comm_jmp,   comm_jpss,  comm_jsr,   comm_ret,   comm_sps,   comm_setr,
    comm_addr,  comm_if_l,  comm_djmp,  comm_for,   comm_nex,   comm_for2,  comm_nex2,  comm_rja,   comm_uja,
    comm_rja2,  comm_uja2,  comm_rja3,  comm_uja3,  comm_rja4,  comm_uja4,  comm_rja5,  comm_uja5,  comm_rja6,
    comm_uja6,  comm_rja7,  comm_uja7,  comm_rmja,  comm_umja,  comm_mdat,  comm_ydat,  comm_mpos,  comm_cafr,
    comm_care,  comm_psxy,  comm_ps_x,  comm_ps_y,  comm_paxy,  comm_pa_x,  comm_pa_y,  comm_exec,  comm_rngc,
    comm_mxyt,  comm_pjmp,  comm_hjmp,  comm_hclr,  comm_ixfw,  comm_ixbw,  comm_quax,  comm_quay,  comm_if_s,
    comm_rapp,  comm_rapk,  comm_gets,  comm_s123,  comm_s456,  comm_a123,  comm_a456,  comm_stop,  comm_smhf,
    comm_ngme,  comm_ngem,  comm_iflb,  comm_asxy,  comm_schx,  comm_schy,  comm_back,  comm_mvix,  comm_sajp,
    comm_ccch,  comm_wset,  comm_wswk,  comm_wadd,  comm_wceq,  comm_wcne,  comm_wcgt,  comm_wclt,  comm_wadd2,
    comm_wceq2, comm_wcne2, comm_wcgt2, comm_wclt2, comm_rapp2, comm_rapk2, comm_iflg,  comm_mpcy,  comm_epcy,
    comm_imgs,  comm_imgc,  comm_rvxy,  comm_rv_x,  comm_rv_y,  comm_ccfl,  comm_myhp,  comm_emhp,  comm_exbgs,
    comm_exbgc, comm_atmf,  comm_chkwf, comm_retmj, comm_sstx,  comm_ssty,  comm_ngda,  comm_flip,  comm_kage,
    comm_dspf,  comm_ifrlf, comm_srlf,  comm_bgrlf, comm_scmd,  comm_rljmp, comm_ifs2,  comm_abbak, comm_sse,
    comm_s_chg, comm_schg2, comm_rhsja, comm_uhsja, comm_ifcom, comm_axjmp, comm_ayjmp, comm_ifs3
};

s32 (*const decode_if_lever[16])() = { comm_dummy, comm_ret,  comm_uja,   comm_uja2, comm_uja3, comm_uja4,
                                       comm_uja5,  comm_uja6, comm_uja7,  comm_umja, comm_back, comm_nex,
                                       comm_nex2,  comm_wca,  comm_retmj, comm_abbak };

const u16 acatkoa_table[65] = { 4,   4,   8,   8,   8,   8,   8,   8,   16,  16,  16,  16,  16,  16,  16,  16,  32,
                                32,  32,  32,  32,  32,  32,  32,  64,  64,  64,  64,  64,  64,  64,  64,  128, 128,
                                128, 128, 128, 128, 128, 128, 256, 256, 256, 256, 256, 256, 256, 256, 128, 128, 128,
                                128, 128, 128, 128, 128, 256, 256, 256, 256, 256, 256, 256, 256, 2048 };
