/**
 * @file animation_win_player.h
 * @brief Public API for winning-character post-round animations.
 *
 * Part of the animation module.
 */

#ifndef _ANIMATION_WIN_PLAYER_H_
#define _ANIMATION_WIN_PLAYER_H_

#include "structs.h"
#include "types.h"

extern s16 win_free[2];
extern s16 a_rno;
extern s16 win_rno[2];
extern s16 poison_flag[2];

void win_player(PlayerEntity* wk);

#endif
