/**
 * @file rumble.h
 * @brief Public API for controller vibration (rumble) effects.
 *
 * Part of the io module.
 */

#ifndef RUMBLE_H
#define RUMBLE_H

#include "structs.h"
#include "types.h"

#define PULREQ_MAX 51
#define PULPARA_MAX 54

extern s16 rumble_request[2][2];
extern u8 rumble_scene;
extern RumbleWorkState rumble_work[2];
extern RumbleRequest pulreq[];
extern RumbleParams pulpara[];

void init_pulpul_work();
void pulpul_stop();
void pulpul_stop2(s16 ix);
void pp_vib_on(s16 id);
void pp_operator_check_flag(u8 fl);
void move_pulpul_work();
void pp_screen_quake(s16 ix);
void init_pulpul_work2(s16 ix);
void init_pulpul_round2(s16 ix);
void pulpul_request(s16 id, s16 ix);
void pulpul_req_copy(s16 id, RumbleRequest* adr);
void pulpul_request_again();
s32 check_vibration_unit(s32 port);
void move_pulpul(RumbleWorkState* wk);
s32 pulpul_pdVibMxStart(RumbleWorkState* wk, s32 arg1, s32 arg2, RumbleParams* param);
s32 vibration_param_transfer(s32 id, RumbleParams* prm);
void pp_pulpara_remake_at_init(State* wk);
void pp_pulpara_remake_at_init2(State* wk);
void pp_pulpara_remake_at_hit(State* wk);
void pp_pulpara_remake_at(State* wk);
void pp_pulpara_remake_dm_all(State* wk);
void pp_pulpara_guard(State* wk);
void pp_pulpara_hit(State* wk);
void pp_pulpara_blocking(State* wk);
void pp_pulpara_catch(State* wk);
void pp_pulpara_caught(State* wk);
void pp_pulpara_shungokusatsu(State* wk);

#endif
