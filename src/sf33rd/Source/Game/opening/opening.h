/**
 * @file opening.h
 * @brief Public API for the opening cinematic sequence and title screen.
 *
 * Part of the opening module.
 */

#ifndef OPENING_H
#define OPENING_H

#include "structs.h"
#include "types.h"

/* === Named Constants (shared across opening module files) === */
#define OPENING_DEMO_PHASE_COUNT 3 /**< Phases in oh_opening_demo() dispatch */
#define OPENING_SCENE_COUNT 19     /**< Scene handlers in opening_move_jp[] */
#define OP_BG0_DISPATCH_COUNT 94   /**< Entries in op_bg0_move_jp[] */
#define SOUND_TRG_TABLE_SIZE 257   /**< Entries in sound_time_tbl[] / sound_trg_tbl[] */
#define OP_CHANGE_SOUND_COUNT 18   /**< Entries in op_change_sound_tbl[] */
#define OP_QUAKE_Y_COUNT 16        /**< Entries in op_quake_y_tbl0[] */
#define OPTSR_TABLE_SIZE 59        /**< Entries in optsr_tbl[] */
#define TITLE_TYPE_COUNT 2         /**< Entries in title[] */
#define OP_COLOR_FADE_STAGES 6     /**< Entries in ot_bg0_0004_tbl[] / ot_bg0_0015_tbl[] */
#define OP_QUAKE_STEP_COUNT 16     /**< Entries in op_bg0_0005_tbl[] */

/* === Shared state (defined in opening.c, used by scenes/bg files) === */
extern s16 op_obj_disp;
extern s8 op_scrn_end;
extern s16 title_tex_flag;
extern s16 op_timer0;
extern OP_W op_w;
extern s16 music_scene;
extern s16 music_time;
extern s16 op_plmove_timer;
extern OPBW* opw_ptr;
extern s16 op_end_flag;
extern s16 op_demo_index;
extern s16 op_sound_status;
extern MVXY op_bg_mvxy[3];
extern const s16 op_change_sound_tbl[OP_CHANGE_SOUND_COUNT];
extern const s16 op_quake_y_tbl0[OP_QUAKE_Y_COUNT];

void TITLE_Init();
s16 TITLE_Move(u16 type);
s16 opening_demo();
void OPBG_Init();
s16 OPBG_Move(s32 /* unused */);
void opening_init();
void sound_trg_init();
void sound_trg_move();
void OPBG_Trans();
void op_work_clear();
s16 oh_opening_demo();
void Bg_Family_Set_op();
void opening_init2();
void opening_move();
void opening_title();
void opning_init_00000();
void opning_init_01000();
void opning_init_02000();
void op_bg_move(s16 r_index);
void op_bg0_move(s16 r_index);
void op_bg1_move(s16 r_index);
void op_bg2_move(s16 r_index);
void op_scrn_pos_set2(s16 bg_no);
void oh_bg_blk_w(OPBW* opbw, s32 blk_no, s16 mapx, s16 mapy, s32 trans);
void oh_bg_blk_wh(OPBW* opbw, s32 blk_no, s16 mapx, s16 mapy, s32 trans);
void oh_bg_blk_wv(OPBW* opbw, s32 blk_no, s16 mapx, s16 mapy, s32 trans);
void oh_bg_blk_whv(OPBW* opbw, s32 blk_no, s16 mapx, s16 mapy, s32 trans);

#endif // OPENING_H
