#ifndef TRAINING_HUD_H
#define TRAINING_HUD_H

#include "structs.h"
#include "training_state.h"

void training_hud_init();
void training_hud_draw();
void training_hud_draw_stun(PLW* player, TrainingPlayerState* state);
void training_hud_draw_advantage(PLW* player, TrainingPlayerState* state);

#endif
