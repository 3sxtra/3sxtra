#ifndef GAME_H
#define GAME_H

#include "structs.h"

void Game(struct _TASK* task_ptr);
void Game_CharSelect();
void Game_Fight();
void Before_Select_Sub();
void Game_Task(struct _TASK* task_ptr);
void Game_ResetMatchState();
void Next_Title_Sub();

#endif
