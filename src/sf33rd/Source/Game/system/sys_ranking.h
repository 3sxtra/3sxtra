/**
 * @file sys_ranking.h
 * @brief Ranking insertion and opponent candidate selection API.
 *
 * Split from sys_sub.h — ranking sort/insert, CPU opponent
 * candidate generation, and defeated-opponent tracking.
 */
#ifndef SYS_RANKING_H
#define SYS_RANKING_H

#include "types.h"

s32 Check_Ranking(s16 PL_id);
void Initialize_EM_Candidate(s16 PL_id);
void Check_Same_CPU(s16 PL_id);
void Clear_Break_Com(s16 PL_id);

#endif
