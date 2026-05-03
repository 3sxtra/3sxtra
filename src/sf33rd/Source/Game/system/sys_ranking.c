/**
 * @file sys_ranking.c
 * @brief Ranking insertion and opponent candidate selection.
 *
 * Handles inserting player results into the top-5 ranking tables
 * (by score, wins, CPU grade, grade), building the ordered list
 * of CPU opponent candidates for arcade mode, and tracking
 * defeated opponents.
 *
 * Split from sys_sub.c for organizational clarity.
 */

#include "sf33rd/Source/Game/system/sys_ranking.h"
#include "game_state.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h" /* random_16 */
#include "common.h"
#include "main.h"
#include "sf33rd/Source/Game/com/com_data.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#define CANDIDATE_BUFF_SIZE 16
#define EM_CANDIDATE_SLOTS 8
#define RANKING_TOP_N 5

u8 Candidate_Buff[CANDIDATE_BUFF_SIZE];

// forward decls
static void Check_Partners_Rank(s16 dir_step, s16 PL_id);
static s32 Check_Sort_Score(s16 PL_id);
static s32 Check_Sort_Wins(s16 PL_id);
static s32 Check_Sort_CPU_Grade(s16 PL_id);
static s32 Check_Sort_Grade(s16 PL_id);
static s32 Check_CPU_Grade_Score(s16 PL_id, s16 i);
static s32 Check_Grade_Score(s16 PL_id, s16 i);
static void Setup_Candidate_Buff(s16 PL_id);
static s16 Check_EM_Buff(s16 ix, s16 ok_urien);
static s32 Check_EM_Sub(s16 ix, s16 ok_urien, s16 Rnd);

/** @brief Insert the player's results into ranking tables (score, wins, CPU grade, grade); returns 1 if ranked. */
s32 Check_Ranking(s16 PL_id) {
    Present_Data[PL_id].name[0] = 12;
    Present_Data[PL_id].name[1] = 10;
    Present_Data[PL_id].name[2] = 25;
    Present_Data[PL_id].player = g_state.Stock_My_char[PL_id];
    Present_Data[PL_id].player_color = g_state.Stock_Player_Color[PL_id];
    Present_Data[PL_id].score = g_state.Continue_Coin[PL_id] + g_state.Score[PL_id][0];
    Present_Data[PL_id].wins = g_state.Stock_Win_Record[PL_id];
    Present_Data[PL_id].cpu_grade = g_state.judge_final[PL_id]->vs_cpu_grade[12];
    Present_Data[PL_id].grade = g_state.Best_Grade[PL_id];

    if (g_state.Break_Com[PL_id][0]) {
        Present_Data[PL_id].all_clear = 1;
    } else {
        Present_Data[PL_id].all_clear = 0;
    }

    *Get_Ranking_Slot(PL_id, 0) = Check_Sort_Score(PL_id);

    if (*Get_Ranking_Slot(PL_id, 0) >= 0 && *Get_Ranking_Slot(PL_id ^ 1, 0) >= 0) {
        Check_Partners_Rank(0, PL_id);
    }

    *Get_Ranking_Slot(PL_id, 1) = Check_Sort_Wins(PL_id);

    if (*Get_Ranking_Slot(PL_id, 1) >= 0 && *Get_Ranking_Slot(PL_id ^ 1, 1) >= 0) {
        Check_Partners_Rank(1, PL_id);
    }

    *Get_Ranking_Slot(PL_id, 2) = Check_Sort_CPU_Grade(PL_id);

    if (*Get_Ranking_Slot(PL_id, 2)) {
        *Get_Ranking_Slot(PL_id, 2) = -1;
    } else {
        *Get_Ranking_Slot(PL_id ^ 1, 2) = -1;
    }

    *Get_Ranking_Slot(PL_id, 3) = Check_Sort_Grade(PL_id);

    if (*Get_Ranking_Slot(PL_id, 3)) {
        *Get_Ranking_Slot(PL_id, 3) = -1;
    } else {
        *Get_Ranking_Slot(PL_id ^ 1, 3) = -1;
    }

    if (*Get_Ranking_Slot(PL_id, 0) >= 0 || *Get_Ranking_Slot(PL_id, 1) >= 0 || *Get_Ranking_Slot(PL_id, 2) >= 0 ||
        *Get_Ranking_Slot(PL_id, 3) >= 0) {
        return 1;
    }

    return 0;
}

/** @brief Adjust the partner's ranking slot if it conflicts with the current player's slot. */
static void Check_Partners_Rank(s16 dir_step, s16 PL_id) {
    if (*Get_Ranking_Slot(PL_id, dir_step) > *Get_Ranking_Slot(PL_id ^ 1, dir_step)) {
        return;
    }

    (*Get_Ranking_Slot(PL_id ^ 1, dir_step))++;

    if (*Get_Ranking_Slot(PL_id ^ 1, dir_step) > 4) {
        *Get_Ranking_Slot(PL_id ^ 1, dir_step) = -1;
    }
}

/** @brief Insert the player's score into the top-5 ranking; returns rank position or -1 if not ranked. */
static s32 Check_Sort_Score(s16 PL_id) {
    s16 i;
    s16 j;

    for (i = 0; i < 5; i++) {
        if (Ranking_Data[i].score < Present_Data[PL_id].score) {
            for (j = 3; j >= i; j--) {
                Ranking_Data[j + 1] = Ranking_Data[j];
            }

            Ranking_Data[i] = Present_Data[PL_id];
            return i;
        }
    }

    return -1;
}

/** @brief Insert the player's win count into the top-5 ranking; returns rank position or -1. */
static s32 Check_Sort_Wins(s16 PL_id) {
    s16 i;
    s16 j;

    for (i = 0; i < 5; i++) {
        if (Ranking_Data[i + 5].wins < Present_Data[PL_id].wins) {
            for (j = 3; j >= i; j--) {
                Ranking_Data[j + 6] = Ranking_Data[j + 5];
            }

            Ranking_Data[i + 5] = Present_Data[PL_id];
            return i;
        }
    }

    return -1;
}

/** @brief Insert the player's CPU grade into the top-5 ranking; returns rank position or -1. */
static s32 Check_Sort_CPU_Grade(s16 PL_id) {
    s16 i;
    s16 j;

    for (i = 0; i < 5; i++) {
        if (!Check_CPU_Grade_Score(PL_id, i)) {
            continue;
        }

        for (j = 3; j >= i; j--) {
            Ranking_Data[j + 11] = Ranking_Data[j + 10];
        }

        Ranking_Data[i + 10] = Present_Data[PL_id];
        return i;
    }

    return -1;
}

/** @brief Insert the player's grade into the top-5 ranking; returns rank position or -1. */
static s32 Check_Sort_Grade(s16 PL_id) {
    s16 i;
    s16 j;

    for (i = 0; i < 5; i++) {
        if (!Check_Grade_Score(PL_id, i)) {
            continue;
        }

        for (j = 3; j >= i; j--) {
            Ranking_Data[j + 16] = Ranking_Data[j + 15];
        }

        Ranking_Data[i + 15] = Present_Data[PL_id];
        return i;
    }

    return -1;
}

/** @brief Compare player's CPU grade + score against ranking slot i; returns 1 if player ranks higher. */
static s32 Check_CPU_Grade_Score(s16 PL_id, s16 i) {
    if (Ranking_Data[i + 10].cpu_grade > Present_Data[PL_id].cpu_grade) {
        return 0;
    }

    if (Ranking_Data[i + 10].cpu_grade < Present_Data[PL_id].cpu_grade) {
        return 1;
    }

    if (Ranking_Data[i + 10].score >= Present_Data[PL_id].score) {
        return 0;
    }

    return 1;
}

/** @brief Compare player's grade + wins against ranking slot i; returns 1 if player ranks higher. */
static s32 Check_Grade_Score(s16 PL_id, s16 i) {
    if (Ranking_Data[i + 15].grade > Present_Data[PL_id].grade) {
        return 0;
    }

    if (Ranking_Data[i + 15].grade < Present_Data[PL_id].grade) {
        return 1;
    }

    if (Ranking_Data[i + 15].wins >= Present_Data[PL_id].wins) {
        return 0;
    }

    return 1;
}

/** @brief Build the ordered list of CPU opponent candidates for arcade mode progression. */
void Initialize_EM_Candidate(s16 PL_id) {
    s16 ix;
    s16 ok_urien = random_16();

    for (ix = 0; ix < CANDIDATE_BUFF_SIZE; ix++) {
        Candidate_Buff[ix] = 0xFF;
    }

    Setup_Candidate_Buff(PL_id);

    for (ix = 0; ix < EM_CANDIDATE_SLOTS; ix++) {
        g_state.EM_Candidate[PL_id][0][ix] = Check_EM_Buff(ix, ok_urien);
        g_state.EM_Candidate[PL_id][1][ix] = Check_EM_Buff(ix, ok_urien);
    }

    g_state.EM_Candidate[PL_id][0][8] = Middle_Class_Boss_Data[g_state.My_char[PL_id]];
    g_state.EM_Candidate[PL_id][1][8] = Middle_Class_Boss_Data[g_state.My_char[PL_id]];

    if (g_state.My_char[PL_id] != 0) {
        g_state.EM_Candidate[PL_id][0][9] = 0;
        g_state.EM_Candidate[PL_id][1][9] = 0;
    } else {
        g_state.EM_Candidate[PL_id][0][9] = 1;
        g_state.EM_Candidate[PL_id][1][9] = 1;
    }
}

/** @brief Populate Candidate_Buff with eligible opponent character IDs (excluding self, boss, defeated). */
static void Setup_Candidate_Buff(s16 PL_id) {
    s16 em;
    s16 ix;
    s16 s2;

    for (em = 0, s2 = ix = 1; ix <= 19; ix++) {
        if (g_state.My_char[PL_id] == 0 && ix == 1) {
            continue;
        }

        if (ix == g_state.My_char[PL_id]) {
            continue;
        }

        if (ix == 17) {
            continue;
        }

        if (ix == Middle_Class_Boss_Data[g_state.My_char[PL_id]]) {
            continue;
        }

        if (g_state.Break_Com[PL_id][ix]) {
            continue;
        }

        Candidate_Buff[em] = ix;
        em++;

        if (em >= CANDIDATE_BUFF_SIZE) {
            break;
        }
    }
}

/** @brief Select an opponent from Candidate_Buff for match slot ix, respecting stage-order restrictions. */
static s16 Check_EM_Buff(s16 ix, s16 ok_urien) {
    s16 em;
    s16 Rnd = random_16();
    s16 Next;

    if (Check_EM_Sub(ix, ok_urien, Rnd)) {
        em = Candidate_Buff[Rnd];
        Candidate_Buff[Rnd] = 0xFF;
        return em;
    }

    Next = random_16() & 1;

    if (Next == 0) {
        Next = -1;
    }

    while (1) {
        if (Check_EM_Sub(ix, ok_urien, Rnd)) {
            em = Candidate_Buff[Rnd];
            Candidate_Buff[Rnd] = 0xFF;
            return em;
        }

        Rnd += Next;

        if (Rnd < 0) {
            Rnd = 15;
        }

        if (Rnd > 15) {
            Rnd = 0;
        }
    }
}

/** @brief Validate whether the candidate at Rnd is eligible for match slot ix. */
static s32 Check_EM_Sub(s16 ix, s16 ok_urien, s16 Rnd) {
    s16 em;

    if (Candidate_Buff[Rnd] == 0xFF) {
        return 0;
    }

    em = Candidate_Buff[Rnd];

    switch (em) {
    case 2:
    case 11:
    case 6:
    case 8:
        if (ix < 4) {
            return 0;
        }

        return 1;

    case 14:
        if (ix < 6) {
            return 0;
        }

        return 1;

    case 13:
        if (ok_urien != 0 && ix < 4) {
            return 0;
        }

        return 1;

    default:
        return 1;
    }
}

/** @brief Re-randomize remaining opponent candidates if the player changed their character. */
void Check_Same_CPU(s16 PL_id) {
    s16 ix;
    s16 ok_urien;

    if (g_state.VS_Index[PL_id] >= 9) {
        return;
    }

    if (g_state.Last_My_char[PL_id] == g_state.My_char[PL_id]) {
        return;
    }

    ok_urien = random_16();

    for (ix = 0; ix < CANDIDATE_BUFF_SIZE; ix++) {
        Candidate_Buff[ix] = 0xFF;
    }

    Setup_Candidate_Buff(PL_id);

    for (ix = g_state.VS_Index[PL_id]; ix < EM_CANDIDATE_SLOTS; ix++) {
        g_state.EM_Candidate[PL_id][0][ix] = Check_EM_Buff(ix, ok_urien);
        g_state.EM_Candidate[PL_id][1][ix] = Check_EM_Buff(ix, ok_urien);
    }

    g_state.EM_Candidate[PL_id][0][8] = Middle_Class_Boss_Data[g_state.My_char[PL_id]];
    g_state.EM_Candidate[PL_id][1][8] = Middle_Class_Boss_Data[g_state.My_char[PL_id]];
}

/** @brief Clear the defeated-opponent tracking array for the given player. */
void Clear_Break_Com(s16 PL_id) {
    s16 x;

    for (x = 0; x <= 19; x++) {
        g_state.Break_Com[PL_id][x] = 0;
    }
}
