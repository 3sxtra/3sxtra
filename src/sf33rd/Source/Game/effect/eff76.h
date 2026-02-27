#ifndef EFF76_H
#define EFF76_H

#include "structs.h"
#include "types.h"

void effect_76_move(WORK_Other* ewk);
s32 effect_76_init(s16 dir_old);
void Setup_Color_L1(WORK_Other* ewk);
s32 chkNameAkuma(s32 plnum, s32 rnum);
void spawn_effect_76(u8 id, u8 order, u8 timer);

#endif
