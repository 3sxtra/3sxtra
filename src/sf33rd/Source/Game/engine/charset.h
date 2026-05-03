#ifndef CHARSET_H
#define CHARSET_H

#include "structs.h"

extern const u16 acatkoa_table[];

// MARK: - Serialized

extern u16 att_req;

void setupCharTableData(State* wk, s32 clr, s32 info);
void char_move_cmd_hit_stop(PlayerEntity* wk);
void char_move(State* wk);
void check_cm_extended_code(State* wk);
void setup_comm_back(State* wk);
void setup_comm_abbak(State* wk);
void check_cgd_patdat(State* wk);
void Set_Collision_Boxes(State* wk);
void set_new_attnum(State* wk);
void set_char_move_init(State* wk, s16 kind_of_char, s16 index);
void set_char_move_init2(State* wk, s16 kind_of_char, s16 index, s16 ip, s16 scf);
void char_move_z(State* wk);
void char_move_wca(State* wk);
void char_move_wca_init(State* wk);
void char_move_cmja(State* wk);
void exset_char_move_init(State* wk, s16 kind_of_char, s16 index);
void char_move_cmms(State* wk);
void char_move_cmms2(State* wk);
s32 char_move_cmms3(PlayerEntity* wk);
void char_move_index(State* wk, s16 ix);
void char_move_cmj4(State* wk);
void get_char_data_zanzou(State* wk);

#endif
