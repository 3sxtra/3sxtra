/**
 * @file gd3rd.h
 * @brief Public API for AFS file reading and load-request queue management.
 *
 * Part of the io module.
 */

#ifndef GD3RD_H
#define GD3RD_H

#include "sf33rd/Source/Game/io/file_loader.h"
#include "sf33rd/Source/Game/io/fs_sys.h"
#include "structs.h"
#include "types.h"

extern s16 plt_req[2];
extern const u8 lpr_wrdata[3];
extern const u8 lpt_seldat[4];

void Init_Load_Request_Queue_1st();
void Request_LDREQ_Break();
u8 Check_LDREQ_Break();
void Push_LDREQ_Queue_Player(s16 id, s16 ix);
void Check_LDREQ_Queue();
s32 Check_LDREQ_Clear();
s32 Check_LDREQ_Queue_Player(s16 id);
void Push_LDREQ_Queue_Direct(s16 ix, s16 id);
void Push_LDREQ_Queue_Player(s16 id, s16 ix);
void Push_LDREQ_Queue_BG(s16 ix);
s32 Check_LDREQ_Queue_BG(s16 ix);
s32 Check_LDREQ_Queue_Direct(s16 ix);

#endif
