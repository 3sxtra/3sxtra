/**
 * @file animation_lose_player.h
 * @brief Public API for losing-character post-round animations.
 *
 * Part of the animation module.
 */

#ifndef _ANIMATION_LOSE_PLAYER_H_
#define _ANIMATION_LOSE_PLAYER_H_

#include "structs.h"
#include "types.h"

extern s16 lose_rno[3];
extern s16 lose_free[2];

void lose_player(PlayerEntity* wk);
void Lose_00000(PlayerEntity* wk);
void Lose_10000(PlayerEntity* wk);
void Lose_20000(PlayerEntity* wk);
void Lose_30000(PlayerEntity* wk);
void Normal_normal_Loser(PlayerEntity* wk);
void Judge_normal_loser(PlayerEntity* wk);
void meta_lose_pause(PlayerEntity* wk);

#endif
