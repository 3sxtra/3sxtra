#ifndef ENTRY_H
#define ENTRY_H

#include "structs.h"
#include "types.h"

typedef enum EntryState {
    ENTRY_TITLE_BLINK = 0,
    ENTRY_WAIT_START = 1,
    ENTRY_MID_GAME_ENTRY = 2,
    ENTRY_PRE_FIGHT_BREAK = 3,
    ENTRY_MID_ROUND_BREAK = 4,
    ENTRY_UNUSED_5 = 5,
    ENTRY_POST_CONTINUE_BREAK = 6,
    ENTRY_POST_FIGHT_BREAK = 7,
    ENTRY_END_GAME_BREAK = 8,
    ENTRY_UNUSED_9 = 9,
    ENTRY_FINAL_ENDING = 10
} EntryState;

typedef enum EntrySubState { ENTRY_SUB_INIT = 0, ENTRY_SUB_ACTIVE = 1, ENTRY_SUB_FINISH = 2 } EntrySubState;

typedef enum EntryPlayerState {
    ENTRY_PL_INIT = 0,
    ENTRY_PL_CREDIT = 1,
    ENTRY_PL_NAMING = 2,
    ENTRY_PL_RANKING = 3,
    ENTRY_PL_LOSER = 5,
    ENTRY_PL_GAME_OVER = 8
} EntryPlayerState;

extern const u8 Coin_Message_Data[7][2];

void Entry_Task(struct _TASK*);
s32 Ck_Break_Into(u16 Sw_0, u16 Sw_1, s16 PL_id);

#endif // ENTRY_H
