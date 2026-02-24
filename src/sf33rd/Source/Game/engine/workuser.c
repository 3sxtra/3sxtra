#include "sf33rd/Source/Game/engine/workuser.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "structs.h"

// MARK: - Non-serializable

const_s16_arr Tech_Address[2];
void* Shell_Address[2];
void* Synchro_Address[2][2];

// MARK: - Unhandled

const u8* Free_Ptr[2];
u8* Lag_Ptr;
u16* Demo_Ptr[2];

/** @brief Returns a pointer to the ranking slot for a given player and slot index. */
s8* Get_Ranking_Slot(s32 playerIdx, s32 slotIdx) {
    if (slotIdx == 0) {
        return &Rank_In[playerIdx][0];
    } else if (slotIdx == 5) {
        if (playerIdx == 0)
            return &Rank_In[1][1];
        if (playerIdx == 1)
            return &Request_Disp_Rank[0][1];
    } else if (slotIdx == 10) {
        if (playerIdx == 0)
            return &Request_Disp_Rank[0][2];
        if (playerIdx == 1)
            return &Request_Disp_Rank[1][2];
    } else if (slotIdx == 15) {
        if (playerIdx == 0)
            return &Request_Disp_Rank[1][3];
    }

    if (slotIdx < 4) {
        return &Rank_In[playerIdx][slotIdx];
    }

    return NULL;
}
